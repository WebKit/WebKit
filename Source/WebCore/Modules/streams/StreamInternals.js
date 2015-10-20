/*
 * Copyright (C) 2015 Canon Inc.
 * Copyright (C) 2015 Igalia.
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

// @optional=STREAMS_API
// @internals

function invokeOrNoop(object, key, args)
{
    "use strict";

    var method = object[key];
    if (typeof method === "undefined")
        return;
    return method.@apply(object, args);
}

function promiseInvokeOrNoop(object, key, args)
{
    "use strict";

    try {
        var method = object[key];
        if (typeof method === "undefined")
            return Promise.resolve();
        var result = method.@apply(object, args);
        return Promise.resolve(result);
    }
    catch(error) {
        return Promise.reject(error);
    }

}

function validateAndNormalizeQueuingStrategy(size, highWaterMark)
{
    "use strict";

    if (size !== undefined && typeof size !== "function")
        throw new @TypeError("size parameter must be a function");

    var normalizedStrategy = { };

    normalizedStrategy.size = size;
    normalizedStrategy.highWaterMark = Number(highWaterMark);

    if (Number.isNaN(normalizedStrategy.highWaterMark))
        throw new @TypeError("highWaterMark parameter is not a number");
    if (normalizedStrategy.highWaterMark < 0)
        throw new @RangeError("highWaterMark is negative");

    return normalizedStrategy;
}

function createNewStreamsPromise()
{
    "use strict";

    var resolveFunction;
    var rejectFunction;
    var promise = new Promise(function(resolve, reject) {
        resolveFunction = resolve;
        rejectFunction = reject;
    });
    promise.@resolve = resolveFunction;
    promise.@reject = rejectFunction;
    return promise;
}

function resolveStreamsPromise(promise, value)
{
    "use strict";

    if (promise && promise.@resolve) {
        promise.@resolve(value);
        promise.@resolve = undefined;
        promise.@reject = undefined;
    }
}

function rejectStreamsPromise(promise, value)
{
    "use strict";

    if (promise && promise.@reject) {
        promise.@reject(value);
        promise.@resolve = undefined;
        promise.@reject = undefined;
    }
}

function newQueue()
{
    return { content: [], size: 0 };
}

function dequeueValue(queue)
{
    "use strict";

    var record = queue.content.shift();
    queue.size -= record.size;
    return record.value;
}

function enqueueValueWithSize(queue, value, size)
{
    size = Number(size);
    if (Number.isNaN(size) || !Number.isFinite(size) || size < 0)
        throw new @RangeError("size has an incorrect value");
    queue.content.push({ value: value, size: size });
    queue.size += size;

    return undefined;
}
