/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "WebKitJSHandle.h"

#include "CommonVM.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "JSDOMGlobalObject.h"
#include "JSWindowProxy.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/MainThread.h>

namespace WebCore {

using HandleToGlobalMap = JSC::WeakGCMap<JSHandleIdentifier, JSDOMGlobalObject>;

static HandleToGlobalMap& handleToGlobalMap(JSC::VM& vm)
{
    return downcast<JSVMClientData>(vm.clientData)->jsHandleGlobalObjects();
}

// Handles are only ever created on the main thread, so the global object a handle names always
// belongs to the common VM.
static HandleToGlobalMap* handleToGlobalMapIfExists()
{
    ASSERT(isMainThread());
    auto* vm = commonVMOrNull();
    return vm ? &handleToGlobalMap(*vm) : nullptr;
}

static JSDOMGlobalObject* globalObjectForIdentifier(JSHandleIdentifier identifier)
{
    auto* map = handleToGlobalMapIfExists();
    return map ? map->get(identifier) : nullptr;
}

Ref<WebKitJSHandle> WebKitJSHandle::create(JSC::JSObject* object)
{
    return adoptRef(*new WebKitJSHandle(object));
}

WebKitJSHandle::~WebKitJSHandle()
{
    jsHandleDestroyed(m_identifier);
}

void WebKitJSHandle::jsHandleSentToAnotherProcess(JSHandleIdentifier identifier)
{
    if (auto* global = globalObjectForIdentifier(identifier))
        global->refJSHandle(identifier);
}

void WebKitJSHandle::jsHandleDestroyed(JSHandleIdentifier identifier)
{
    auto* map = handleToGlobalMapIfExists();
    if (!map)
        return;
    auto* global = map->get(identifier);
    if (!global || global->derefJSHandle(identifier))
        map->remove(identifier);
}

JSC::JSObject* WebKitJSHandle::objectForIdentifier(JSHandleIdentifier identifier)
{
    auto* global = globalObjectForIdentifier(identifier);
    if (!global)
        return nullptr;
    return global->jsHandle(identifier);
}

static Markable<FrameIdentifier> NODELETE windowFrameIdentifier(JSC::JSObject* object)
{
    if (auto* window = dynamicDowncast<WebCore::JSWindowProxy>(object)) {
        if (auto* frame = window->wrapped().frame())
            return frame->frameID();
    }
    return std::nullopt;
}

WebKitJSHandle::WebKitJSHandle(JSC::JSObject* object)
    : m_identifier(JSHandleIdentifier(WebProcessJSHandleIdentifier(reinterpret_cast<uintptr_t>(object)), Process::identifier()))
    , m_windowFrameIdentifier(WebCore::windowFrameIdentifier(object))
{
    auto* global = dynamicDowncast<JSDOMGlobalObject>(object->realmMayBeNull());
    if (!global)
        return;
    global->addJSHandle(m_identifier, *object);
    ASSERT(&global->vm() == commonVMOrNull());
    handleToGlobalMap(global->vm()).set(m_identifier, global);
}

}
