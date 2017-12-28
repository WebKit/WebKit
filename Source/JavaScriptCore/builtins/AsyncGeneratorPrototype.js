/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
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

@globalPrivate
function asyncGeneratorQueueIsEmpty(generator)
{
    "use strict";

    return generator.@asyncGeneratorQueueLast === null;
}

@globalPrivate
function asyncGeneratorQueueEnqueue(generator, item)
{
    "use strict";

    @assert(item.@asyncGeneratorQueueItemNext === null && item.@asyncGeneratorQueueItemPrevious === null);

    if (generator.@asyncGeneratorQueueFirst === null) {
        @assert(generator.@asyncGeneratorQueueLast === null);

        generator.@asyncGeneratorQueueFirst = item;
        generator.@asyncGeneratorQueueLast = item;
    } else {
        item.@asyncGeneratorQueueItemPrevious = generator.@asyncGeneratorQueueLast;
        generator.@asyncGeneratorQueueLast.@asyncGeneratorQueueItemNext = item;
        generator.@asyncGeneratorQueueLast = item;
    }
}

@globalPrivate
function asyncGeneratorQueueDequeue(generator)
{
    "use strict";

    if (generator.@asyncGeneratorQueueFirst === null)
        return null;

    const result = generator.@asyncGeneratorQueueFirst;
    generator.@asyncGeneratorQueueFirst = result.@asyncGeneratorQueueItemNext;

    if (generator.@asyncGeneratorQueueFirst === null)
        generator.@asyncGeneratorQueueLast = null;

    return result;
}

@globalPrivate
function asyncGeneratorDequeue(generator)
{
    "use strict";

    const queue = generator.@asyncGeneratorQueue;

    @assert(!@asyncGeneratorQueueIsEmpty(generator), "Async genetator's Queue is an empty List.");
    
    return @asyncGeneratorQueueDequeue(generator);
}

@globalPrivate
function isExecutionState(generator)
{
    "use strict";

    return (generator.@generatorState > 0 && generator.@asyncGeneratorSuspendReason === @AsyncGeneratorSuspendReasonNone)
        || generator.@generatorState === @AsyncGeneratorStateExecuting
        || generator.@asyncGeneratorSuspendReason === @AsyncGeneratorSuspendReasonAwait;
}

@globalPrivate
function isSuspendYieldState(generator)
{
    "use strict";

    return (generator.@generatorState > 0 && generator.@asyncGeneratorSuspendReason === @AsyncGeneratorSuspendReasonYield)
        || generator.@generatorState === @AsyncGeneratorStateSuspendedYield;
}

@globalPrivate
function asyncGeneratorReject(generator, exception)
{
    "use strict";

    @assert(typeof generator.@asyncGeneratorSuspendReason === "number", "Generator is not an AsyncGenerator instance.");

    const { promiseCapability } = @asyncGeneratorDequeue(generator);
    promiseCapability.@reject.@call(@undefined, exception);

    return @asyncGeneratorResumeNext(generator);
}

@globalPrivate
function asyncGeneratorResolve(generator, value, done)
{
    "use strict";

    @assert(typeof generator.@asyncGeneratorSuspendReason === "number", "Generator is not an AsyncGenerator instance.");

    const { promiseCapability } = @asyncGeneratorDequeue(generator);
    promiseCapability.@resolve.@call(@undefined, { done, value: value });

    return @asyncGeneratorResumeNext(generator);
}

@globalPrivate
function asyncGeneratorYield(generator, value, resumeMode)
{
    "use strict";

    function asyncGeneratorYieldAwaited(result)
    {
        generator.@asyncGeneratorSuspendReason = @AsyncGeneratorSuspendReasonYield;
        @asyncGeneratorResolve(generator, result, false);
    }

    generator.@asyncGeneratorSuspendReason = @AsyncGeneratorSuspendReasonAwait;

    @awaitValue(generator, value, asyncGeneratorYieldAwaited);

    return @undefined;
}

@globalPrivate
function awaitValue(generator, value, onFullfiled)
{
    "use strict";

    const wrappedValue = @newPromiseCapability(@Promise);

    const onRejected = function (result) { @doAsyncGeneratorBodyCall(generator, result, @GeneratorResumeModeThrow); };

    wrappedValue.@resolve.@call(@undefined, value);
    wrappedValue.@promise.@then(onFullfiled, onRejected);

    return wrappedValue;
}

