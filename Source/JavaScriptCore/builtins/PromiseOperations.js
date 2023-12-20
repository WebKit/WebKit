/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

// @internal

@linkTimeConstant
function pushNewPromiseReaction(thenable, existingReactions, promiseOrCapability, onFulfilled, onRejected, context)
{
    "use strict";

    if (!existingReactions) {
        existingReactions = {
            @promiseOrCapability: promiseOrCapability,
            @onFulfilled: onFulfilled,
            @onRejected: onRejected,
            @context: context,
            // This is 4x the number of out of line reactions (promise, fulfill callback, reject callback, context).
            @outOfLineReactionCounts: 0,
        };
        @putPromiseInternalField(thenable, @promiseFieldReactionsOrResult, existingReactions);
    } else {
        var outOfLineReactionCounts = existingReactions.@outOfLineReactionCounts;
        @putByValDirect(existingReactions, outOfLineReactionCounts++, promiseOrCapability);
        @putByValDirect(existingReactions, outOfLineReactionCounts++, onFulfilled);
        @putByValDirect(existingReactions, outOfLineReactionCounts++, onRejected);
        @putByValDirect(existingReactions, outOfLineReactionCounts++, context);
        existingReactions.@outOfLineReactionCounts = outOfLineReactionCounts;
    }
}

@linkTimeConstant
function newPromiseCapabilitySlow(constructor)
{
    "use strict";

    var promiseCapability = {
        resolve: @undefined,
        reject: @undefined,
        promise: @undefined,
    };

    var promise = new constructor((resolve, reject) => {
        if (promiseCapability.resolve !== @undefined)
            @throwTypeError("resolve function is already set");
        if (promiseCapability.reject !== @undefined)
            @throwTypeError("reject function is already set");

        promiseCapability.resolve = resolve;
        promiseCapability.reject = reject;
    });

    if (!@isCallable(promiseCapability.resolve))
        @throwTypeError("executor did not take a resolve function");

    if (!@isCallable(promiseCapability.reject))
        @throwTypeError("executor did not take a reject function");

    promiseCapability.promise = promise;

    return promiseCapability;
}

@linkTimeConstant
function newPromiseCapability(constructor)
{
    "use strict";

    if (constructor === @Promise) {
        var promise = @newPromise();
        var capturedPromise = promise;
        function @resolve(resolution) {
            return @resolvePromiseWithFirstResolvingFunctionCallCheck(capturedPromise, resolution);
        }
        function @reject(reason) {
            return @rejectPromiseWithFirstResolvingFunctionCallCheck(capturedPromise, reason);
        }
        return { resolve: @resolve, reject: @reject, promise };
    }

    return @newPromiseCapabilitySlow(constructor);
}

@linkTimeConstant
function promiseResolve(constructor, value)
{
    "use strict";

    if (@isPromise(value) && value.constructor === constructor)
        return value;

    if (constructor === @Promise) {
        var promise = @newPromise();
        @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, value);
        return promise;
    }

    return @promiseResolveSlow(constructor, value);
}

@linkTimeConstant
function promiseResolveSlow(constructor, value)
{
    "use strict";

    @assert(constructor !== @Promise);
    var promiseCapability = @newPromiseCapabilitySlow(constructor);
    promiseCapability.resolve.@call(@undefined, value);
    return promiseCapability.promise;
}

@linkTimeConstant
function promiseRejectSlow(constructor, reason)
{
    "use strict";

    @assert(constructor !== @Promise);
    var promiseCapability = @newPromiseCapabilitySlow(constructor);
    promiseCapability.reject.@call(@undefined, reason);
    return promiseCapability.promise;
}

@linkTimeConstant
function newHandledRejectedPromise(error)
{
    "use strict";
    var promise = @newPromise();
    @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, error);
    @putPromiseInternalField(promise, @promiseFieldFlags, @getPromiseInternalField(promise, @promiseFieldFlags) | @promiseFlagsIsHandled);
    return promise;
}

