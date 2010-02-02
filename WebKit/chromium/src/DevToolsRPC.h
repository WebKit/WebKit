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

// DevTools RPC subsystem is a simple string serialization-based rpc
// implementation. The client is responsible for defining the Rpc-enabled
// interface in terms of its macros:
//
// #define MYAPI_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3)
//   METHOD0(Method1)
//   METHOD1(Method3, int)
// (snippet above should be multiline macro, add trailing backslashes)
//
// DEFINE_RPC_CLASS(MyApi, MYAPI_STRUCT)
//
// The snippet above will generate three classes: MyApi, MyApiStub and
// MyApiDispatch.
//
// 1. For each method defined in the marco MyApi will have a
// pure virtual function generated, so that MyApi would look like:
//
// class MyApi {
// private:
//     MyApi() { }
//     ~MyApi() { }
//     virtual void method1() = 0;
//     virtual void method2(
//         int param1,
//         const String& param2,
//         const Value& param3) = 0;
//     virtual void method3(int param1) = 0;
// };
//
// 2. MyApiStub will implement MyApi interface and would serialize all calls
// into the string-based calls of the underlying transport:
//
// DevToolsRPC::Delegate* transport;
// myApi = new MyApiStub(transport);
// myApi->method1();
// myApi->method3(2);
//
// 3. MyApiDelegate is capable of dispatching the calls and convert them to the
// calls to the underlying MyApi methods:
//
// MyApi* realObject;
// MyApiDispatch::dispatch(realObject, rawStringCallGeneratedByStub);
//
// will make corresponding calls to the real object.

#ifndef DevToolsRPC_h
#define DevToolsRPC_h

#include "PlatformString.h"
#include "Vector.h"
#include "WebDevToolsMessageData.h"

#include <wtf/Noncopyable.h>

namespace WebCore {
class String;
}

using WebCore::String;
using WTF::Vector;

namespace WebKit {

///////////////////////////////////////////////////////
// RPC dispatch macro

template<typename T>
struct RpcTypeTrait {
    typedef T ApiType;
};

template<>
struct RpcTypeTrait<bool> {
    typedef bool ApiType;
    static bool parse(const WebCore::String& t)
    {
        return t == "true";
    }
    static WebCore::String toString(bool b)
    {
        return b ? "true" : "false";
    }
};

template<>
struct RpcTypeTrait<int> {
    typedef int ApiType;
    static int parse(const WebCore::String& t)
    {
        bool success;
        int i = t.toIntStrict(&success);
        ASSERT(success);
        return i;
    }
    static WebCore::String toString(int i)
    {
        return WebCore::String::number(i);
    }
};

template<>
struct RpcTypeTrait<String> {
    typedef const String& ApiType;
    static String parse(const WebCore::String& t)
    {
        return t;
    }
    static WebCore::String toString(const String& t)
    {
        return t;
    }
};

///////////////////////////////////////////////////////
// RPC Api method declarations

#define TOOLS_RPC_API_METHOD0(Method) \
    virtual void Method() = 0;

#define TOOLS_RPC_API_METHOD1(Method, T1) \
    virtual void Method(RpcTypeTrait<T1>::ApiType t1) = 0;

#define TOOLS_RPC_API_METHOD2(Method, T1, T2) \
    virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                        RpcTypeTrait<T2>::ApiType t2) = 0;

#define TOOLS_RPC_API_METHOD3(Method, T1, T2, T3) \
    virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                        RpcTypeTrait<T2>::ApiType t2, \
                        RpcTypeTrait<T3>::ApiType t3) = 0;

#define TOOLS_RPC_API_METHOD4(Method, T1, T2, T3, T4) \
    virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                        RpcTypeTrait<T2>::ApiType t2, \
                        RpcTypeTrait<T3>::ApiType t3, \
                        RpcTypeTrait<T4>::ApiType t4) = 0;

#define TOOLS_RPC_API_METHOD5(Method, T1, T2, T3, T4, T5) \
    virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                        RpcTypeTrait<T2>::ApiType t2, \
                        RpcTypeTrait<T3>::ApiType t3, \
                        RpcTypeTrait<T4>::ApiType t4, \
                        RpcTypeTrait<T5>::ApiType t5) = 0;

