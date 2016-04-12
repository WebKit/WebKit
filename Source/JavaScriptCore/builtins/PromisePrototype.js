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
        throw new @TypeError("|this| is not a object");

    var constructor = @speciesConstructor(this, @Promise);

    var resultCapability = @newPromiseCapability(constructor);

    if (typeof onFulfilled !== "function")
        onFulfilled = function (argument) { return argument; };

    if (typeof onRejected !== "function")
        onRejected = function (argument) { throw argument; };

    var fulfillReaction = @newPromiseReaction(resultCapability, onFulfilled);
    var rejectReaction = @newPromiseReaction(resultCapability, onRejected);

    var state = this.@promiseState;

    if (state === @promiseStatePending) {
        @putByValDirect(this.@promiseFulfillReactions, this.@promiseFulfillReactions.length, fulfillReaction)
        @putByValDirect(this.@promiseRejectReactions, this.@promiseRejectReactions.length, rejectReaction)
    } else if (state === @promiseStateFulfilled)
        @enqueueJob(@promiseReactionJob, [fulfillReaction, this.@promiseResult]);
    else if (state === @promiseStateRejected)
        @enqueueJob(@promiseReactionJob, [rejectReaction, this.@promiseResult]);

    return resultCapability.@promise;
}
