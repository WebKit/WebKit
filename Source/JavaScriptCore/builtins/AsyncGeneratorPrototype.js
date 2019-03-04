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

    return @getByIdDirectPrivate(generator, "asyncGeneratorQueueLast") === null;
}

@globalPrivate
function asyncGeneratorQueueEnqueue(generator, item)
{
    "use strict";

    @assert(@getByIdDirectPrivate(item, "asyncGeneratorQueueItemNext") === null && @getByIdDirectPrivate(item, "asyncGeneratorQueueItemPrevious") === null);

    if (@getByIdDirectPrivate(generator, "asyncGeneratorQueueFirst") === null) {
        @assert(@getByIdDirectPrivate(generator, "asyncGeneratorQueueLast") === null);

        @putByIdDirectPrivate(generator, "asyncGeneratorQueueFirst", item);
        @putByIdDirectPrivate(generator, "asyncGeneratorQueueLast", item);
    } else {
        var last = @getByIdDirectPrivate(generator, "asyncGeneratorQueueLast");
        @putByIdDirectPrivate(item, "asyncGeneratorQueueItemPrevious", last);
        @putByIdDirectPrivate(last, "asyncGeneratorQueueItemNext", item);
        @putByIdDirectPrivate(generator, "asyncGeneratorQueueLast", item);
    }
}

@globalPrivate
function asyncGeneratorQueueDequeue(generator)
{
    "use strict";

    const result = @getByIdDirectPrivate(generator, "asyncGeneratorQueueFirst");
    if (result === null)
        return null;

    var updatedFirst = @getByIdDirectPrivate(result, "asyncGeneratorQueueItemNext");
    @putByIdDirectPrivate(generator, "asyncGeneratorQueueFirst", updatedFirst);

    if (updatedFirst === null)
        @putByIdDirectPrivate(generator, "asyncGeneratorQueueLast", null);

    return result;
}

@globalPrivate
function asyncGeneratorDequeue(generator)
{
    "use strict";

    const queue = @getByIdDirectPrivate(generator, "asyncGeneratorQueue");

    @assert(!@asyncGeneratorQueueIsEmpty(generator), "Async genetator's Queue is an empty List.");
    
    return @asyncGeneratorQueueDequeue(generator);
}

@globalPrivate
function isExecutionState(generator)
{
    "use strict";

    var state = @getByIdDirectPrivate(generator, "generatorState");
    var reason = @getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason");
    return (state > 0 && reason === @AsyncGeneratorSuspendReasonNone)
        || state === @AsyncGeneratorStateExecuting
        || reason === @AsyncGeneratorSuspendReasonAwait;
}

