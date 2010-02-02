/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Additional set of macros for the JS RPC.

#ifndef DevToolsRPCJS_h
#define DevToolsRPCJS_h

// Do not remove this one although it is not used.
#include "BoundObject.h"
#include "DevToolsRPC.h"
#include "WebFrame.h"
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace WebKit {

///////////////////////////////////////////////////////
// JS RPC binds and stubs

#define TOOLS_RPC_JS_BIND_METHOD0(Method) \
    boundObj.addProtoFunction(#Method, OCLASS::js##Method);

#define TOOLS_RPC_JS_BIND_METHOD1(Method, T1) \
    boundObj.addProtoFunction(#Method, OCLASS::js##Method);

#define TOOLS_RPC_JS_BIND_METHOD2(Method, T1, T2) \
    boundObj.addProtoFunction(#Method, OCLASS::js##Method);

#define TOOLS_RPC_JS_BIND_METHOD3(Method, T1, T2, T3) \
    boundObj.addProtoFunction(#Method, OCLASS::js##Method);

#define TOOLS_RPC_JS_BIND_METHOD4(Method, T1, T2, T3, T4) \
    boundObj.addProtoFunction(#Method, OCLASS::js##Method);

#define TOOLS_RPC_JS_BIND_METHOD5(Method, T1, T2, T3, T4, T5) \
    boundObj.addProtoFunction(#Method, OCLASS::js##Method);

#define TOOLS_RPC_JS_STUB_METHOD0(Method) \
    static v8::Handle<v8::Value> js##Method(const v8::Arguments& args) { \
        sendRpcMessageFromJS(#Method, args, 0); \
        return v8::Undefined(); \
    }

#define TOOLS_RPC_JS_STUB_METHOD1(Method, T1) \
    static v8::Handle<v8::Value> js##Method(const v8::Arguments& args) { \
        sendRpcMessageFromJS(#Method, args, 1); \
        return v8::Undefined(); \
    }

#define TOOLS_RPC_JS_STUB_METHOD2(Method, T1, T2) \
    static v8::Handle<v8::Value> js##Method(const v8::Arguments& args) { \
        sendRpcMessageFromJS(#Method, args, 2); \
        return v8::Undefined(); \
    }

#define TOOLS_RPC_JS_STUB_METHOD3(Method, T1, T2, T3) \
    static v8::Handle<v8::Value> js##Method(const v8::Arguments& args) { \
        sendRpcMessageFromJS(#Method, args, 3); \
        return v8::Undefined(); \
    }

#define TOOLS_RPC_JS_STUB_METHOD4(Method, T1, T2, T3, T4) \
    static v8::Handle<v8::Value> js##Method(const v8::Arguments& args) { \
        sendRpcMessageFromJS(#Method, args, 4); \
        return v8::Undefined(); \
    }

#define TOOLS_RPC_JS_STUB_METHOD5(Method, T1, T2, T3, T4, T5) \
    static v8::Handle<v8::Value> js##Method(const v8::Arguments& args) { \
        sendRpcMessageFromJS(#Method, args, 5); \
        return v8::Undefined(); \
    }

///////////////////////////////////////////////////////
// JS RPC main obj macro

#define DEFINE_RPC_JS_BOUND_OBJ(Class, STRUCT, DClass, DELEGATE_STRUCT) \
class JS##Class##BoundObj : public Class##Stub { \
public: \
    JS##Class##BoundObj(Delegate* rpcDelegate, \
                        v8::Handle<v8::Context> context, \
                        const char* classname) \
        : Class##Stub(rpcDelegate) { \
      BoundObject boundObj(context, this, classname); \
      STRUCT( \
          TOOLS_RPC_JS_BIND_METHOD0, \
          TOOLS_RPC_JS_BIND_METHOD1, \
          TOOLS_RPC_JS_BIND_METHOD2, \
          TOOLS_RPC_JS_BIND_METHOD3, \
          TOOLS_RPC_JS_BIND_METHOD4, \
          TOOLS_RPC_JS_BIND_METHOD5) \
      boundObj.build(); \
    } \
    virtual ~JS##Class##BoundObj() { } \
    typedef JS##Class##BoundObj OCLASS; \
    STRUCT( \
        TOOLS_RPC_JS_STUB_METHOD0, \
        TOOLS_RPC_JS_STUB_METHOD1, \
        TOOLS_RPC_JS_STUB_METHOD2, \
        TOOLS_RPC_JS_STUB_METHOD3, \
        TOOLS_RPC_JS_STUB_METHOD4, \
        TOOLS_RPC_JS_STUB_METHOD5) \
private: \
    static void sendRpcMessageFromJS(const char* method, \
                                     const v8::Arguments& jsArguments, \
                                     size_t argsCount) \
    { \
        Vector<String> args(argsCount); \
        for (size_t i = 0; i < argsCount; i++) \
            args[i] = WebCore::toWebCoreStringWithNullCheck(jsArguments[i]); \
        void* selfPtr = v8::External::Cast(*jsArguments.Data())->Value(); \
        JS##Class##BoundObj* self = static_cast<JS##Class##BoundObj*>(selfPtr); \
        self->sendRpcMessage(#Class, method, args); \
    } \
};

} // namespace WebKit

#endif
