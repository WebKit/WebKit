/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "JSMicrotask.h"

#include "CatchScope.h"
#include "Debugger.h"
#include "DeferTermination.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "Microtask.h"
#include "StrongInlines.h"
#include "VMTrapsInlines.h"

namespace JSC {

class JSMicrotask final : public Microtask {
public:
    static constexpr unsigned maxArguments = 4;
    JSMicrotask(VM& vm, JSValue job, JSValue argument0, JSValue argument1, JSValue argument2, JSValue argument3)
    {
        m_job.set(vm, job);
        if (argument0 && !argument0.isUndefined())
            m_arguments[0].set(vm, argument0);
        if (argument1 && !argument1.isUndefined())
            m_arguments[1].set(vm, argument1);
        if (argument2 && !argument2.isUndefined())
            m_arguments[2].set(vm, argument2);
        if (argument3 && !argument3.isUndefined())
            m_arguments[3].set(vm, argument3);
    }

private:
    void run(JSGlobalObject*) final;

    Strong<Unknown> m_job;
    Strong<Unknown> m_arguments[maxArguments];
};

Ref<Microtask> createJSMicrotask(VM& vm, JSValue job, JSValue argument0, JSValue argument1, JSValue argument2, JSValue argument3)
{
    return adoptRef(*new JSMicrotask(vm, job, argument0, argument1, argument2, argument3));
}

void runJSMicrotask(JSGlobalObject* globalObject, MicrotaskIdentifier identifier, JSValue job, JSValue argument0, JSValue argument1, JSValue argument2, JSValue argument3)
{
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    // If termination is issued, do not run microtasks. Otherwise, microtask should not care about exceptions.
    if (UNLIKELY(!scope.clearExceptionExceptTermination()))
        return;

    auto handlerCallData = JSC::getCallData(job);
    if (UNLIKELY(!scope.clearExceptionExceptTermination()))
        return;
    ASSERT(handlerCallData.type != CallData::Type::None);

    MarkedArgumentBuffer handlerArguments;
    handlerArguments.append(!argument0 ? jsUndefined() : argument0);
    handlerArguments.append(!argument1 ? jsUndefined() : argument1);
    handlerArguments.append(!argument2 ? jsUndefined() : argument2);
    handlerArguments.append(!argument3 ? jsUndefined() : argument3);
    if (UNLIKELY(handlerArguments.hasOverflowed()))
        return;

    if (UNLIKELY(globalObject->hasDebugger())) {
        DeferTerminationForAWhile deferTerminationForAWhile(vm);
        globalObject->debugger()->willRunMicrotask(globalObject, identifier);
        scope.clearException();
    }

    if (LIKELY(!vm.hasPendingTerminationException())) {
        profiledCall(globalObject, ProfilingReason::Microtask, job, handlerCallData, jsUndefined(), handlerArguments);
        scope.clearExceptionExceptTermination();
    }

    if (UNLIKELY(globalObject->hasDebugger())) {
        DeferTerminationForAWhile deferTerminationForAWhile(vm);
        globalObject->debugger()->didRunMicrotask(globalObject, identifier);
        scope.clearException();
    }
}

void JSMicrotask::run(JSGlobalObject* globalObject)
{
    runJSMicrotask(globalObject, identifier(), m_job.get(), m_arguments[0].get(), m_arguments[1].get(), m_arguments[2].get(), m_arguments[3].get());
}

} // namespace JSC