@globalPrivate
function isSuspendYieldState(generator)
{
    "use strict";

    var state = @getByIdDirectPrivate(generator, "generatorState");
    return (state > 0 && @getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") === @AsyncGeneratorSuspendReasonYield)
        || state === @AsyncGeneratorStateSuspendedYield;
}

@globalPrivate
function asyncGeneratorReject(generator, exception)
{
    "use strict";

    @assert(typeof @getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") === "number", "Generator is not an AsyncGenerator instance.");

    const { promiseCapability } = @asyncGeneratorDequeue(generator);
    promiseCapability.@reject.@call(@undefined, exception);

    return @asyncGeneratorResumeNext(generator);
}

@globalPrivate
function asyncGeneratorResolve(generator, value, done)
{
    "use strict";

    @assert(typeof @getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") === "number", "Generator is not an AsyncGenerator instance.");

    const { promiseCapability } = @asyncGeneratorDequeue(generator);
    promiseCapability.@resolve.@call(@undefined, { value, done });

    return @asyncGeneratorResumeNext(generator);
}

@globalPrivate
function asyncGeneratorYield(generator, value, resumeMode)
{
    "use strict";

    function asyncGeneratorYieldAwaited(result)
    {
        @putByIdDirectPrivate(generator, "asyncGeneratorSuspendReason", @AsyncGeneratorSuspendReasonYield);
        @asyncGeneratorResolve(generator, result, false);
    }

    @putByIdDirectPrivate(generator, "asyncGeneratorSuspendReason", @AsyncGeneratorSuspendReasonAwait);

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
    let state = @getByIdDirectPrivate(generator, "generatorState");

    @putByIdDirectPrivate(generator, "generatorState", @AsyncGeneratorStateExecuting);
    @putByIdDirectPrivate(generator, "asyncGeneratorSuspendReason", @AsyncGeneratorSuspendReasonNone);

    try {
        value = @getByIdDirectPrivate(generator, "generatorNext").@call(@getByIdDirectPrivate(generator, "generatorThis"), generator, state, resumeValue, resumeMode, @getByIdDirectPrivate(generator, "generatorFrame"));
        if (@getByIdDirectPrivate(generator, "generatorState") === @AsyncGeneratorStateExecuting)
            @putByIdDirectPrivate(generator, "generatorState", @AsyncGeneratorStateCompleted);
    } catch (error) {
        @putByIdDirectPrivate(generator, "generatorState", @AsyncGeneratorStateCompleted);
        @putByIdDirectPrivate(generator, "asyncGeneratorSuspendReason", @AsyncGeneratorSuspendReasonNone);

        return @asyncGeneratorReject(generator, error);
    }

    if (@getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") === @AsyncGeneratorSuspendReasonAwait) {
        const onFulfilled = function(result) { @doAsyncGeneratorBodyCall(generator, result, @GeneratorResumeModeNormal); };

        @awaitValue(generator, value, onFulfilled);

        return @undefined;
    }

    if (@getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") === @AsyncGeneratorSuspendReasonYield)
        return @asyncGeneratorYield(generator, value, resumeMode);

    if (@getByIdDirectPrivate(generator, "generatorState") === @AsyncGeneratorStateCompleted) {
        @putByIdDirectPrivate(generator, "asyncGeneratorSuspendReason", @AsyncGeneratorSuspendReasonNone);
        return @asyncGeneratorResolve(generator, value, true);
    }

    return @undefined;
}

@globalPrivate
function asyncGeneratorResumeNext(generator)
{
    "use strict";

    @assert(typeof @getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") === "number", "Generator is not an AsyncGenerator instance.");

    let state = @getByIdDirectPrivate(generator, "generatorState");

    @assert(state !== @AsyncGeneratorStateExecuting, "Async generator should not be in executing state");

    if (state === @AsyncGeneratorStateAwaitingReturn)
        return @undefined;

    if (@asyncGeneratorQueueIsEmpty(generator))
        return @undefined;

    const next = @getByIdDirectPrivate(generator, "asyncGeneratorQueueFirst");

    if (next.resumeMode !== @GeneratorResumeModeNormal) {
        if (state === @AsyncGeneratorStateSuspendedStart) {
            @putByIdDirectPrivate(generator, "generatorState", @AsyncGeneratorStateCompleted);
            state = @AsyncGeneratorStateCompleted;
        }

        if (state === @AsyncGeneratorStateCompleted) {
            if (next.resumeMode === @GeneratorResumeModeReturn) {
                @putByIdDirectPrivate(generator, "generatorState", @AsyncGeneratorStateAwaitingReturn);

                const promiseCapability = @newPromiseCapability(@Promise);
                promiseCapability.@resolve.@call(@undefined, next.value);

                const throwawayCapabilityPromise = promiseCapability.@promise.@then(
                    function (result) { generator.@generatorState = @AsyncGeneratorStateCompleted; @asyncGeneratorResolve(generator, result, true); },
                    function (error) { generator.@generatorState = @AsyncGeneratorStateCompleted; @asyncGeneratorReject(generator, error); });

                @putByIdDirectPrivate(throwawayCapabilityPromise, "promiseIsHandled", true);

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
    if (!@isObject(generator) || typeof @getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") !== 'number') {
        promiseCapability.@reject.@call(@undefined, @makeTypeError('|this| should be an async generator'));
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
