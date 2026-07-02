/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ScriptExecutionContext.h"
#include <JavaScriptCore/PropertyName.h>
#include <JavaScriptCore/Weak.h>
#include <JavaScriptCore/WeakHandleOwner.h>
#include <wtf/CurrentThread.h>
#include <wtf/NakedPtr.h>
#include <wtf/TZoneMalloc.h>

namespace JSC {
class JSObject;
class JSValue;
template<size_t> class MarkedArgumentBufferWithSize;
using MarkedArgumentBuffer = MarkedArgumentBufferWithSize<8>;
}

namespace WebCore {

class JSDOMGlobalObject;

template<typename ImplementationClass> struct JSDOMCallbackConverterTraits;

// We have to clean up this data on the context thread because unprotecting a
// JSObject on the wrong thread without synchronization would corrupt the heap
// (and synchronization would be slow).

class WEBCORE_EXPORT JSCallbackData {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(JSCallbackData, WEBCORE_EXPORT);
public:
    enum class CallbackType { Function, Object, FunctionOrObject };

    JSCallbackData(JSC::JSObject* callback, JSDOMGlobalObject*, void* owner);
    ~JSCallbackData();

    JSDOMGlobalObject* globalObject();
    JSC::JSObject* callback();

    template<typename Visitor> void visitJSFunctionInGCThread(Visitor&);

    static JSC::JSValue invokeCallback(JSDOMGlobalObject&, JSC::JSObject* callback, JSC::JSValue thisValue, JSC::MarkedArgumentBuffer&, CallbackType, JSC::PropertyName functionName, NakedPtr<JSC::Exception>& returnedException);

    JSC::JSValue invokeCallback(JSC::JSValue thisValue, JSC::MarkedArgumentBuffer&, CallbackType, JSC::PropertyName functionName, NakedPtr<JSC::Exception>&);

private:
    JSC::Weak<JSDOMGlobalObject> m_globalObject;

    class WEBCORE_EXPORT WeakOwner : public JSC::WeakHandleOwner {
        bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* owner, JSC::AbstractSlotVisitor&, ASCIILiteral*) override;
    };
    WeakOwner m_weakOwner;
    JSC::Weak<JSC::JSObject> m_callback;

#if ASSERT_ENABLED
    const uint32_t m_threadID { currentThreadID() };
#endif
};

class DeleteCallbackDataTask : public ScriptExecutionContext::Task {
public:
    template <typename CallbackDataType>
    explicit DeleteCallbackDataTask(CallbackDataType* data)
        : ScriptExecutionContext::Task(ScriptExecutionContext::Task::CleanupTask, [data = std::unique_ptr<CallbackDataType>(data)] (ScriptExecutionContext&) {
        })
    {
    }
};

} // namespace WebCore