///////////////////////////////////////////////////////
// RPC stub method implementations

#define TOOLS_RPC_STUB_METHOD0(Method) \
    virtual void Method() { \
        Vector<String> args; \
        this->sendRpcMessage(m_className, #Method, args); \
    }

#define TOOLS_RPC_STUB_METHOD1(Method, T1) \
    virtual void Method(RpcTypeTrait<T1>::ApiType t1) { \
        Vector<String> args(1); \
        args[0] = RpcTypeTrait<T1>::toString(t1); \
        this->sendRpcMessage(m_className, #Method, args); \
    }

#define TOOLS_RPC_STUB_METHOD2(Method, T1, T2) \
    virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                        RpcTypeTrait<T2>::ApiType t2) { \
        Vector<String> args(2); \
        args[0] = RpcTypeTrait<T1>::toString(t1); \
        args[1] = RpcTypeTrait<T2>::toString(t2); \
        this->sendRpcMessage(m_className, #Method, args); \
    }

#define TOOLS_RPC_STUB_METHOD3(Method, T1, T2, T3) \
    virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                        RpcTypeTrait<T2>::ApiType t2, \
                        RpcTypeTrait<T3>::ApiType t3) { \
        Vector<String> args(3); \
        args[0] = RpcTypeTrait<T1>::toString(t1); \
        args[1] = RpcTypeTrait<T2>::toString(t2); \
        args[2] = RpcTypeTrait<T3>::toString(t3); \
        this->sendRpcMessage(m_className, #Method, args); \
    }

#define TOOLS_RPC_STUB_METHOD4(Method, T1, T2, T3, T4) \
    virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                        RpcTypeTrait<T2>::ApiType t2, \
                        RpcTypeTrait<T3>::ApiType t3, \
                        RpcTypeTrait<T4>::ApiType t4) { \
        Vector<String> args(4); \
        args[0] = RpcTypeTrait<T1>::toString(t1); \
        args[1] = RpcTypeTrait<T2>::toString(t2); \
        args[2] = RpcTypeTrait<T3>::toString(t3); \
        args[3] = RpcTypeTrait<T4>::toString(t4); \
        this->sendRpcMessage(m_className, #Method, args); \
    }

#define TOOLS_RPC_STUB_METHOD5(Method, T1, T2, T3, T4, T5) \
    virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                        RpcTypeTrait<T2>::ApiType t2, \
                        RpcTypeTrait<T3>::ApiType t3, \
                        RpcTypeTrait<T4>::ApiType t4, \
                        RpcTypeTrait<T5>::ApiType t5) { \
        Vector<String> args(5); \
        args[0] = RpcTypeTrait<T1>::toString(t1); \
        args[1] = RpcTypeTrait<T2>::toString(t2); \
        args[2] = RpcTypeTrait<T3>::toString(t3); \
        args[3] = RpcTypeTrait<T4>::toString(t4); \
        args[4] = RpcTypeTrait<T5>::toString(t5); \
        this->sendRpcMessage(m_className, #Method, args); \
    }

///////////////////////////////////////////////////////
// RPC dispatch method implementations

#define TOOLS_RPC_DISPATCH0(Method) \
if (methodName == #Method) { \
    delegate->Method(); \
    return true; \
}

#define TOOLS_RPC_DISPATCH1(Method, T1) \
if (methodName == #Method) { \
    delegate->Method(RpcTypeTrait<T1>::parse(args[0])); \
    return true; \
}

#define TOOLS_RPC_DISPATCH2(Method, T1, T2) \
if (methodName == #Method) { \
    delegate->Method( \
        RpcTypeTrait<T1>::parse(args[0]), \
        RpcTypeTrait<T2>::parse(args[1]) \
    ); \
    return true; \
}

#define TOOLS_RPC_DISPATCH3(Method, T1, T2, T3) \
if (methodName == #Method) { \
    delegate->Method( \
        RpcTypeTrait<T1>::parse(args[0]), \
        RpcTypeTrait<T2>::parse(args[1]), \
        RpcTypeTrait<T3>::parse(args[2]) \
    ); \
    return true; \
}

#define TOOLS_RPC_DISPATCH4(Method, T1, T2, T3, T4) \
if (methodName == #Method) { \
    delegate->Method( \
        RpcTypeTrait<T1>::parse(args[0]), \
        RpcTypeTrait<T2>::parse(args[1]), \
        RpcTypeTrait<T3>::parse(args[2]), \
        RpcTypeTrait<T4>::parse(args[3]) \
    ); \
    return true; \
}

#define TOOLS_RPC_DISPATCH5(Method, T1, T2, T3, T4, T5) \
if (methodName == #Method) { \
    delegate->Method( \
        RpcTypeTrait<T1>::parse(args[0]), \
        RpcTypeTrait<T2>::parse(args[1]), \
        RpcTypeTrait<T3>::parse(args[2]), \
        RpcTypeTrait<T4>::parse(args[3]), \
        RpcTypeTrait<T5>::parse(args[4]) \
    ); \
    return true; \
}

#define TOOLS_END_RPC_DISPATCH() \
}

// This macro defines three classes: Class with the Api, ClassStub that is
// serializing method calls and ClassDispatch that is capable of dispatching
// the serialized message into its delegate.
#define DEFINE_RPC_CLASS(Class, STRUCT) \
class Class : public Noncopyable {\
public: \
    Class() \
    { \
        m_className = #Class; \
    } \
    virtual ~Class() { } \
    \
    STRUCT( \
        TOOLS_RPC_API_METHOD0, \
        TOOLS_RPC_API_METHOD1, \
        TOOLS_RPC_API_METHOD2, \
        TOOLS_RPC_API_METHOD3, \
        TOOLS_RPC_API_METHOD4, \
        TOOLS_RPC_API_METHOD5) \
    WebCore::String m_className; \
}; \
\
class Class##Stub \
    : public Class \
    , public DevToolsRPC { \
public: \
    explicit Class##Stub(Delegate* delegate) : DevToolsRPC(delegate) { } \
    virtual ~Class##Stub() { } \
    typedef Class CLASS; \
    STRUCT( \
        TOOLS_RPC_STUB_METHOD0, \
        TOOLS_RPC_STUB_METHOD1, \
        TOOLS_RPC_STUB_METHOD2, \
        TOOLS_RPC_STUB_METHOD3, \
        TOOLS_RPC_STUB_METHOD4, \
        TOOLS_RPC_STUB_METHOD5) \
}; \
\
class Class##Dispatch : public Noncopyable { \
public: \
    Class##Dispatch() { } \
    virtual ~Class##Dispatch() { } \
    \
    static bool dispatch(Class* delegate, \
                         const WebKit::WebDevToolsMessageData& data) { \
        String className = data.className; \
        if (className != #Class) \
            return false; \
        String methodName = data.methodName; \
        Vector<String> args; \
        for (size_t i = 0; i < data.arguments.size(); i++) \
            args.append(data.arguments[i]); \
        typedef Class CLASS; \
        STRUCT( \
            TOOLS_RPC_DISPATCH0, \
            TOOLS_RPC_DISPATCH1, \
            TOOLS_RPC_DISPATCH2, \
            TOOLS_RPC_DISPATCH3, \
            TOOLS_RPC_DISPATCH4, \
            TOOLS_RPC_DISPATCH5) \
        return false; \
    } \
};

///////////////////////////////////////////////////////
// RPC base class
class DevToolsRPC {
public:
    class Delegate {
    public:
        Delegate() { }
        virtual ~Delegate() { }
        virtual void sendRpcMessage(const WebKit::WebDevToolsMessageData& data) = 0;
    };

    explicit DevToolsRPC(Delegate* delegate) : m_delegate(delegate) { }
    virtual ~DevToolsRPC() { };

protected:
    void sendRpcMessage(const String& className,
                        const String& methodName,
                        const Vector<String>& args) {
      WebKit::WebVector<WebKit::WebString> webArgs(args.size());
      for (size_t i = 0; i < args.size(); i++)
          webArgs[i] = args[i];
      WebKit::WebDevToolsMessageData data;
      data.className = className;
      data.methodName = methodName;
      data.arguments.swap(webArgs);
      this->m_delegate->sendRpcMessage(data);
    }

    Delegate* m_delegate;
};

} // namespace WebKit

#endif