@linkTimeConstant
function triggerPromiseReactions(state, reactions, argument)
{
    "use strict";

    if (!reactions)
        return;

    var isResolved = state === @promiseStateFulfilled;
    var handler = isResolved ? reactions.@onFulfilled : reactions.@onRejected;

    @enqueueJob(@promiseReactionJob, reactions.@promiseOrCapability, handler, argument, handler ? reactions.@context : state);

    for (var i = 0, count = reactions.@outOfLineReactionCounts; i < count; i += 4) {
        var promise = reactions[i];
        var handler = isResolved ? reactions[i + 1] : reactions[i + 2];
        var context = reactions[i + 3];
        @enqueueJob(@promiseReactionJob, promise, handler, argument, handler ? context : state);
    }
    @assert(i === count);
}

@linkTimeConstant
function resolvePromise(promise, resolution)
{
    "use strict";

    @assert(@isPromise(promise));

    if (resolution === promise)
        return @rejectPromise(promise, @makeTypeError("Cannot resolve a promise with itself"));

    if (!@isObject(resolution))
        return @fulfillPromise(promise, resolution);

    var then;
    try {
        then = resolution.then;
    } catch (error) {
        return @rejectPromise(promise, error);
    }

    if (@isPromise(resolution) && then === @defaultPromiseThen) {
        @enqueueJob(@promiseResolveThenableJobFast, resolution, promise);
        return;
    }

    if (!@isCallable(then))
        return @fulfillPromise(promise, resolution);

    @enqueueJob(@promiseResolveThenableJob, resolution, then, @createResolvingFunctions(promise));
}

// Keep in sync with JSPromise::rejectedPromise.
@linkTimeConstant
function rejectPromise(promise, reason)
{
    "use strict";

    @assert(@isPromise(promise));
    @assert((@getPromiseInternalField(promise, @promiseFieldFlags) & @promiseStateMask) == @promiseStatePending);

    var flags = @getPromiseInternalField(promise, @promiseFieldFlags);
    var reactions = @getPromiseInternalField(promise, @promiseFieldReactionsOrResult);
    @putPromiseInternalField(promise, @promiseFieldReactionsOrResult, reason);
    @putPromiseInternalField(promise, @promiseFieldFlags, flags | @promiseStateRejected);

    if (!(flags & @promiseFlagsIsHandled))
        @hostPromiseRejectionTracker(promise, @promiseRejectionReject);

    @triggerPromiseReactions(@promiseStateRejected, reactions, reason);
}

@linkTimeConstant
function fulfillPromise(promise, value)
{
    "use strict";

    @assert(@isPromise(promise));
    @assert((@getPromiseInternalField(promise, @promiseFieldFlags) & @promiseStateMask) == @promiseStatePending);

    var flags = @getPromiseInternalField(promise, @promiseFieldFlags);
    var reactions = @getPromiseInternalField(promise, @promiseFieldReactionsOrResult);
    @putPromiseInternalField(promise, @promiseFieldReactionsOrResult, value);
    @putPromiseInternalField(promise, @promiseFieldFlags, flags | @promiseStateFulfilled);

    @triggerPromiseReactions(@promiseStateFulfilled, reactions, value);
}

@linkTimeConstant
function resolvePromiseWithFirstResolvingFunctionCallCheck(promise, value)
{
    "use strict";

    @assert(@isPromise(promise));
    var flags = @getPromiseInternalField(promise, @promiseFieldFlags);
    if (flags & @promiseFlagsIsFirstResolvingFunctionCalled)
        return;
    @putPromiseInternalField(promise, @promiseFieldFlags, flags | @promiseFlagsIsFirstResolvingFunctionCalled);
    return @resolvePromise(promise, value);
}

@linkTimeConstant
function fulfillPromiseWithFirstResolvingFunctionCallCheck(promise, value)
{
    "use strict";

    @assert(@isPromise(promise));
    var flags = @getPromiseInternalField(promise, @promiseFieldFlags);
    if (flags & @promiseFlagsIsFirstResolvingFunctionCalled)
        return;
    @putPromiseInternalField(promise, @promiseFieldFlags, flags | @promiseFlagsIsFirstResolvingFunctionCalled);
    return @fulfillPromise(promise, value);
}

