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

// @conditional=ENABLE(STREAMS_API)
// @internal

function privateInitializeReadableStreamReader(stream)
{
    "use strict";

    if (!@isReadableStream(stream))
       throw new @TypeError("ReadableStreamReader needs a ReadableStream");
    if (@isReadableStreamLocked(stream))
       throw new @TypeError("ReadableStream is locked");

    this.@state = stream.@state;
    this.@readRequests = [];
    if (stream.@state === @streamReadable) {
        this.@ownerReadableStream = stream;
        this.@storedError = undefined;
        stream.@reader = this;
        this.@closedPromiseCapability = @newPromiseCapability(@Promise);
        return this;
    }
    if (stream.@state === @streamClosed) {
        this.@ownerReadableStream = null;
        this.@storedError = undefined;
        this.@closedPromiseCapability = { @promise: @Promise.@resolve() };
        return this;
    }
    // FIXME: ASSERT(stream.@state === @streamErrored);
    this.@ownerReadableStream = null;
    this.@storedError = stream.@storedError;
    this.@closedPromiseCapability = { @promise: @Promise.@reject(stream.@storedError) };

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

    teeState.cancelPromiseCapability = @newPromiseCapability(@InternalPromise);

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
            @cancelReadableStream(stream, [teeState.reason1, teeState.reason2]).then(teeState.cancelPromiseCapability.@resolve,
                                                                                     teeState.cancelPromiseCapability.@reject);
        }
        return teeState.cancelPromiseCapability.@promise;
    }
}

function teeReadableStreamBranch2CancelFunction(teeState, stream)
{
    return function(r) {
        teeState.canceled2 = true;
        teeState.reason2 = r;
        if (teeState.canceled1) {
            @cancelReadableStream(stream, [teeState.reason1, teeState.reason2]).then(teeState.cancelPromiseCapability.@resolve,
                                                                                     teeState.cancelPromiseCapability.@reject);
        }
        return teeState.cancelPromiseCapability.@promise;
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

    // FIXME: ASSERT(stream.@state === @streamReadable);
    stream.@queue = @newQueue();
    stream.@storedError = error;
    stream.@state = @streamErrored;

    if (!stream.@reader)
        return;
    var reader = stream.@reader;

    var requests = reader.@readRequests;
    for (var index = 0, length = requests.length; index < length; ++index)
        requests[index].@reject.@call(undefined, error);
    reader.@readRequests = [];

    @releaseReadableStreamReader(reader);
    reader.@storedError = error;
    reader.@state = @streamErrored;

    reader.@closedPromiseCapability.@reject.@call(undefined, error);
}

function requestReadableStreamPull(stream)
{
    "use strict";

    if (stream.@state !== @streamReadable)
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

    if (stream.@state === @streamClosed)
        return @Promise.@resolve();
    if (stream.@state === @streamErrored)
        return @Promise.@reject(stream.@storedError);
    stream.@queue = @newQueue();
    @finishClosingReadableStream(stream);
    return @promiseInvokeOrNoop(stream.@underlyingSource, "cancel", [reason]).then(function() { });
}

function finishClosingReadableStream(stream)
{
    "use strict";

    // FIXME: ASSERT(stream.@state ===  @streamReadable);
    stream.@state = @streamClosed;
    var reader = stream.@reader;
    if (reader)
        @closeReadableStreamReader(reader);
}

function closeReadableStream(stream)
{
    "use strict";

    // FIXME: ASSERT(!stream.@closeRequested);
    // FIXME: ASSERT(stream.@state !== @streamErrored);
    if (stream.@state === @streamClosed)
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
        requests[index].@resolve.@call(undefined, {value:undefined, done: true});
    reader.@readRequests = [];
    @releaseReadableStreamReader(reader);
    reader.@state = @streamClosed;
    reader.@closedPromiseCapability.@resolve.@call(undefined);
}

function enqueueInReadableStream(stream, chunk)
{
    "use strict";

    // FIXME: ASSERT(!stream.@closeRequested);
    // FIXME: ASSERT(stream.@state !== @streamErrored);
    if (stream.@state === @streamClosed)
        return undefined;
    if (@isReadableStreamLocked(stream) && stream.@reader.@readRequests.length) {
        stream.@reader.@readRequests.shift().@resolve.@call(undefined, {value: chunk, done: false});
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

    if (reader.@state === @streamClosed)
        return @Promise.@resolve({value: undefined, done: true});
    if (reader.@state === @streamErrored)
        return @Promise.@reject(reader.@storedError);
    // FIXME: ASSERT(!!reader.@ownerReadableStream);
    // FIXME: ASSERT(reader.@ownerReadableStream.@state === @streamReadable);
    var stream = reader.@ownerReadableStream;
    if (stream.@queue.content.length) {
        var chunk = @dequeueValue(stream.@queue);
        if (!stream.@closeRequested)
            @requestReadableStreamPull(stream);
        else if (!stream.@queue.content.length)
            @finishClosingReadableStream(stream);
        return @Promise.@resolve({value: chunk, done: false});
    }
    var readPromiseCapability = @newPromiseCapability(@Promise);
    reader.@readRequests.push(readPromiseCapability);
    @requestReadableStreamPull(stream);
    return readPromiseCapability.@promise;
}
