/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

// https://tc39.es/ecma262/#sec-asyncgenerator-prototype-next
function next(value)
{
    "use strict";

    var promise = @newPromise();
    // 3. Let result be Completion(AsyncGeneratorValidate(gen, empty)).
    // 4. IfAbruptRejectPromise(result, promiseCapability).
    // 5. Let state be gen.[[AsyncGeneratorState]].
    // 6. If state is completed, then
    // 6.a. Let iteratorResult be CreateIteratorResultObject(undefined, true).
    // 6.b. Perform ! Call(promiseCapability.[[Resolve]], undefined, « iteratorResult »).
    // 6.c. Return promiseCapability.[[Promise]].
    // 7. Let completion be NormalCompletion(value).
    // 8. Perform AsyncGeneratorEnqueue(gen, completion, promiseCapability).
    // 9. If state is either suspended-start or suspended-yield, then
    // 9.a. Perform AsyncGeneratorResume(gen, completion).
    // 10. Else,
    // 10.a. Assert: state is either executing or draining-queue.
    var resumeMode = @asyncGeneratorNextQueueEnqueue(this, value, @GeneratorResumeModeNormal, promise);
    if (resumeMode === @AsyncGeneratorResumeModeEmpty)
        return promise;
    @assert(@isAsyncGenerator(this), "Generator is not an AsyncGenerator instance.");

    // https://tc39.es/ecma262/#sec-asyncgeneratorresume
    //
    // 3. Set gen.[[AsyncGeneratorState]] to executing.
    // 4. Perform ! RunSuspendedContext(genContext, completion).
    var state = @getAsyncGeneratorInternalField(this, @generatorFieldState);
    @putAsyncGeneratorInternalField(this, @generatorFieldState, @AsyncGeneratorStateExecuting);

    var result;
    try {
        result = @getAsyncGeneratorInternalField(this, @generatorFieldNext).@call(
            @getAsyncGeneratorInternalField(this, @generatorFieldThis),
            this,
            state >> @AsyncGeneratorSuspendReasonShift,
            @getAsyncGeneratorInternalField(this, @asyncGeneratorFieldResumeValue),
            @GeneratorResumeModeNormal,
            @getAsyncGeneratorInternalField(this, @generatorFieldFrame));
    } catch (error) {
        // https://tc39.es/ecma262/#sec-asyncgeneratorstart
        // 4.g. Set acGen.[[AsyncGeneratorState]] to draining-queue.
        // 4.h. If result is a normal completion, set result to NormalCompletion(undefined).
        // 4.i. If result is a return completion, set result to NormalCompletion(result.[[Value]]).
        // 4.j. Perform AsyncGeneratorCompleteStep(acGen, result, true).
        // 4.k. Perform AsyncGeneratorDrainQueue(acGen).
        @asyncGeneratorCompleteAndDrain(this, error, true);
        return promise;
    }

    state = @getAsyncGeneratorInternalField(this, @generatorFieldState);

    // Body suspended at `await` / `yield` / `yield*`.
    if (state > 0) {
        @asyncGeneratorSuspend(this, result);
        return promise;
    }

    // https://tc39.es/ecma262/#sec-asyncgeneratorstart
    // 4.g. Set acGen.[[AsyncGeneratorState]] to draining-queue.
    // 4.h. If result is a normal completion, set result to NormalCompletion(undefined).
    // 4.i. If result is a return completion, set result to NormalCompletion(result.[[Value]]).
    // 4.j. Perform AsyncGeneratorCompleteStep(acGen, result, true).
    // 4.k. Perform AsyncGeneratorDrainQueue(acGen).
    @asyncGeneratorCompleteAndDrain(this, result, false);
    return promise;
}
