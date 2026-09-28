/// @file sero2-node.cpp
/// Node.js N-API wrapper for the rustlet-rt / sero2 C/C++ proxy library.

#include <napi.h>
#include <string>
#include <iostream>

#include "etl/delegate.h"

#include "BootLoaderIF.hpp"
#include "ProxyIF.hpp"
#include "MessageDispatcherPassthruIF.hpp"

#include "ApplicationMessages.hpp"

class Sero2Proxy : public Napi::ObjectWrap<Sero2Proxy> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    Sero2Proxy(const Napi::CallbackInfo& info);
    ~Sero2Proxy();

private:
    // Lifecycle
    Napi::Value Open(const Napi::CallbackInfo& info);
    Napi::Value Close(const Napi::CallbackInfo& info);
    Napi::Value IsAvailable(const Napi::CallbackInfo& info);
    Napi::Value GetRunLevel(const Napi::CallbackInfo& info);
    Napi::Value GetStatus(const Napi::CallbackInfo& info);

    // Messaging - Send
    Napi::Value SendLedCommand(const Napi::CallbackInfo& info);

    // Messaging - Receive
    Napi::Value OnAdcStatus(const Napi::CallbackInfo& info);
    Napi::Value ClearAdcStatus(const Napi::CallbackInfo& info);

    // Internal receive callback and cleanup
    void onAdcReceived(const AdcStatus& data);
    void performClose();

    std::string m_configFile;
    bool m_opened = false;
    Napi::ThreadSafeFunction m_tsfnAdcStatus;
};

Napi::Object Sero2Proxy::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Sero2Proxy", {
        InstanceMethod("open", &Sero2Proxy::Open),
        InstanceMethod("close", &Sero2Proxy::Close),
        InstanceMethod("isAvailable", &Sero2Proxy::IsAvailable),
        InstanceMethod("getRunLevel", &Sero2Proxy::GetRunLevel),
        InstanceMethod("getStatus", &Sero2Proxy::GetStatus),
        InstanceMethod("sendLedCommand", &Sero2Proxy::SendLedCommand),
        InstanceMethod("onAdcStatus", &Sero2Proxy::OnAdcStatus),
        InstanceMethod("clearAdcStatus", &Sero2Proxy::ClearAdcStatus),
    });

    exports.Set("Sero2Proxy", func);
    return exports;
}

Sero2Proxy::Sero2Proxy(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Sero2Proxy>(info) {
    if (info.Length() > 0 && info[0].IsString()) {
        m_configFile = info[0].As<Napi::String>().Utf8Value();
    }
}

Sero2Proxy::~Sero2Proxy() {
    performClose();
}

void Sero2Proxy::performClose() {
    if (m_opened) {
        MessageDispatcherPassthruRTIF<AdcStatus>::getInstance().clear();
        if (m_tsfnAdcStatus) {
            m_tsfnAdcStatus.Release();
            m_tsfnAdcStatus = nullptr;
        }
        stopBootLoader();
        m_opened = false;
    }
}

Napi::Value Sero2Proxy::Open(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (m_opened) {
        return Napi::Boolean::New(env, true);
    }

    std::string configFile = m_configFile;
    if (info.Length() > 0 && info[0].IsString()) {
        configFile = info[0].As<Napi::String>().Utf8Value();
    }

    const char* pConfigFile = configFile.empty() ? nullptr : configFile.c_str();

    int result = startBootLoader("sero2-node", pConfigFile);
    if (result != 0) {
        std::string errMsg = "Failed to start BootLoader: ";
        errMsg += getStatusBootLoader();
        Napi::Error::New(env, errMsg).ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    m_opened = true;
    return Napi::Boolean::New(env, true);
}

Napi::Value Sero2Proxy::Close(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    performClose();
    return env.Undefined();
}

Napi::Value Sero2Proxy::IsAvailable(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::Boolean::New(env, isAvailableProxy());
}

Napi::Value Sero2Proxy::GetRunLevel(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::Number::New(env, getRunLevelBootLoader());
}

Napi::Value Sero2Proxy::GetStatus(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, getStatusBootLoader());
}

Napi::Value Sero2Proxy::SendLedCommand(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (!m_opened) {
        Napi::Error::New(env, "Proxy is not open. Call open() first.").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    uint16_t sequence = 0;
    bool on = false;

    if (info.Length() >= 2) {
        sequence = static_cast<uint16_t>(info[0].As<Napi::Number>().Uint32Value());
        on = info[1].As<Napi::Boolean>().Value();
    } else if (info.Length() == 1) {
        on = info[0].As<Napi::Boolean>().Value();
    } else {
        Napi::TypeError::New(env, "Expected (sequence: number, on: boolean) or (on: boolean)").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    LedCommand cmd = { on };
    dispatchMessage(sequence, &cmd);

    return Napi::Boolean::New(env, true);
}

Napi::Value Sero2Proxy::OnAdcStatus(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "Expected callback function").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (m_tsfnAdcStatus) {
        MessageDispatcherPassthruRTIF<AdcStatus>::getInstance().clear();
        m_tsfnAdcStatus.Release();
        m_tsfnAdcStatus = nullptr;
    }

    m_tsfnAdcStatus = Napi::ThreadSafeFunction::New(
        env,
        info[0].As<Napi::Function>(),
        "AdcStatusCallback",
        0,
        1
    );

    MessageDispatcherPassthruRTIF<AdcStatus>::PassthruCallback actCallback(
        etl::delegate<void(const AdcStatus&)>::create<Sero2Proxy, &Sero2Proxy::onAdcReceived>(*this)
    );
    MessageDispatcherPassthruRTIF<AdcStatus>::getInstance().registerCallback(actCallback);

    return env.Undefined();
}

Napi::Value Sero2Proxy::ClearAdcStatus(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    MessageDispatcherPassthruRTIF<AdcStatus>::getInstance().clear();
    if (m_tsfnAdcStatus) {
        m_tsfnAdcStatus.Release();
        m_tsfnAdcStatus = nullptr;
    }

    return env.Undefined();
}

void Sero2Proxy::onAdcReceived(const AdcStatus& data) {
    if (!m_tsfnAdcStatus) {
        return;
    }

    AdcStatus* copy = new AdcStatus(data);
    auto callback = [](Napi::Env env, Napi::Function jsCallback, AdcStatus* pData) {
        if (env != nullptr && jsCallback != nullptr && pData != nullptr) {
            Napi::Object obj = Napi::Object::New(env);
            obj.Set("max", Napi::Number::New(env, pData->m_max));
            obj.Set("min", Napi::Number::New(env, pData->m_min));
            obj.Set("value", Napi::Number::New(env, pData->m_value));
            jsCallback.Call({ obj });
        }
        delete pData;
    };

    napi_status status = m_tsfnAdcStatus.NonBlockingCall(copy, callback);
    if (status != napi_ok) {
        delete copy;
    }
}

static Napi::String Version(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, "0.1.0");
}

static Napi::Object ModuleInit(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "version"),
                Napi::Function::New(env, Version));
    Sero2Proxy::Init(env, exports);
    return exports;
}

NODE_API_MODULE(sero2_node, ModuleInit)
