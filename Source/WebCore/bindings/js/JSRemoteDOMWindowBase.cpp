/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSRemoteDOMWindowBase.h"

#include "JSRemoteDOMWindow.h"
#include "JSWindowProxy.h"

using namespace JSC;

namespace WebCore {

const ClassInfo JSRemoteDOMWindowBase::s_info = { "Window", &JSDOMGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSRemoteDOMWindowBase) };

const GlobalObjectMethodTable JSRemoteDOMWindowBase::s_globalObjectMethodTable = {
    nullptr, // shellSupportsRichSourceInfo
    nullptr, // shouldInterruptScript
    &javaScriptRuntimeFlags,
    nullptr, // queueTaskToEventLoop
    nullptr, // shouldInterruptScriptBeforeTimeout
    nullptr, // moduleLoaderImportModule
    nullptr, // moduleLoaderResolve
    nullptr, // moduleLoaderFetch
    nullptr, // moduleLoaderCreateImportMetaProperties
    nullptr, // moduleLoaderEvaluate
    nullptr, // promiseRejectionTracker
    nullptr, // defaultLanguage
    nullptr, // compileStreaming
    nullptr, // instantiateStreaming
};

JSRemoteDOMWindowBase::JSRemoteDOMWindowBase(VM& vm, Structure* structure, RefPtr<RemoteDOMWindow>&& window, JSWindowProxy* proxy)
    : JSDOMGlobalObject(vm, structure, proxy->world(), &s_globalObjectMethodTable)
    , m_wrapped(WTFMove(window))
{
}

void JSRemoteDOMWindowBase::destroy(JSCell* cell)
{
    static_cast<JSRemoteDOMWindowBase*>(cell)->JSRemoteDOMWindowBase::~JSRemoteDOMWindowBase();
}

RuntimeFlags JSRemoteDOMWindowBase::javaScriptRuntimeFlags(const JSGlobalObject*)
{
    return RuntimeFlags { };
}

JSRemoteDOMWindow* toJSRemoteDOMWindow(JSC::VM& vm, JSValue value)
{
    if (!value.isObject())
        return nullptr;

    while (!value.isNull()) {
        JSObject* object = asObject(value);
        const ClassInfo* classInfo = object->classInfo(vm);
        if (classInfo == JSRemoteDOMWindow::info())
            return jsCast<JSRemoteDOMWindow*>(object);
        if (classInfo == JSWindowProxy::info())
            return jsDynamicCast<JSRemoteDOMWindow*>(vm, jsCast<JSWindowProxy*>(object)->window());
        value = object->getPrototypeDirect(vm);
    }
    return nullptr;
}

} // namespace WebCore