@linkTimeConstant
function rejectPromiseWithFirstResolvingFunctionCallCheck(promise, reason)
{
    "use strict";

    @assert(@isPromise(promise));
    var flags = @getPromiseInternalField(promise, @promiseFieldFlags);
    if (flags & @promiseFlagsIsFirstResolvingFunctionCalled)
        return;
    @putPromiseInternalField(promise, @promiseFieldFlags, flags | @promiseFlagsIsFirstResolvingFunctionCalled);
    return @rejectPromise(promise, reason);
}

@linkTimeConstant
function createResolvingFunctions(promise)
{
    "use strict";
    @assert(@isPromise(promise));

    var alreadyResolved = false;

    var resolve = (0, /* prevent function name inference */ (resolution) => {
        if (alreadyResolved)
            return @undefined;
        alreadyResolved = true;

        return @resolvePromise(promise, resolution);
    });

    var reject = (0, /* prevent function name inference */ (reason) => {
        if (alreadyResolved)
            return @undefined;
        alreadyResolved = true;

        return @rejectPromise(promise, reason);
    });

    return { resolve, reject };
}

@linkTimeConstant
function promiseReactionJobWithoutPromise(handler, argument, context)
{
    "use strict";

    try {
        if (context)
            handler(argument, context);
        else
            handler(argument);
    } catch {
        // This is user-uncatchable promise. We just ignore the error here.
    }
}

// This function has strong guarantee that each handler function (onFulfilled and onRejected) will be called at most once.
@linkTimeConstant
function resolveWithoutPromise(resolution, onFulfilled, onRejected, context)
{
    "use strict";

    if (!@isObject(resolution)) {
        @fulfillWithoutPromise(resolution, onFulfilled, onRejected, context);
        return;
    }

    var then;
    try {
        then = resolution.then;
    } catch (error) {
        @rejectWithoutPromise(error, onFulfilled, onRejected, context);
        return;
    }

    if (@isPromise(resolution) && then === @defaultPromiseThen) {
        @enqueueJob(@promiseResolveThenableJobWithoutPromiseFast, resolution, onFulfilled, onRejected, context);
        return;
    }

    if (!@isCallable(then)) {
        @fulfillWithoutPromise(resolution, onFulfilled, onRejected, context);
        return;
    }

    // Wrap onFulfilled and onRejected with @createResolvingFunctionsWithoutPromise to ensure that each function will be called at most once.
    @enqueueJob(@promiseResolveThenableJob, resolution, then, @createResolvingFunctionsWithoutPromise(onFulfilled, onRejected, context));
}

// This function has strong guarantee that each handler function (onFulfilled and onRejected) will be called at most once.
@linkTimeConstant
function rejectWithoutPromise(reason, onFulfilled, onRejected, context)
{
    "use strict";

    @enqueueJob(@promiseReactionJobWithoutPromise, onRejected, reason, context);
}

// This function has strong guarantee that each handler function (onFulfilled and onRejected) will be called at most once.
@linkTimeConstant
function fulfillWithoutPromise(value, onFulfilled, onRejected, context)
{
    "use strict";

    @enqueueJob(@promiseReactionJobWithoutPromise, onFulfilled, value, context);
}

// This function has strong guarantee that each handler function (onFulfilled and onRejected) will be called at most once.
// This is special version of resolveWithoutPromise which skips resolution's then handling.
// https://github.com/tc39/ecma262/pull/1250
@linkTimeConstant
function resolveWithoutPromiseForAsyncAwait(resolution, onFulfilled, onRejected, context)
{
    "use strict";

    if (@isPromise(resolution)) {
        try {
            var { constructor } = resolution;
        } catch (error) {
            onRejected(error, context);
            return;
        }

        if (constructor === @Promise || constructor === @InternalPromise)
            return @performPromiseThen(resolution, onFulfilled, onRejected, @undefined, context);
    }

    return @resolveWithoutPromise(resolution, onFulfilled, onRejected, context);
}

