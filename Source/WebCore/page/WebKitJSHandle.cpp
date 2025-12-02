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

#include "DOMWindow.h"
#include "Frame.h"
#include "JSWindowProxy.h"
#include <JavaScriptCore/JSCellInlines.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/Weak.h>

namespace WebCore {

struct JSHandleData {
    JSC::Strong<JSC::JSObject> strongReference;
    size_t refCount { 0 };
};
using HandleMap = HashMap<JSHandleIdentifier, JSHandleData>;
static HandleMap& handleMap()
{
    static MainThreadNeverDestroyed<HandleMap> map;
    return map.get();
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
    auto it = handleMap().find(identifier);
    if (it == handleMap().end()) {
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT(it->value.refCount);
    ++it->value.refCount;
}

void WebKitJSHandle::jsHandleDestroyed(JSHandleIdentifier identifier)
{
    auto it = handleMap().find(identifier);
    if (it == handleMap().end()) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (!--it->value.refCount)
        handleMap().remove(identifier);
}

JSC::JSObject* WebKitJSHandle::objectForIdentifier(JSHandleIdentifier identifier)
{
    auto it = handleMap().find(identifier);
    if (it == handleMap().end()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    return it->value.strongReference.get();
}

static Markable<FrameIdentifier> windowFrameIdentifier(JSC::JSObject* object)
{
    if (auto* window = jsDynamicCast<WebCore::JSWindowProxy*>(object)) {
        if (RefPtr frame = window->protectedWrapped()->frame())
            return frame->frameID();
    }
    return std::nullopt;
}

WebKitJSHandle::WebKitJSHandle(JSC::JSObject* object)
    : m_identifier(JSHandleIdentifier(WebProcessJSHandleIdentifier(reinterpret_cast<uintptr_t>(object)), Process::identifier()))
    , m_windowFrameIdentifier(WebCore::windowFrameIdentifier(object))
{
    auto addResult = handleMap().ensure(m_identifier, [&] {
        return JSHandleData {
            JSC::Strong<JSC::JSObject> { object->globalObject()->vm(), object },
            0 // Immediately incremented.
        };
    });
    auto& data = addResult.iterator->value;
    data.refCount++;
}

}
