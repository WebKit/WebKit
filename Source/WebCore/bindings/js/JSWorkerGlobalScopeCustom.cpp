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
#include "JSWorkerGlobalScope.h"

#include "JSDOMExceptionHandling.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/JSMicrotask.h>

namespace WebCore {
using namespace JSC;

template<typename Visitor>
void JSWorkerGlobalScope::visitAdditionalChildren(Visitor& visitor)
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

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSWorkerGlobalScope);

JSValue JSWorkerGlobalScope::queueMicrotask(JSGlobalObject& lexicalGlobalObject, CallFrame& callFrame)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(callFrame.argumentCount() < 1))
        return throwException(&lexicalGlobalObject, scope, createNotEnoughArgumentsError(&lexicalGlobalObject));

    JSValue functionValue = callFrame.uncheckedArgument(0);
    if (UNLIKELY(!functionValue.isCallable(vm)))
        return JSValue::decode(throwArgumentMustBeFunctionError(lexicalGlobalObject, scope, 0, "callback", "WorkerGlobalScope", "queueMicrotask"));

    scope.release();
    Base::queueMicrotask(JSC::createJSMicrotask(vm, functionValue));
    return jsUndefined();
}

} // namespace WebCore
