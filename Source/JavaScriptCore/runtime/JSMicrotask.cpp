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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

void runJSMicrotask(JSGlobalObject* globalObject, MicrotaskIdentifier identifier, JSValue job, std::span<const JSValue> arguments)
{
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (UNLIKELY(!job.isObject()))
        return;

    // If termination is issued, do not run microtasks. Otherwise, microtask should not care about exceptions.
    if (UNLIKELY(!scope.clearExceptionExceptTermination()))
        return;

    auto handlerCallData = JSC::getCallData(job);
    if (UNLIKELY(!scope.clearExceptionExceptTermination()))
        return;
    ASSERT(handlerCallData.type != CallData::Type::None);

    unsigned count = 0;
    for (auto argument : arguments) {
        if (!argument)
            break;
        ++count;
    }

    if (UNLIKELY(globalObject->hasDebugger())) {
        DeferTerminationForAWhile deferTerminationForAWhile(vm);
        globalObject->debugger()->willRunMicrotask(globalObject, identifier);
        scope.clearException();
    }

    if (LIKELY(!vm.hasPendingTerminationException())) {
        profiledCall(globalObject, ProfilingReason::Microtask, job, handlerCallData, jsUndefined(), ArgList { std::bit_cast<EncodedJSValue*>(arguments.data()), count });
        scope.clearExceptionExceptTermination();
    }

    if (UNLIKELY(globalObject->hasDebugger())) {
        DeferTerminationForAWhile deferTerminationForAWhile(vm);
        globalObject->debugger()->didRunMicrotask(globalObject, identifier);
        scope.clearException();
    }
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
