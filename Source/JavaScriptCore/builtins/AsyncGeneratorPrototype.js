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

@linkTimeConstant
function asyncGeneratorQueueIsEmpty(generator)
{
    "use strict";

    return @getAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueFirst) === null;
}

@linkTimeConstant
function asyncGeneratorQueueEnqueue(generator, item)
{
    "use strict";

    @assert(@getByIdDirectPrivate(item, "asyncGeneratorQueueItemNext") === null);

    if (@getAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueFirst) === null) {
        @assert(@getAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueLast) === null);

        @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueFirst, item);
        @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueLast, item);
    } else {
        var last = @getAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueLast);
        @putByIdDirectPrivate(last, "asyncGeneratorQueueItemNext", item);
        @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueLast, item);
    }
}

@linkTimeConstant
function asyncGeneratorQueueDequeue(generator)
{
    "use strict";

    @assert(!@asyncGeneratorQueueIsEmpty(generator), "Async genetator's Queue is an empty List.");

    var result = @getAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueFirst);

    var updatedFirst = @getByIdDirectPrivate(result, "asyncGeneratorQueueItemNext");
    @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueFirst, updatedFirst);

    if (updatedFirst === null)
        @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueLast, null);

    return result;
}

@linkTimeConstant
function isExecutionState(generator)
{
    "use strict";

    var state = @getAsyncGeneratorInternalField(generator, @generatorFieldState);
    var reason = @getAsyncGeneratorInternalField(generator, @asyncGeneratorFieldSuspendReason);
    return (state > 0 && reason === @AsyncGeneratorSuspendReasonNone)
        || state === @AsyncGeneratorStateExecuting
        || reason === @AsyncGeneratorSuspendReasonAwait;
}

@linkTimeConstant
function isSuspendYieldState(generator)
{
    "use strict";

    var state = @getAsyncGeneratorInternalField(generator, @generatorFieldState);
    return (state > 0 && @getAsyncGeneratorInternalField(generator, @asyncGeneratorFieldSuspendReason) === @AsyncGeneratorSuspendReasonYield)
        || state === @AsyncGeneratorStateSuspendedYield;
}

@linkTimeConstant
function asyncGeneratorReject(generator, exception)
{
    "use strict";

    @assert(@isAsyncGenerator(generator), "Generator is not an AsyncGenerator instance.");

    var promise = @asyncGeneratorQueueDequeue(generator).promise;
    @assert(@isPromise(promise));
    @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, exception);

    return @asyncGeneratorResumeNext(generator);
}

@linkTimeConstant
function asyncGeneratorResolve(generator, value, done)
{
    "use strict";

    @assert(@isAsyncGenerator(generator), "Generator is not an AsyncGenerator instance.");

    var promise = @asyncGeneratorQueueDequeue(generator).promise;
    @assert(@isPromise(promise));
    @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, { value, done });

    return @asyncGeneratorResumeNext(generator);
}

@linkTimeConstant
function asyncGeneratorYieldAwaited(result, generator)
{
    "use strict";

    @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldSuspendReason, @AsyncGeneratorSuspendReasonYield);
    @asyncGeneratorResolve(generator, result, false);
}

@linkTimeConstant
function asyncGeneratorYieldOnRejected(result, generator)
{
    "use strict";

    @doAsyncGeneratorBodyCall(generator, result, @GeneratorResumeModeThrow);
}

@linkTimeConstant
function asyncGeneratorYield(generator, value, resumeMode)
{
    "use strict";

    @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldSuspendReason, @AsyncGeneratorSuspendReasonAwait);
    @awaitValue(generator, value, @asyncGeneratorYieldAwaited);
}

@linkTimeConstant
function awaitValue(generator, value, onFulfilled)
{
    "use strict";

    @resolveWithoutPromiseForAsyncAwait(value, onFulfilled, @asyncGeneratorYieldOnRejected, generator);
}

@linkTimeConstant
function doAsyncGeneratorBodyCallOnFulfilledNormal(result, generator)
{
    "use strict";

    @doAsyncGeneratorBodyCall(generator, result, @GeneratorResumeModeNormal);
}

@linkTimeConstant
function doAsyncGeneratorBodyCallOnFulfilledReturn(result, generator)
{
    "use strict";

    @doAsyncGeneratorBodyCall(generator, result, @GeneratorResumeModeReturn);
}