@globalPrivate
function doAsyncGeneratorBodyCall(generator, resumeValue, resumeMode)
{
    "use strict";

    let value = @undefined;
    let state = generator.@generatorState;

    generator.@generatorState = @AsyncGeneratorStateExecuting;
    generator.@asyncGeneratorSuspendReason = @AsyncGeneratorSuspendReasonNone;

    try {
        value = generator.@generatorNext.@call(generator.@generatorThis, generator, state, resumeValue, resumeMode, generator.@generatorFrame);
        if (generator.@generatorState === @AsyncGeneratorStateExecuting)
            generator.@generatorState = @AsyncGeneratorStateCompleted;
    } catch (error) {
        generator.@generatorState = @AsyncGeneratorStateCompleted;
        generator.@asyncGeneratorSuspendReason = @AsyncGeneratorSuspendReasonNone;

        return @asyncGeneratorReject(generator, error);
    }

    if (generator.@asyncGeneratorSuspendReason === @AsyncGeneratorSuspendReasonAwait) {
        const onFulfilled = function(result) { @doAsyncGeneratorBodyCall(generator, result, @GeneratorResumeModeNormal); };

        @awaitValue(generator, value, onFulfilled);

        return @undefined;
    }

    if (generator.@asyncGeneratorSuspendReason === @AsyncGeneratorSuspendReasonYield)
        return @asyncGeneratorYield(generator, value, resumeMode);

    if (generator.@generatorState === @AsyncGeneratorStateCompleted) {
        generator.@asyncGeneratorSuspendReason = @AsyncGeneratorSuspendReasonNone;
        return @asyncGeneratorResolve(generator, value, true);
    }

    return @undefined;
}

@globalPrivate
function asyncGeneratorResumeNext(generator)
{
    "use strict";

    @assert(typeof generator.@asyncGeneratorSuspendReason === "number", "Generator is not an AsyncGenerator instance.");

    let state = generator.@generatorState;

    @assert(state !== @AsyncGeneratorStateExecuting, "Async generator should not be in executing state");

    if (state === @AsyncGeneratorStateAwaitingReturn)
        return @undefined;

    if (@asyncGeneratorQueueIsEmpty(generator))
        return @undefined;

    const next = generator.@asyncGeneratorQueueFirst;

    if (next.resumeMode !== @GeneratorResumeModeNormal) {
        if (state === @AsyncGeneratorStateSuspendedStart) {
            generator.@generatorState = @AsyncGeneratorStateCompleted;
            state = @AsyncGeneratorStateCompleted;
        }

        if (state === @AsyncGeneratorStateCompleted) {
            if (next.resumeMode === @GeneratorResumeModeReturn) {
                generator.@generatorState = @AsyncGeneratorStateAwaitingReturn;

                const promiseCapability = @newPromiseCapability(@Promise);
                promiseCapability.@resolve.@call(@undefined, next.value);

                const throwawayCapabilityPromise = promiseCapability.@promise.@then(
                    function (result) { generator.@generatorState = @AsyncGeneratorStateCompleted; @asyncGeneratorResolve(generator, result, true); },
                    function (error) { generator.@generatorState = @AsyncGeneratorStateCompleted; @asyncGeneratorReject(generator, error); });

                throwawayCapabilityPromise.@promiseIsHandled = true;

                return @undefined;
            }

            @assert(next.resumeMode === @GeneratorResumeModeThrow, "Async generator has wrong mode");

            return @asyncGeneratorReject(generator, next.value);;
        }
    } else if (state === @AsyncGeneratorStateCompleted)
        return @asyncGeneratorResolve(generator, @undefined, true);

    @assert(state === @AsyncGeneratorStateSuspendedStart || @isSuspendYieldState(generator), "Async generator has wrong state");

    @doAsyncGeneratorBodyCall(generator, next.value, next.resumeMode);

    return @undefined;
}

@globalPrivate
function asyncGeneratorEnqueue(generator, value, resumeMode)
{
    "use strict";
    
    const promiseCapability = @newPromiseCapability(@Promise);
    if (!@isObject(generator) || typeof generator.@asyncGeneratorSuspendReason !== 'number') {
        promiseCapability.@reject.@call(@undefined, new @TypeError('|this| should be an async generator'));
        return promiseCapability.@promise;
    }

    @asyncGeneratorQueueEnqueue(generator, {resumeMode, value, promiseCapability, @asyncGeneratorQueueItemNext: null, @asyncGeneratorQueueItemPrevious: null});

    if (!@isExecutionState(generator))
        @asyncGeneratorResumeNext(generator);

    return promiseCapability.@promise;
}

function next(value)
{
    "use strict";

    return @asyncGeneratorEnqueue(this, value, @GeneratorResumeModeNormal);
}

function return(value)
{
    "use strict";

    return @asyncGeneratorEnqueue(this, value, @GeneratorResumeModeReturn);
}

function throw(exception)
{
    "use strict";
    
    return @asyncGeneratorEnqueue(this, exception, @GeneratorResumeModeThrow);
}