@linkTimeConstant
function createResolvingFunctionsWithoutPromise(onFulfilled, onRejected, context)
{
    "use strict";

    var alreadyResolved = false;

    var resolve = (0, /* prevent function name inference */ (resolution) => {
        if (alreadyResolved)
            return @undefined;
        alreadyResolved = true;

        @resolveWithoutPromise(resolution, onFulfilled, onRejected, context);
    });

    var reject = (0, /* prevent function name inference */ (reason) => {
        if (alreadyResolved)
            return @undefined;
        alreadyResolved = true;

        @rejectWithoutPromise(reason, onFulfilled, onRejected, context);
    });

    return { resolve, reject };
}

@linkTimeConstant
function promiseReactionJob(promiseOrCapability, handler, argument, contextOrState)
{
    // Promise Reaction has four types.
    // 1. @promiseOrCapability is PromiseCapability, and having handlers.
    //     The most generic one.
    // 2. @promiseOrCapability is Promise, and having handlers.
    //     We just have promise.
    // 3. @promiseOrCapability is Promise, and not having handlers.
    //     It only has promise. Just resolving it with the value. contextOrState is undefined, so it delivers state instead.
    // 4. Only having @onFulfilled and @onRejected
    //     It does not have promise capability. Just handlers are passed.
    "use strict";

    // Case (3).
    if (@isUndefinedOrNull(handler)) {
        try {
            @assert(@isPromise(promiseOrCapability));
            if (contextOrState === @promiseStateFulfilled)
                @resolvePromise(promiseOrCapability, argument);
            else
                @rejectPromise(promiseOrCapability, argument);
        } catch {
            // This is user-uncatchable promise. We just ignore the error here.
        }
        return;
    }

    // Case (4).
    if (!promiseOrCapability) {
        @promiseReactionJobWithoutPromise(handler, argument, contextOrState);
        return;
    }

    // Case (1), or (2).
    try {
        var result = (contextOrState) ? handler(argument, contextOrState) : handler(argument);
    } catch (error) {
        if (@isPromise(promiseOrCapability)) {
            @rejectPromise(promiseOrCapability, error);
            return;
        }
        promiseOrCapability.reject.@call(@undefined, error);
        return;
    }

    if (@isPromise(promiseOrCapability)) {
        @resolvePromise(promiseOrCapability, result);
        return;
    }
    promiseOrCapability.resolve.@call(@undefined, result);
}

@linkTimeConstant
function promiseResolveThenableJobFast(thenable, promiseToResolve)
{
    "use strict";

    @assert(@isPromise(thenable));
    @assert(@isPromise(promiseToResolve));

    // Even if we are using @defaultPromiseThen, still thenable.constructor access is observable, and if it is not returning @Promise,
    // we need to call this constructor.
    var constructor = @speciesConstructor(thenable, @Promise);
    if (constructor !== @Promise && constructor !== @InternalPromise) {
        @promiseResolveThenableJobWithDerivedPromise(thenable, constructor, @createResolvingFunctions(promiseToResolve));
        return;
    }

    var flags = @getPromiseInternalField(thenable, @promiseFieldFlags);
    var state = flags & @promiseStateMask;
    var reactionsOrResult = @getPromiseInternalField(thenable, @promiseFieldReactionsOrResult);
    if (state === @promiseStatePending)
        @pushNewPromiseReaction(thenable, reactionsOrResult, promiseToResolve, @undefined, @undefined);
    else {
        if (state === @promiseStateRejected && !(flags & @promiseFlagsIsHandled))
            @hostPromiseRejectionTracker(thenable, @promiseRejectionHandle);
        @enqueueJob(@promiseReactionJob, promiseToResolve, @undefined, reactionsOrResult, state);
    }
    @putPromiseInternalField(thenable, @promiseFieldFlags, @getPromiseInternalField(thenable, @promiseFieldFlags) | @promiseFlagsIsHandled);
}

