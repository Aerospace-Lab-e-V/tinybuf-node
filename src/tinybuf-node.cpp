/// @file tinybuf-node.cpp
/// Node.js N-API wrapper for the rustlet-rt / tinybuf C/C++ proxy library.

#include <napi.h>
#include <string>

#include "etl/delegate.h"
#include "etl/queue_spsc_atomic.h"

#include "ProxyIF.hpp"
#include "ApplicationMessages.hpp"

extern "C"
{
  extern const char*    getStatusBootLoader();
  extern int            startBootLoader(const char* p_nameApplication, const char* p_configFile);
  extern int            stopBootLoader();
  extern int            getRunLevelBootLoader();

  extern int            sendMessage_LedCommand(const LedCommand* p_message);
  extern void           registerCallback_AdcStatus(void (*p_callback)(const AdcStatus*));
  extern bool           isAvailableProxy();

  void                  listenAdcStatus(const AdcStatus* p_status);
}

class TinybufProxy;

static etl::delegate<void(const AdcStatus&)> s_actCallback;

void
listenAdcStatus
 (const AdcStatus* p_status)
{
  if ((nullptr != p_status) &&
      (true == s_actCallback.is_valid()))
  {
    s_actCallback(*p_status);
  }
}

class TinybufProxy : public Napi::ObjectWrap<TinybufProxy> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    TinybufProxy(const Napi::CallbackInfo& info);
    ~TinybufProxy();

private:
    static void CallJsAdcStatus(Napi::Env env, Napi::Function jsCallback, TinybufProxy* proxy, void* data);
    using AdcThreadSafeFunction = Napi::TypedThreadSafeFunction<TinybufProxy, void, CallJsAdcStatus>;

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
    AdcThreadSafeFunction m_tsfnAdcStatus;
    etl::queue_spsc_atomic<AdcStatus, 128> m_adcQueue;
};

Napi::Object TinybufProxy::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "TinybufProxy", {
        InstanceMethod("open", &TinybufProxy::Open),
        InstanceMethod("close", &TinybufProxy::Close),
        InstanceMethod("isAvailable", &TinybufProxy::IsAvailable),
        InstanceMethod("getRunLevel", &TinybufProxy::GetRunLevel),
        InstanceMethod("getStatus", &TinybufProxy::GetStatus),
        InstanceMethod("sendLedCommand", &TinybufProxy::SendLedCommand),
        InstanceMethod("onAdcStatus", &TinybufProxy::OnAdcStatus),
        InstanceMethod("clearAdcStatus", &TinybufProxy::ClearAdcStatus),
    });

    exports.Set("TinybufProxy", func);
    return exports;
}

TinybufProxy::TinybufProxy(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<TinybufProxy>(info) {
    if (info.Length() > 0 && info[0].IsString()) {
        m_configFile = info[0].As<Napi::String>().Utf8Value();
    }
}

TinybufProxy::~TinybufProxy() {
    performClose();
}

void TinybufProxy::performClose() {
    if (m_opened) {
        registerCallback_AdcStatus(nullptr);
        s_actCallback.clear();
        m_adcQueue.clear();
        if (m_tsfnAdcStatus) {
            m_tsfnAdcStatus.Release();
            m_tsfnAdcStatus = nullptr;
        }
        stopBootLoader();
        m_opened = false;
    }
}

Napi::Value TinybufProxy::Open(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (m_opened) {
        return Napi::Boolean::New(env, true);
    }

    std::string configFile = m_configFile;
    if (info.Length() > 0 && info[0].IsString()) {
        configFile = info[0].As<Napi::String>().Utf8Value();
    }

    const char* pConfigFile = configFile.empty() ? nullptr : configFile.c_str();

    int result = startBootLoader("tinybuf-node", pConfigFile);
    if (result != 0) {
        std::string errMsg = "Failed to start BootLoader: ";
        errMsg += getStatusBootLoader();
        Napi::Error::New(env, errMsg).ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    m_opened = true;
    return Napi::Boolean::New(env, true);
}

Napi::Value TinybufProxy::Close(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    performClose();
    return env.Undefined();
}

Napi::Value TinybufProxy::IsAvailable(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::Boolean::New(env, isAvailableProxy());
}

Napi::Value TinybufProxy::GetRunLevel(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::Number::New(env, getRunLevelBootLoader());
}

Napi::Value TinybufProxy::GetStatus(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, getStatusBootLoader());
}

Napi::Value TinybufProxy::SendLedCommand(const Napi::CallbackInfo& info) {
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
    if (info.Length() >= 2) {
        dispatchMessage(sequence, &cmd);
    } else {
        sendMessage_LedCommand(&cmd);
    }

    return Napi::Boolean::New(env, true);
}

Napi::Value TinybufProxy::OnAdcStatus(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "Expected callback function").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (m_tsfnAdcStatus) {
        registerCallback_AdcStatus(nullptr);
        s_actCallback.clear();
        m_tsfnAdcStatus.Release();
        m_tsfnAdcStatus = nullptr;
        m_adcQueue.clear();
    }

    m_tsfnAdcStatus = AdcThreadSafeFunction::New(
        env,
        info[0].As<Napi::Function>(),
        "AdcStatusCallback",
        0,
        1,
        this
    );

    s_actCallback = etl::delegate<void(const AdcStatus&)>::create<TinybufProxy, &TinybufProxy::onAdcReceived>(*this);
    registerCallback_AdcStatus(listenAdcStatus);

    return env.Undefined();
}

Napi::Value TinybufProxy::ClearAdcStatus(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    registerCallback_AdcStatus(nullptr);
    s_actCallback.clear();
    m_adcQueue.clear();
    if (m_tsfnAdcStatus) {
        m_tsfnAdcStatus.Release();
        m_tsfnAdcStatus = nullptr;
    }

    return env.Undefined();
}

void TinybufProxy::onAdcReceived(const AdcStatus& data) {
    if (m_tsfnAdcStatus) {
        if (m_adcQueue.push(data)) {
            m_tsfnAdcStatus.NonBlockingCall(nullptr);
        }
    }
}

void TinybufProxy::CallJsAdcStatus(Napi::Env env, Napi::Function jsCallback, TinybufProxy* proxy, void* /*data*/) {
    if (env != nullptr && jsCallback != nullptr && proxy != nullptr) {
        AdcStatus status;
        while (proxy->m_adcQueue.pop(status)) {
            Napi::Object obj = Napi::Object::New(env);
            obj.Set("max", Napi::Number::New(env, status.m_max));
            obj.Set("min", Napi::Number::New(env, status.m_min));
            obj.Set("value", Napi::Number::New(env, status.m_value));
            jsCallback.Call({ obj });
        }
    }
}

static Napi::String Version(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, "0.1.2");
}

static Napi::Object ModuleInit(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "version"),
                Napi::Function::New(env, Version));
    TinybufProxy::Init(env, exports);
    return exports;
}

NODE_API_MODULE(tinybuf_node, ModuleInit)
