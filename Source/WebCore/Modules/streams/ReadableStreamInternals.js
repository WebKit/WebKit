/*
 * Copyright (C) 2015 Canon Inc. All rights reserved.
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

function privateInitializeReadableStreamReader(stream)
{
    "use strict";

    if (!@isReadableStream(stream))
       throw new @TypeError("ReadableStreamReader needs a ReadableStream");
    if (@isReadableStreamLocked(stream))
       throw new @TypeError("ReadableStream is locked");

    this.@state = stream.@state;
    this.@readRequests = [];
    if (stream.@state === @readableStreamReadable) {
        this.@ownerReadableStream = stream;
        this.@storedError = undefined;
        stream.@reader = this;
        this.@closedPromise = new Promise(function(resolve, reject) {
            stream.@reader.@closedPromiseResolve = resolve;
            stream.@reader.@closedPromiseReject = reject;
        });
        return this;
    }
    if (stream.@state === @readableStreamClosed) {
        this.@ownerReadableStream = null;
        this.@storedError = undefined;
        this.@closedPromise = Promise.resolve();
        return this;
    }
    // TODO: ASSERT(stream.@state === @readableStreamErrored);
    this.@ownerReadableStream = null;
    this.@storedError = stream.@storedError;
    this.@closedPromise = Promise.reject(stream.@storedError);

    return this;
}

function privateInitializeReadableStreamController(stream)
{
    "use strict";

    if (!@isReadableStream(stream))
        throw new @TypeError("ReadableStreamController needs a ReadableStream");
    if (typeof stream.@controller !== "undefined")
        throw new @TypeError("ReadableStream already has a controller");
    this.@controlledReadableStream = stream;

    return this;
}

// @optional=STREAMS_API
// @internals

function teeReadableStream(stream, shouldClone)
{
    "use strict";

    throw new @TypeError("tee is not implemented");
}

function isReadableStream(stream)
{
    "use strict";

    return @isObject(stream) && !!stream.@underlyingSource;
}

function isReadableStreamReader(reader)
{
    "use strict";

    return @isObject(reader) && typeof reader.@ownerReadableStream !== "undefined";
}

function isReadableStreamController(controller)
{
    "use strict";

    return @isObject(controller) && !!controller.@controlledReadableStream;
}

function errorReadableStream(stream, error)
{
    "use strict";

    // TODO: ASSERT(stream.@state === @readableStreamReadable);
    stream.@queue = [];
    stream.@storedError = error;
    stream.@state = @readableStreamErrored;

    if (!stream.@reader)
        return;
    var reader = stream.@reader;

    var requests = reader.@readRequests;
    for (var index = 0, length = requests.length; index < length; ++index)
        requests[index].reject(error);
    reader.@readRequests = [];

    @releaseReadableStreamReader(reader);
    reader.@storedError = error;
    reader.@state = @readableStreamErrored;

    reader.@closedPromiseReject(error);
}

function requestReadableStreamPull(stream)
{
    "use strict";

    if (stream.@state !== @readableStreamReadable)
        return;
    if (stream.@closeRequested)
        return;
    if (!stream.@started)
        return;
    if ((!@isReadableStreamLocked(stream) || !stream.@reader.@readRequests.length) && @getReadableStreamDesiredSize(stream) <= 0)
        return;
 
    if (stream.@pulling) {
        stream.@pullAgain = true;
        return;
    }

    stream.@pulling = true;

    var promise = @promiseInvokeOrNoop(stream.@underlyingSource, "pull", [stream.@controller]);
    promise.then(function() {
        stream.@pulling = false;
        if (stream.@pullAgain) {
            stream.@pullAgain = false;
            @requestReadableStreamPull(stream);
        }
    }, function(error) {
        @errorReadableStream(stream, error);
    });
}

function isReadableStreamLocked(stream)
{
   "use strict";

    return !!stream.@reader;
}

function getReadableStreamDesiredSize(stream)
{
   "use strict";

   return stream.@highWaterMark - stream.@queueSize;
}

function releaseReadableStreamReader(reader)
{
    "use strict";

    reader.@ownerReadableStream.@reader = undefined;
    reader.@ownerReadableStream = null;
}

function cancelReadableStream(stream, reason)
{
    "use strict";

    if (stream.@state === @readableStreamClosed)
        return Promise.resolve();
    if (stream.@state === @readableStreamErrored)
        return Promise.reject(stream.@storedError);
    stream.@queue = [];
    @finishClosingReadableStream(stream);
    return @promiseInvokeOrNoop(stream.@underlyingSource, "cancel", [reason]).then(function() { });
}

function finishClosingReadableStream(stream)
{
    "use strict";

    // TODO: ASSERT(stream.@state ===  @readableStreamReadable);
    stream.@state = @readableStreamClosed;
    var reader = stream.@reader;
    if (reader)
        @closeReadableStreamReader(reader);
}

function closeReadableStream(stream)
{
    "use strict";

    // TODO. ASSERT(!stream.@closeRequested);
    // TODO: ASSERT(stream.@state !== @readableStreamErrored);
    if (stream.@state === @readableStreamClosed)
        return; 
    stream.@closeRequested = true;
    if (!stream.@queue.length)
        @finishClosingReadableStream(stream);
}

function closeReadableStreamReader(reader)
{
    "use strict";

    var requests = reader.@readRequests;
    for (var index = 0, length = requests.length; index < length; ++index)
        requests[index].resolve({value:undefined, done: true});
    reader.@readRequests = [];
    @releaseReadableStreamReader(reader);
    reader.@state = @readableStreamClosed;
    reader.@closedPromiseResolve();
}

function enqueueInReadableStream(stream, chunk)
{
    "use strict";

    // TODO: ASSERT(!stream.@closeRequested);
    // TODO: ASSERT(stream.@state !== @readableStreamErrored);
    if (stream.@state === @readableStreamClosed)
        return undefined;
    if (@isReadableStreamLocked(stream) && stream.@reader.@readRequests.length) {
        stream.@reader.@readRequests.shift().resolve({value: chunk, done: false});
        @requestReadableStreamPull(stream);
        return;
    }
    try {
        var size = 1;
        if (stream.@strategySize) {
            size = Number(stream.@strategySize(chunk));
            if (Number.isNaN(size) || size === +Infinity || size < 0)
                throw new RangeError("Chunk size is not valid");
        }
        stream.@queue.push({ value: chunk, size: size });
        stream.@queueSize += size;
    }
    catch(error) {
        @errorReadableStream(stream, error);
        throw error;
    }
    @requestReadableStreamPull(stream);
}

function readFromReadableStreamReader(reader)
{
    "use strict";

    if (reader.@state === @readableStreamClosed)
        return Promise.resolve({value: undefined, done: true});
    if (reader.@state === @readableStreamErrored)
        return Promise.reject(reader.@storedError);
    // TODO: ASSERT(!!reader.@ownerReadableStream);
    // TODO: ASSERT(reader.@ownerReadableStream.@state === @readableStreamReadable);
    var stream = reader.@ownerReadableStream;
    if (stream.@queue.length) {
        var chunk = stream.@queue.shift();
        stream.@queueSize -= chunk.size;
        if (!stream.@closeRequested)
            @requestReadableStreamPull(stream);
        else if (!stream.@queue.length)
            @finishClosingReadableStream(stream);
        return Promise.resolve({value: chunk.value, done: false});
    }
    var readRequest = {};
    var readPromise = new Promise(function(resolve, reject) {
        readRequest.resolve = resolve;
        readRequest.reject = reject;
    });
    reader.@readRequests.push(readRequest);
    @requestReadableStreamPull(stream);
    return readPromise;
}

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