@linkTimeConstant
function promiseResolveThenableJobWithoutPromiseFast(thenable, onFulfilled, onRejected, context)
{
    "use strict";

    @assert(@isPromise(thenable));

    // Even if we are using @defaultPromiseThen, still thenable.constructor access is observable, and if it is not returning @Promise,
    // we need to call this constructor.
    var constructor = @speciesConstructor(thenable, @Promise);
    if (constructor !== @Promise && constructor !== @InternalPromise) {
        @promiseResolveThenableJobWithDerivedPromise(thenable, constructor, @createResolvingFunctionsWithoutPromise(onFulfilled, onRejected, context));
        return;
    }

    var flags = @getPromiseInternalField(thenable, @promiseFieldFlags);
    var state = flags & @promiseStateMask;
    var reactionsOrResult = @getPromiseInternalField(thenable, @promiseFieldReactionsOrResult);
    if (state === @promiseStatePending)
        @pushNewPromiseReaction(thenable, reactionsOrResult, @undefined, onFulfilled, onRejected, context);
    else {
        if (state === @promiseStateRejected) {
            if (!(flags & @promiseFlagsIsHandled))
                @hostPromiseRejectionTracker(thenable, @promiseRejectionHandle);
            @rejectWithoutPromise(reactionsOrResult, onFulfilled, onRejected, context);
        } else
            @fulfillWithoutPromise(reactionsOrResult, onFulfilled, onRejected, context);
    }
    @putPromiseInternalField(thenable, @promiseFieldFlags, @getPromiseInternalField(thenable, @promiseFieldFlags) | @promiseFlagsIsHandled);
}

@linkTimeConstant
function promiseResolveThenableJob(thenable, then, resolvingFunctions)
{
    "use strict";

    try {
        return then.@call(thenable, resolvingFunctions.resolve, resolvingFunctions.reject);
    } catch (error) {
        return resolvingFunctions.reject.@call(@undefined, error);
    }
}

@linkTimeConstant
function promiseResolveThenableJobWithDerivedPromise(thenable, constructor, resolvingFunctions)
{
    "use strict";

    try {
        var promiseOrCapability = @newPromiseCapabilitySlow(constructor);
        @performPromiseThen(thenable, resolvingFunctions.resolve, resolvingFunctions.reject, promiseOrCapability, @undefined);
        return promiseOrCapability.promise;
    } catch (error) {
        return resolvingFunctions.reject.@call(@undefined, error);
    }
}

@linkTimeConstant
function promiseEmptyOnFulfilled(argument)
{
    "use strict";

    return argument;
}

@linkTimeConstant
function promiseEmptyOnRejected(argument)
{
    "use strict";

    throw argument;
}

@linkTimeConstant
function performPromiseThen(promise, onFulfilled, onRejected, promiseOrCapability, context)
{
    "use strict";

    if (!@isCallable(onFulfilled))
        onFulfilled = @promiseEmptyOnFulfilled;

    if (!@isCallable(onRejected))
        onRejected = @promiseEmptyOnRejected;

    var reactionsOrResult = @getPromiseInternalField(promise, @promiseFieldReactionsOrResult);
    var flags = @getPromiseInternalField(promise, @promiseFieldFlags);
    var state = flags & @promiseStateMask;
    if (state === @promiseStatePending)
        @pushNewPromiseReaction(promise, reactionsOrResult, promiseOrCapability, onFulfilled, onRejected, context);
    else {
        var handler;

        if (state === @promiseStateRejected) {
            handler = onRejected;
            if (!(flags & @promiseFlagsIsHandled))
                @hostPromiseRejectionTracker(promise, @promiseRejectionHandle);
        } else
            handler = onFulfilled;
        @enqueueJob(@promiseReactionJob, promiseOrCapability, handler, reactionsOrResult, context);
    }
    @putPromiseInternalField(promise, @promiseFieldFlags, @getPromiseInternalField(promise, @promiseFieldFlags) | @promiseFlagsIsHandled);
}