@linkTimeConstant
function doAsyncGeneratorBodyCall(generator, resumeValue, resumeMode)
{
    "use strict";

    if (resumeMode === @GeneratorResumeModeReturn && @isSuspendYieldState(generator)) {
        @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldSuspendReason, @AsyncGeneratorSuspendReasonAwait);
        @awaitValue(generator, resumeValue, @doAsyncGeneratorBodyCallOnFulfilledReturn);
        return;
    }

    var value = @undefined;
    var state = @getAsyncGeneratorInternalField(generator, @generatorFieldState);

    @putAsyncGeneratorInternalField(generator, @generatorFieldState, @AsyncGeneratorStateExecuting);
    @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldSuspendReason, @AsyncGeneratorSuspendReasonNone);

    try {
        value = @getAsyncGeneratorInternalField(generator, @generatorFieldNext).@call(@getAsyncGeneratorInternalField(generator, @generatorFieldThis), generator, state, resumeValue, resumeMode, @getAsyncGeneratorInternalField(generator, @generatorFieldFrame));
        state = @getAsyncGeneratorInternalField(generator, @generatorFieldState);
        if (state === @AsyncGeneratorStateExecuting) {
            @putAsyncGeneratorInternalField(generator, @generatorFieldState, @AsyncGeneratorStateCompleted);
            state = @AsyncGeneratorStateCompleted;
        }
    } catch (error) {
        @putAsyncGeneratorInternalField(generator, @generatorFieldState, @AsyncGeneratorStateCompleted);
        @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldSuspendReason, @AsyncGeneratorSuspendReasonNone);

        return @asyncGeneratorReject(generator, error);
    }

    var reason = @getAsyncGeneratorInternalField(generator, @asyncGeneratorFieldSuspendReason);
    if (reason === @AsyncGeneratorSuspendReasonAwait) {
        @awaitValue(generator, value, @doAsyncGeneratorBodyCallOnFulfilledNormal);
        return;
    }

    if (reason === @AsyncGeneratorSuspendReasonYield)
        return @asyncGeneratorYield(generator, value, resumeMode);

    if (state === @AsyncGeneratorStateCompleted) {
        @assert(@getAsyncGeneratorInternalField(generator, @generatorFieldState) == @AsyncGeneratorStateCompleted);
        @putAsyncGeneratorInternalField(generator, @asyncGeneratorFieldSuspendReason, @AsyncGeneratorSuspendReasonNone);
        return @asyncGeneratorResolve(generator, value, true);
    }
}

@linkTimeConstant
function asyncGeneratorResumeNextOnFulfilled(result, generator)
{
    "use strict";

    @putAsyncGeneratorInternalField(generator, @generatorFieldState, @AsyncGeneratorStateCompleted);
    @asyncGeneratorResolve(generator, result, true);
}

@linkTimeConstant
function asyncGeneratorResumeNextOnRejected(error, generator)
{
    "use strict";

    @putAsyncGeneratorInternalField(generator, @generatorFieldState, @AsyncGeneratorStateCompleted);
    @asyncGeneratorReject(generator, error);
}

@linkTimeConstant
function asyncGeneratorResumeNext(generator)
{
    "use strict";

    @assert(@isAsyncGenerator(generator), "Generator is not an AsyncGenerator instance.");

    var state = @getAsyncGeneratorInternalField(generator, @generatorFieldState);

    @assert(state !== @AsyncGeneratorStateExecuting, "Async generator should not be in executing state");

    if (state === @AsyncGeneratorStateAwaitingReturn)
        return;

    if (@asyncGeneratorQueueIsEmpty(generator))
        return;

    var next = @getAsyncGeneratorInternalField(generator, @asyncGeneratorFieldQueueFirst);

    if (next.resumeMode !== @GeneratorResumeModeNormal) {
        if (state === @AsyncGeneratorStateSuspendedStart) {
            @putAsyncGeneratorInternalField(generator, @generatorFieldState, @AsyncGeneratorStateCompleted);
            state = @AsyncGeneratorStateCompleted;
        }

        if (state === @AsyncGeneratorStateCompleted) {
            if (next.resumeMode === @GeneratorResumeModeReturn) {
                @putAsyncGeneratorInternalField(generator, @generatorFieldState, @AsyncGeneratorStateAwaitingReturn);
                @resolveWithoutPromiseForAsyncAwait(next.value, @asyncGeneratorResumeNextOnFulfilled, @asyncGeneratorResumeNextOnRejected, generator);
                return;
            }

            @assert(next.resumeMode === @GeneratorResumeModeThrow, "Async generator has wrong mode");

            return @asyncGeneratorReject(generator, next.value);;
        }
    } else if (state === @AsyncGeneratorStateCompleted)
        return @asyncGeneratorResolve(generator, @undefined, true);

    @assert(state === @AsyncGeneratorStateSuspendedStart || @isSuspendYieldState(generator), "Async generator has wrong state");

    @doAsyncGeneratorBodyCall(generator, next.value, next.resumeMode);
}

@linkTimeConstant
function asyncGeneratorEnqueue(generator, value, resumeMode)
{
    "use strict";

    var promise = @newPromise();
    if (!@isAsyncGenerator(generator)) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, @makeTypeError('|this| should be an async generator'));
        return promise;
    }

    @asyncGeneratorQueueEnqueue(generator, {resumeMode, value, promise, @asyncGeneratorQueueItemNext: null});

    if (!@isExecutionState(generator))
        @asyncGeneratorResumeNext(generator);

    return promise;
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
