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

@globalPrivate
function asyncGeneratorQueueIsEmpty(generator)
{
    "use strict";

    return @getByIdDirectPrivate(generator, "asyncGeneratorQueueFirst") === null;
}

@globalPrivate
function asyncGeneratorQueueEnqueue(generator, item)
{
    "use strict";

    @assert(@getByIdDirectPrivate(item, "asyncGeneratorQueueItemNext") === null);

    if (@getByIdDirectPrivate(generator, "asyncGeneratorQueueFirst") === null) {
        @assert(@getByIdDirectPrivate(generator, "asyncGeneratorQueueLast") === null);

        @putByIdDirectPrivate(generator, "asyncGeneratorQueueFirst", item);
        @putByIdDirectPrivate(generator, "asyncGeneratorQueueLast", item);
    } else {
        var last = @getByIdDirectPrivate(generator, "asyncGeneratorQueueLast");
        @putByIdDirectPrivate(last, "asyncGeneratorQueueItemNext", item);
        @putByIdDirectPrivate(generator, "asyncGeneratorQueueLast", item);
    }
}

@globalPrivate
function asyncGeneratorQueueDequeue(generator)
{
    "use strict";

    @assert(!@asyncGeneratorQueueIsEmpty(generator), "Async genetator's Queue is an empty List.");

    var result = @getByIdDirectPrivate(generator, "asyncGeneratorQueueFirst");

    var updatedFirst = @getByIdDirectPrivate(result, "asyncGeneratorQueueItemNext");
    @putByIdDirectPrivate(generator, "asyncGeneratorQueueFirst", updatedFirst);

    if (updatedFirst === null)
        @putByIdDirectPrivate(generator, "asyncGeneratorQueueLast", null);

    return result;
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

    var promise = @asyncGeneratorQueueDequeue(generator).promise;
    @assert(@isPromise(promise));
    @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, exception);

    return @asyncGeneratorResumeNext(generator);
}

@globalPrivate
function asyncGeneratorResolve(generator, value, done)
{
    "use strict";

    @assert(typeof @getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") === "number", "Generator is not an AsyncGenerator instance.");

    var promise = @asyncGeneratorQueueDequeue(generator).promise;
    @assert(@isPromise(promise));
    @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, { value, done });

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
}

@globalPrivate
function awaitValue(generator, value, onFulfilled)
{
    "use strict";

    var onRejected = function (result) { @doAsyncGeneratorBodyCall(generator, result, @GeneratorResumeModeThrow); };
    @resolveWithoutPromise(value, onFulfilled, onRejected);
}

@globalPrivate
function doAsyncGeneratorBodyCall(generator, resumeValue, resumeMode)
{
    "use strict";

    var value = @undefined;
    var state = @getByIdDirectPrivate(generator, "generatorState");

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
        var onFulfilled = function(result) { @doAsyncGeneratorBodyCall(generator, result, @GeneratorResumeModeNormal); };

        @awaitValue(generator, value, onFulfilled);
        return;
    }

    if (@getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") === @AsyncGeneratorSuspendReasonYield)
        return @asyncGeneratorYield(generator, value, resumeMode);

    if (@getByIdDirectPrivate(generator, "generatorState") === @AsyncGeneratorStateCompleted) {
        @putByIdDirectPrivate(generator, "asyncGeneratorSuspendReason", @AsyncGeneratorSuspendReasonNone);
        return @asyncGeneratorResolve(generator, value, true);
    }
}

@globalPrivate
function asyncGeneratorResumeNext(generator)
{
    "use strict";

    @assert(typeof @getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") === "number", "Generator is not an AsyncGenerator instance.");

    var state = @getByIdDirectPrivate(generator, "generatorState");

    @assert(state !== @AsyncGeneratorStateExecuting, "Async generator should not be in executing state");

    if (state === @AsyncGeneratorStateAwaitingReturn)
        return;

    if (@asyncGeneratorQueueIsEmpty(generator))
        return;

    var next = @getByIdDirectPrivate(generator, "asyncGeneratorQueueFirst");

    if (next.resumeMode !== @GeneratorResumeModeNormal) {
        if (state === @AsyncGeneratorStateSuspendedStart) {
            @putByIdDirectPrivate(generator, "generatorState", @AsyncGeneratorStateCompleted);
            state = @AsyncGeneratorStateCompleted;
        }

        if (state === @AsyncGeneratorStateCompleted) {
            if (next.resumeMode === @GeneratorResumeModeReturn) {
                @putByIdDirectPrivate(generator, "generatorState", @AsyncGeneratorStateAwaitingReturn);
                @resolveWithoutPromise(next.value,
                    function (result) {
                        @putByIdDirectPrivate(generator, "generatorState", @AsyncGeneratorStateCompleted);
                        @asyncGeneratorResolve(generator, result, true);
                    },
                    function (error) {
                        @putByIdDirectPrivate(generator, "generatorState", @AsyncGeneratorStateCompleted);
                        @asyncGeneratorReject(generator, error);
                    });
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

@globalPrivate
function asyncGeneratorEnqueue(generator, value, resumeMode)
{
    "use strict";

    var promise = @newPromise();
    if (!@isObject(generator) || typeof @getByIdDirectPrivate(generator, "asyncGeneratorSuspendReason") !== 'number') {
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
