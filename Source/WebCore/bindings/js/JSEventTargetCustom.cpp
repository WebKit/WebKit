/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSEventTarget.h"

#include "EventTarget.h"
#include "EventTargetHeaders.h"
#include "EventTargetInterfaces.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSEventListener.h"
#include "JSLocalDOMWindow.h"
#include "JSWindowProxy.h"
#include "JSWorkerGlobalScope.h"
#include "LocalDOMWindow.h"
#include "WorkerGlobalScope.h"

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif

namespace WebCore {
using namespace JSC;

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<EventTarget>&& value)
{
    return createWrapper<EventTarget>(globalObject, WTFMove(value));
}

EventTarget* JSEventTarget::toWrapped(VM&, JSValue value)
{
    if (value.inherits<JSWindowProxy>())
        return &jsCast<JSWindowProxy*>(asObject(value))->wrapped();
    if (value.inherits<JSLocalDOMWindow>())
        return &jsCast<JSLocalDOMWindow*>(asObject(value))->wrapped();
    if (value.inherits<JSWorkerGlobalScope>())
        return &jsCast<JSWorkerGlobalScope*>(asObject(value))->wrapped();
    if (value.inherits<JSEventTarget>())
        return &jsCast<JSEventTarget*>(asObject(value))->wrapped();
    return nullptr;
}

JSEventTargetWrapper jsEventTargetCast(VM& vm, JSValue thisValue)
{
    if (auto* target = jsDynamicCast<JSEventTarget*>(thisValue))
        return { target->wrapped(), *target };
    if (auto* window = toJSDOMGlobalObject<JSLocalDOMWindow>(vm, thisValue))
        return { window->wrapped(), *window };
    if (auto* scope = toJSDOMGlobalObject<JSWorkerGlobalScope>(vm, thisValue))
        return { scope->wrapped(), *scope };
    return { };
}

template<typename Visitor>
void JSEventTarget::visitAdditionalChildren(Visitor& visitor)
{
    wrapped().visitJSEventListeners(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSEventTarget);

} // namespace WebCore
