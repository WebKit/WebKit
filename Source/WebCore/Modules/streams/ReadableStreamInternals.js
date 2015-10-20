/*
 * Copyright (C) 2015 Canon Inc. All rights reserved.
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
        this.@closedPromise = @createNewStreamsPromise();
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

function teeReadableStream(stream, shouldClone)
{
    "use strict";

    // TODO: Assert: IsReadableStream(stream) is true.
    // TODO: Assert: Type(shouldClone) is Boolean.

    let reader = stream.getReader();

    let teeState = {
        closedOrErrored: false,
        canceled1: false,
        canceled2: false,
        reason1: undefined,
        reason: undefined,
    };

    teeState.cancelPromise = new @InternalPromise(function(resolve, reject) {
       teeState.resolvePromise = resolve;
       teeState.rejectPromise = reject;
    });

    let pullFunction = @teeReadableStreamPullFunction(teeState, reader, shouldClone);

    let underlyingSource1 = {
        "pull": pullFunction,
        "cancel": @teeReadableStreamBranch1CancelFunction(teeState, stream)
    };

    let underlyingSource2 = {
        "pull": pullFunction,
        "cancel": @teeReadableStreamBranch2CancelFunction(teeState, stream)
    };

    let branch1 = new ReadableStream(underlyingSource1);
    let branch2 = new ReadableStream(underlyingSource2);

    reader.closed.catch(function(e) {
        if (teeState.closedOrErrored)
            return;
        @errorReadableStream(branch1, e);
        @errorReadableStream(branch2, e);
        teeState.closedOrErrored = true;
    });

    // Additional fields compared to the spec, as they are needed within pull/cancel functions.
    teeState.branch1 = branch1;
    teeState.branch2 = branch2;

    return [branch1, branch2];
}

function teeReadableStreamPullFunction(teeState, reader, shouldClone)
{
    return function() {
        reader.read().then(function(result) {
            if (result.done && !teeState.closedOrErrored) {
                @closeReadableStream(teeState.branch1);
                @closeReadableStream(teeState.branch2);
                teeState.closedOrErrored = true;
            }
            if (teeState.closedOrErrored)
                return;
            if (!teeState.canceled1) {
                // TODO: Implement cloning if shouldClone is true
                @enqueueInReadableStream(teeState.branch1, result.value);
            }
            if (!teeState.canceled2) {
                // TODO: Implement cloning if shouldClone is true
                @enqueueInReadableStream(teeState.branch2, result.value);
            }
        });
    }
}

function teeReadableStreamBranch1CancelFunction(teeState, stream)
{
    return function(r) {
        teeState.canceled1 = true;
        teeState.reason1 = r;
        if (teeState.canceled2) {
            @cancelReadableStream(stream, [teeState.reason1, teeState.reason2]).then(teeState.resolvePromise, teeState.rejectPromise);
        }
        return teeState.cancelPromise;
    }
}

function teeReadableStreamBranch2CancelFunction(teeState, stream)
{
    return function(r) {
        teeState.canceled2 = true;
        teeState.reason2 = r;
        if (teeState.canceled1) {
            @cancelReadableStream(stream, [teeState.reason1, teeState.reason2]).then(teeState.resolvePromise, teeState.rejectPromise);
        }
        return teeState.cancelPromise;
    }
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
    stream.@queue = @newQueue();
    stream.@storedError = error;
    stream.@state = @readableStreamErrored;

    if (!stream.@reader)
        return;
    var reader = stream.@reader;

    var requests = reader.@readRequests;
    for (var index = 0, length = requests.length; index < length; ++index)
        @rejectStreamsPromise(requests[index], error);
    reader.@readRequests = [];

    @releaseReadableStreamReader(reader);
    reader.@storedError = error;
    reader.@state = @readableStreamErrored;

    @rejectStreamsPromise(reader.@closedPromise, error);
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

   return stream.@strategy.highWaterMark - stream.@queue.size;
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
    stream.@queue = @newQueue();
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
    if (!stream.@queue.content.length)
        @finishClosingReadableStream(stream);
}

function closeReadableStreamReader(reader)
{
    "use strict";

    var requests = reader.@readRequests;
    for (var index = 0, length = requests.length; index < length; ++index)
        @resolveStreamsPromise(requests[index], {value:undefined, done: true});
    reader.@readRequests = [];
    @releaseReadableStreamReader(reader);
    reader.@state = @readableStreamClosed;
    @resolveStreamsPromise(reader.@closedPromise);
}

function enqueueInReadableStream(stream, chunk)
{
    "use strict";

    // TODO: ASSERT(!stream.@closeRequested);
    // TODO: ASSERT(stream.@state !== @readableStreamErrored);
    if (stream.@state === @readableStreamClosed)
        return undefined;
    if (@isReadableStreamLocked(stream) && stream.@reader.@readRequests.length) {
        @resolveStreamsPromise(stream.@reader.@readRequests.shift(), {value: chunk, done: false});
        @requestReadableStreamPull(stream);
        return;
    }
    try {
        var size = 1;
        if (stream.@strategy.size) {
            size = Number(stream.@strategy.size(chunk));
            if (Number.isNaN(size) || size === +Infinity || size < 0)
                throw new RangeError("Chunk size is not valid");
        }
        @enqueueValueWithSize(stream.@queue, chunk, size);
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
    if (stream.@queue.content.length) {
        var chunk = @dequeueValue(stream.@queue);
        if (!stream.@closeRequested)
            @requestReadableStreamPull(stream);
        else if (!stream.@queue.content.length)
            @finishClosingReadableStream(stream);
        return Promise.resolve({value: chunk, done: false});
    }
    var readPromise = @createNewStreamsPromise();
    reader.@readRequests.push(readPromise);
    @requestReadableStreamPull(stream);
    return readPromise;
}
