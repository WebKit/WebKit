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

@globalPrivate
function newPromiseReaction(promiseOrCapability, onFulfilled, onRejected)
{
    "use strict";

    return {
        @promiseOrCapability: promiseOrCapability,
        @onFulfilled: onFulfilled,
        @onRejected: onRejected,
        @next: @undefined,
    };
}

@globalPrivate
function newPromiseCapabilitySlow(constructor)
{
    var promiseCapability = {
        @resolve: @undefined,
        @reject: @undefined,
        @promise: @undefined,
    };

    if (!@isConstructor(constructor))
        @throwTypeError("promise capability requires a constructor function");

    var promise = new constructor(function (resolve, reject) {
        if (promiseCapability.@resolve !== @undefined)
            @throwTypeError("resolve function is already set");
        if (promiseCapability.@reject !== @undefined)
            @throwTypeError("reject function is already set");

        promiseCapability.@resolve = resolve;
        promiseCapability.@reject = reject;
    });

    if (typeof promiseCapability.@resolve !== "function")
        @throwTypeError("executor did not take a resolve function");

    if (typeof promiseCapability.@reject !== "function")
        @throwTypeError("executor did not take a reject function");

    promiseCapability.@promise = promise;

    return promiseCapability;
}

@globalPrivate
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
        return { @resolve, @reject, @promise: promise };
    }

    return @newPromiseCapabilitySlow(constructor);
}

@globalPrivate
function promiseResolveSlow(constructor, value)
{
    @assert(constructor !== @Promise);
    var promiseCapability = @newPromiseCapabilitySlow(constructor);
    promiseCapability.@resolve.@call(@undefined, value);
    return promiseCapability.@promise;
}

@globalPrivate
function promiseRejectSlow(constructor, reason)
{
    @assert(constructor !== @Promise);
    var promiseCapability = @newPromiseCapabilitySlow(constructor);
    promiseCapability.@reject.@call(@undefined, reason);
    return promiseCapability.@promise;
}

@globalPrivate
function newHandledRejectedPromise(error)
{
    "use strict";
    var promise = @newPromise();
    @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, error);
    @putPromiseInternalField(promise, @promiseFieldFlags, @getPromiseInternalField(promise, @promiseFieldFlags) | @promiseFlagsIsHandled);
    return promise;
}

@globalPrivate
function triggerPromiseReactions(state, reactions, argument)
{
    "use strict";

    // Reverse the order of singly-linked-list.
    var previous = @undefined;
    var current = reactions;
    while (current) {
        var next = current.@next;
        current.@next = previous;
        previous = current;
        current = next;
    }
    reactions = previous;

    current = reactions;
    while (current) {
        @enqueueJob(@promiseReactionJob, state, current, argument);
        current = current.@next;
    }
}

@globalPrivate
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

    if (typeof then !== 'function')
        return @fulfillPromise(promise, resolution);

    @enqueueJob(@promiseResolveThenableJob, resolution, then, @createResolvingFunctions(promise));
}

@globalPrivate
function rejectPromise(promise, reason)
{
    "use strict";

    @assert(@isPromise(promise));
    @assert((@getPromiseInternalField(promise, @promiseFieldFlags) & @promiseStateMask) == @promiseStatePending);

    var flags = @getPromiseInternalField(promise, @promiseFieldFlags);
    var reactions = @getPromiseInternalField(promise, @promiseFieldReactionsOrResult);
    @putPromiseInternalField(promise, @promiseFieldReactionsOrResult, reason);
    @putPromiseInternalField(promise, @promiseFieldFlags, flags | @promiseStateRejected);

    @InspectorInstrumentation.promiseRejected(promise, reason, reactions);

    if (!(flags & @promiseFlagsIsHandled))
        @hostPromiseRejectionTracker(promise, @promiseRejectionReject);

    @triggerPromiseReactions(@promiseStateRejected, reactions, reason);
}

@globalPrivate
function fulfillPromise(promise, value)
{
    "use strict";

    @assert(@isPromise(promise));
    @assert((@getPromiseInternalField(promise, @promiseFieldFlags) & @promiseStateMask) == @promiseStatePending);

    var flags = @getPromiseInternalField(promise, @promiseFieldFlags);
    var reactions = @getPromiseInternalField(promise, @promiseFieldReactionsOrResult);
    @putPromiseInternalField(promise, @promiseFieldReactionsOrResult, value);
    @putPromiseInternalField(promise, @promiseFieldFlags, flags | @promiseStateFulfilled);

    @InspectorInstrumentation.promiseFulfilled(promise, value, reactions);

    @triggerPromiseReactions(@promiseStateFulfilled, reactions, value);
}

