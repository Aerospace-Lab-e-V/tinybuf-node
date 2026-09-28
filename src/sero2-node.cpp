/// @file sero2-node.cpp
/// Node.js N-API wrapper for the sero2 C++ protocol library.

#include <napi.h>

// ══════════════════════════════════════════════════════════════════
//  TODO: Connect sero2 C++ implementation here
// ══════════════════════════════════════════════════════════════════
// #include "sero2.hpp"

static Napi::String Version(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, "0.1.0");
}

// ══════════════════════════════════════════════════════════════════
//  Module registration
// ══════════════════════════════════════════════════════════════════

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    // TODO: Register your Sero2 N-API classes / functions here
    exports.Set(Napi::String::New(env, "version"),
                Napi::Function::New(env, Version));
    return exports;
}

NODE_API_MODULE(sero2_node, Init)
