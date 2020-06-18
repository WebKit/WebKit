/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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

function catch(onRejected)
{
    "use strict";

    return this.then(@undefined, onRejected);
}

function then(onFulfilled, onRejected)
{
    "use strict";

    if (!@isPromise(this))
        @throwTypeError("|this| is not a Promise");

    var constructor = @speciesConstructor(this, @Promise);

    var promise;
    var promiseOrCapability;
    if (constructor === @Promise) {
        promiseOrCapability = @newPromise();
        promise = promiseOrCapability;
    } else {
        promiseOrCapability = @newPromiseCapabilitySlow(constructor);
        promise = promiseOrCapability.@promise;
    }

    if (typeof onFulfilled !== "function")
        onFulfilled = function (argument) { return argument; };

    if (typeof onRejected !== "function")
        onRejected = function (argument) { throw argument; };

    var reaction = @newPromiseReaction(promiseOrCapability, onFulfilled, onRejected);

    var flags = @getPromiseInternalField(this, @promiseFieldFlags);
    var state = flags & @promiseStateMask;
    if (state === @promiseStatePending) {
        reaction.@next = @getPromiseInternalField(this, @promiseFieldReactionsOrResult);
        @putPromiseInternalField(this, @promiseFieldReactionsOrResult, reaction);
    } else {
        if (state === @promiseStateRejected && !(flags & @promiseFlagsIsHandled))
            @hostPromiseRejectionTracker(this, @promiseRejectionHandle);
        @enqueueJob(@promiseReactionJob, state, reaction, @getPromiseInternalField(this, @promiseFieldReactionsOrResult));
    }
    @putPromiseInternalField(this, @promiseFieldFlags, @getPromiseInternalField(this, @promiseFieldFlags) | @promiseFlagsIsHandled);

    return promise;
}

function finally(onFinally)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("|this| is not an object");

    var constructor = @speciesConstructor(this, @Promise);

    @assert(@isConstructor(constructor));

    var thenFinally;
    var catchFinally;

    if (typeof onFinally !== "function") {
        thenFinally = onFinally;
        catchFinally = onFinally;
    } else {
        thenFinally = @getThenFinally(onFinally, constructor);
        catchFinally = @getCatchFinally(onFinally, constructor);
    }

    return this.then(thenFinally, catchFinally);
}

@globalPrivate
function getThenFinally(onFinally, constructor)
{
    "use strict";

    return (value) =>
    {
        @assert(typeof onFinally === "function");
        var result = onFinally();

        @assert(@isConstructor(constructor));
        var resultCapability = @newPromiseCapability(constructor);

        resultCapability.@resolve.@call(@undefined, result);

        var promise = resultCapability.@promise;
        return promise.then(() => value);
    }
}

@globalPrivate
function getCatchFinally(onFinally, constructor)
{
    "use strict";

    return (reason) =>
    {
        @assert(typeof onFinally === "function");
        var result = onFinally();

        @assert(@isConstructor(constructor));
        var resultCapability = @newPromiseCapability(constructor);

        resultCapability.@resolve.@call(@undefined, result);

        var promise = resultCapability.@promise;
        return promise.then(() => { throw reason; });
    }
}