@globalPrivate
function resolvePromiseWithFirstResolvingFunctionCallCheck(promise, value)
{
    @assert(@isPromise(promise));
    var flags = @getPromiseInternalField(promise, @promiseFieldFlags);
    if (flags & @promiseFlagsIsFirstResolvingFunctionCalled)
        return;
    @putPromiseInternalField(promise, @promiseFieldFlags, flags | @promiseFlagsIsFirstResolvingFunctionCalled);
    return @resolvePromise(promise, value);
}

@globalPrivate
function rejectPromiseWithFirstResolvingFunctionCallCheck(promise, reason)
{
    @assert(@isPromise(promise));
    var flags = @getPromiseInternalField(promise, @promiseFieldFlags);
    if (flags & @promiseFlagsIsFirstResolvingFunctionCalled)
        return;
    @putPromiseInternalField(promise, @promiseFieldFlags, flags | @promiseFlagsIsFirstResolvingFunctionCalled);
    return @rejectPromise(promise, reason);
}

@globalPrivate
function createResolvingFunctions(promise)
{
    "use strict";

    @assert(@isPromise(promise));

    var alreadyResolved = false;

    function @resolve(resolution) {
        if (alreadyResolved)
            return @undefined;
        alreadyResolved = true;

        return @resolvePromise(promise, resolution);
    }

    function @reject(reason) {
        if (alreadyResolved)
            return @undefined;
        alreadyResolved = true;

        return @rejectPromise(promise, reason);
    }

    return { @resolve, @reject };
}

@globalPrivate
function promiseReactionJobWithoutPromise(handler, argument)
{
    "use strict";

    try {
        handler(argument);
    } catch {
        // This is user-uncatchable promise. We just ignore the error here.
    }
}

// This function has strong guarantee that each handler function (onFulfilled and onRejected) will be called at most once.
@globalPrivate
function resolveWithoutPromise(resolution, onFulfilled, onRejected)
{
    "use strict";

    if (!@isObject(resolution)) {
        @fulfillWithoutPromise(resolution, onFulfilled, onRejected);
        return;
    }

    var then;
    try {
        then = resolution.then;
    } catch (error) {
        @rejectWithoutPromise(error, onFulfilled, onRejected);
        return;
    }

    if (@isPromise(resolution) && then === @defaultPromiseThen) {
        @enqueueJob(@promiseResolveThenableJobWithoutPromiseFast, resolution, onFulfilled, onRejected);
        return;
    }

    if (typeof then !== 'function') {
        @fulfillWithoutPromise(resolution, onFulfilled, onRejected);
        return;
    }

    // Wrap onFulfilled and onRejected with @createResolvingFunctionsWithoutPromise to ensure that each function will be called at most once.
    @enqueueJob(@promiseResolveThenableJob, resolution, then, @createResolvingFunctionsWithoutPromise(onFulfilled, onRejected));
}

// This function has strong guarantee that each handler function (onFulfilled and onRejected) will be called at most once.
@globalPrivate
function rejectWithoutPromise(reason, onFulfilled, onRejected)
{
    "use strict";

    @enqueueJob(@promiseReactionJobWithoutPromise, onRejected, reason);
}

// This function has strong guarantee that each handler function (onFulfilled and onRejected) will be called at most once.
@globalPrivate
function fulfillWithoutPromise(value, onFulfilled, onRejected)
{
    "use strict";

    @enqueueJob(@promiseReactionJobWithoutPromise, onFulfilled, value);
}

@globalPrivate
function createResolvingFunctionsWithoutPromise(onFulfilled, onRejected)
{
    "use strict";

    var alreadyResolved = false;

    function @resolve(resolution) {
        if (alreadyResolved)
            return @undefined;
        alreadyResolved = true;

        @resolveWithoutPromise(resolution, onFulfilled, onRejected);
    }

    function @reject(reason) {
        if (alreadyResolved)
            return @undefined;
        alreadyResolved = true;

        @rejectWithoutPromise(reason, onFulfilled, onRejected);
    }

    return { @resolve, @reject };
}

