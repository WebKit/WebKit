/*
 * Copyright (C) 2008, 2016 Apple Inc. All Rights Reserved.
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

#include "DOMWindow.h"
#include "EventTarget.h"
#include "EventTargetHeaders.h"
#include "EventTargetInterfaces.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowShell.h"
#include "JSEventListener.h"
#include "JSWorkerGlobalScope.h"
#include "WorkerGlobalScope.h"

using namespace JSC;

namespace WebCore {

#define TRY_TO_WRAP_WITH_INTERFACE(interfaceName) \
    case interfaceName##EventTargetInterfaceType: \
        return toJS(exec, globalObject, static_cast<interfaceName&>(target));

JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, EventTarget& target)
{
    switch (target.eventTargetInterface()) {
    DOM_EVENT_TARGET_INTERFACES_FOR_EACH(TRY_TO_WRAP_WITH_INTERFACE)
    }

    ASSERT_NOT_REACHED();
    return jsNull();
}

#undef TRY_TO_WRAP_WITH_INTERFACE

#define TRY_TO_UNWRAP_WITH_INTERFACE(interfaceName) \
    if (value.inherits(JS##interfaceName::info()))                      \
        return &jsCast<JS##interfaceName*>(asObject(value))->wrapped();

EventTarget* JSEventTarget::toWrapped(ExecState&, JSValue value)
{
    TRY_TO_UNWRAP_WITH_INTERFACE(DOMWindowShell)
    TRY_TO_UNWRAP_WITH_INTERFACE(DOMWindow)
    TRY_TO_UNWRAP_WITH_INTERFACE(WorkerGlobalScope)
    TRY_TO_UNWRAP_WITH_INTERFACE(EventTarget)
    return nullptr;
}

#undef TRY_TO_UNWRAP_WITH_INTERFACE

std::unique_ptr<JSEventTargetWrapper> jsEventTargetCast(JSValue thisValue)
{
    if (auto* target = jsDynamicCast<JSEventTarget*>(thisValue))
        return std::make_unique<JSEventTargetWrapper>(target->wrapped(), *target);
    if (auto* window = toJSDOMWindow(thisValue))
        return std::make_unique<JSEventTargetWrapper>(window->wrapped(), *window);
    if (auto* scope = toJSWorkerGlobalScope(thisValue))
        return std::make_unique<JSEventTargetWrapper>(scope->wrapped(), *scope);
    return nullptr;
}

} // namespace WebCore
