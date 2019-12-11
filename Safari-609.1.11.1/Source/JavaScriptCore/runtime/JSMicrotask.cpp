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
#include "Error.h"
#include "Exception.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "Microtask.h"
#include "StrongInlines.h"

namespace JSC {

class JSMicrotask final : public Microtask {
public:
    static constexpr unsigned maxArguments = 3;
    JSMicrotask(VM& vm, JSValue job, JSValue argument0, JSValue argument1, JSValue argument2)
    {
        m_job.set(vm, job);
        m_arguments[0].set(vm, argument0);
        m_arguments[1].set(vm, argument1);
        m_arguments[2].set(vm, argument2);
    }

    JSMicrotask(VM& vm, JSValue job)
    {
        m_job.set(vm, job);
    }

private:
    void run(JSGlobalObject*) override;

    Strong<Unknown> m_job;
    Strong<Unknown> m_arguments[maxArguments];
};

Ref<Microtask> createJSMicrotask(VM& vm, JSValue job)
{
    return adoptRef(*new JSMicrotask(vm, job));
}

Ref<Microtask> createJSMicrotask(VM& vm, JSValue job, JSValue argument0, JSValue argument1, JSValue argument2)
{
    return adoptRef(*new JSMicrotask(vm, job, argument0, argument1, argument2));
}

void JSMicrotask::run(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    CallData handlerCallData;
    CallType handlerCallType = getCallData(vm, m_job.get(), handlerCallData);
    ASSERT(handlerCallType != CallType::None);

    MarkedArgumentBuffer handlerArguments;
    for (unsigned index = 0; index < maxArguments; ++index) {
        JSValue arg = m_arguments[index].get();
        if (!arg)
            break;
        handlerArguments.append(arg);
    }
    if (UNLIKELY(handlerArguments.hasOverflowed()))
        return;

    if (UNLIKELY(globalObject->hasDebugger()))
        globalObject->debugger()->willRunMicrotask();

    profiledCall(globalObject, ProfilingReason::Microtask, m_job.get(), handlerCallType, handlerCallData, jsUndefined(), handlerArguments);
    scope.clearException();
}

} // namespace JSC