@globalPrivate
function promiseReactionJob(state, reaction, argument)
{
    // Promise Reaction has four types.
    // 1. @promiseOrCapability is PromiseCapability, and having handlers.
    //     The most generic one.
    // 2. @promiseOrCapability is Promise, and having handlers.
    //     We just have promise.
    // 3. @promiseOrCapability is Promise, and not having handlers.
    //     It only has promise. Just resolving it with the value.
    // 4. Only having @onFulfilled and @onRejected
    //     It does not have promise capability. Just handlers are passed.
    "use strict";

    var promiseOrCapability = reaction.@promiseOrCapability;

    // Case (3).
    if (!reaction.@onRejected) {
        @assert(!reaction.@onFulfilled);
        try {
            @assert(@isPromise(promiseOrCapability));
            if (state === @promiseStateFulfilled)
                @resolvePromise(promiseOrCapability, argument);
            else
                @rejectPromise(promiseOrCapability, argument);
        } catch {
            // This is user-uncatchable promise. We just ignore the error here.
        }
        return;
    }

    var handler = (state === @promiseStateFulfilled) ? reaction.@onFulfilled: reaction.@onRejected;

    // Case (4).
    if (!promiseOrCapability) {
        @promiseReactionJobWithoutPromise(handler, argument);
        return;
    }

    // Case (1), or (2).
    var result;
    try {
        result = handler(argument);
    } catch (error) {
        if (@isPromise(promiseOrCapability)) {
            @rejectPromise(promiseOrCapability, error);
            return;
        }
        promiseOrCapability.@reject.@call(@undefined, error);
        return;
    }

    if (@isPromise(promiseOrCapability)) {
        @resolvePromise(promiseOrCapability, result);
        return;
    }
    promiseOrCapability.@resolve.@call(@undefined, result);
}

@globalPrivate
function promiseResolveThenableJobFast(thenable, promiseToResolve)
{
    "use strict";

    @assert(@isPromise(thenable));
    @assert(@isPromise(promiseToResolve));

    var flags = @getPromiseInternalField(thenable, @promiseFieldFlags);
    var state = flags & @promiseStateMask;
    var reaction = @newPromiseReaction(promiseToResolve, @undefined, @undefined);
    if (state === @promiseStatePending) {
        reaction.@next = @getPromiseInternalField(thenable, @promiseFieldReactionsOrResult);
        @putPromiseInternalField(thenable, @promiseFieldReactionsOrResult, reaction);
    } else {
        if (state === @promiseStateRejected && !(flags & @promiseFlagsIsHandled))
            @hostPromiseRejectionTracker(thenable, @promiseRejectionHandle);
        @enqueueJob(@promiseReactionJob, state, reaction, @getPromiseInternalField(thenable, @promiseFieldReactionsOrResult));
    }
    @putPromiseInternalField(thenable, @promiseFieldFlags, @getPromiseInternalField(thenable, @promiseFieldFlags) | @promiseFlagsIsHandled);
}

@globalPrivate
function promiseResolveThenableJobWithoutPromiseFast(thenable, onFulfilled, onRejected)
{
    "use strict";

    @assert(@isPromise(thenable));

    var flags = @getPromiseInternalField(thenable, @promiseFieldFlags);
    var state = flags & @promiseStateMask;
    if (state === @promiseStatePending) {
        var reaction = @newPromiseReaction(@undefined, onFulfilled, onRejected);
        reaction.@next = @getPromiseInternalField(thenable, @promiseFieldReactionsOrResult);
        @putPromiseInternalField(thenable, @promiseFieldReactionsOrResult, reaction);
    } else {
        var result = @getPromiseInternalField(thenable, @promiseFieldReactionsOrResult);
        if (state === @promiseStateRejected) {
            if (!(flags & @promiseFlagsIsHandled))
                @hostPromiseRejectionTracker(thenable, @promiseRejectionHandle);
            @rejectWithoutPromise(result, onFulfilled, onRejected);
        } else
            @fulfillWithoutPromise(result, onFulfilled, onRejected);
    }
    @putPromiseInternalField(thenable, @promiseFieldFlags, @getPromiseInternalField(thenable, @promiseFieldFlags) | @promiseFlagsIsHandled);
}

@globalPrivate
function promiseResolveThenableJob(thenable, then, resolvingFunctions)
{
    "use strict";

    try {
        return then.@call(thenable, resolvingFunctions.@resolve, resolvingFunctions.@reject);
    } catch (error) {
        return resolvingFunctions.@reject.@call(@undefined, error);
    }
}
