/*
 * Copyright (C) 2008-2009, 2011, 2016 Apple Inc. All Rights Reserved.
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
#include "JSWorkerGlobalScope.h"

#include "JSDOMConvert.h"
#include "ScheduledAction.h"
#include "WorkerGlobalScope.h"

using namespace JSC;

namespace WebCore {

void JSWorkerGlobalScope::visitAdditionalChildren(SlotVisitor& visitor)
{
    if (auto* location = wrapped().optionalLocation())
        visitor.addOpaqueRoot(location);
    if (auto* navigator = wrapped().optionalNavigator())
        visitor.addOpaqueRoot(navigator);
    ScriptExecutionContext& context = wrapped();
    visitor.addOpaqueRoot(&context);
    
    // Normally JSEventTargetCustom.cpp's JSEventTarget::visitAdditionalChildren() would call this. But
    // even though WorkerGlobalScope is an EventTarget, JSWorkerGlobalScope does not subclass
    // JSEventTarget, so we need to do this here.
    wrapped().visitJSEventListeners(visitor);
}

JSValue JSWorkerGlobalScope::setTimeout(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 1))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    std::unique_ptr<ScheduledAction> action = ScheduledAction::create(&state, globalObject()->world(), wrapped().contentSecurityPolicy());
    RETURN_IF_EXCEPTION(scope, JSValue());
    if (!action)
        return jsNumber(0);
    int delay = state.argument(1).toInt32(&state);
    return jsNumber(wrapped().setTimeout(WTFMove(action), delay));
}

JSValue JSWorkerGlobalScope::setInterval(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 1))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    std::unique_ptr<ScheduledAction> action = ScheduledAction::create(&state, globalObject()->world(), wrapped().contentSecurityPolicy());
    RETURN_IF_EXCEPTION(scope, JSValue());
    if (!action)
        return jsNumber(0);
    int delay = state.argument(1).toInt32(&state);
    return jsNumber(wrapped().setInterval(WTFMove(action), delay));
}

} // namespace WebCore
