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

    this.@readRequests = [];
    this.@ownerReadableStream = stream;
    stream.@reader = this;
    if (stream.@state === @streamReadable) {
        this.@closedPromiseCapability = @newPromiseCapability(@Promise);
        return this;
    }
    if (stream.@state === @streamClosed) {
        this.@closedPromiseCapability = { @promise: @Promise.@resolve() };
        return this;
    }
    @assert(stream.@state === @streamErrored);
    this.@closedPromiseCapability = { @promise: @Promise.@reject(stream.@storedError) };

    return this;
}

function privateInitializeReadableStreamController(stream)
{
    "use strict";

    if (!@isReadableStream(stream))
        throw new @TypeError("ReadableStreamController needs a ReadableStream");
    if (stream.@controller !== @undefined)
        throw new @TypeError("ReadableStream already has a controller");
    this.@controlledReadableStream = stream;

    return this;
}

function teeReadableStream(stream, shouldClone)
{
    "use strict";

    @assert(@isReadableStream(stream));
    @assert(typeof(shouldClone) === "boolean");

    const reader = new @ReadableStreamReader(stream);

    const teeState = {
        closedOrErrored: false,
        canceled1: false,
        canceled2: false,
        reason1: @undefined,
        reason: @undefined,
    };

    teeState.cancelPromiseCapability = @newPromiseCapability(@InternalPromise);

    const pullFunction = @teeReadableStreamPullFunction(teeState, reader, shouldClone);

    const branch1 = new @ReadableStream({
        "pull": pullFunction,
        "cancel": @teeReadableStreamBranch1CancelFunction(teeState, stream)
    });
    const branch2 = new @ReadableStream({
        "pull": pullFunction,
        "cancel": @teeReadableStreamBranch2CancelFunction(teeState, stream)
    });

    reader.@closedPromiseCapability.@promise.@then(@undefined, function(e) {
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
    "use strict";

    return function() {
        @Promise.prototype.@then.@call(@readFromReadableStreamReader(reader), function(result) {
            @assert(@isObject(result));
            @assert(typeof result.done === "boolean");
            if (result.done && !teeState.closedOrErrored) {
                @closeReadableStream(teeState.branch1);
                @closeReadableStream(teeState.branch2);
                teeState.closedOrErrored = true;
            }
            if (teeState.closedOrErrored)
                return;
            if (!teeState.canceled1) {
                // FIXME: Implement cloning if shouldClone is true
                @enqueueInReadableStream(teeState.branch1, result.value);
            }
            if (!teeState.canceled2) {
                // FIXME: Implement cloning if shouldClone is true
                @enqueueInReadableStream(teeState.branch2, result.value);
            }
        });
    }
}

function teeReadableStreamBranch1CancelFunction(teeState, stream)
{
    "use strict";

    return function(r) {
        teeState.canceled1 = true;
        teeState.reason1 = r;
        if (teeState.canceled2) {
            @cancelReadableStream(stream, [teeState.reason1, teeState.reason2]).@then(
                teeState.cancelPromiseCapability.@resolve,
                teeState.cancelPromiseCapability.@reject);
        }
        return teeState.cancelPromiseCapability.@promise;
    }
}

function teeReadableStreamBranch2CancelFunction(teeState, stream)
{
    "use strict";

    return function(r) {
        teeState.canceled2 = true;
        teeState.reason2 = r;
        if (teeState.canceled1) {
            @cancelReadableStream(stream, [teeState.reason1, teeState.reason2]).@then(
                teeState.cancelPromiseCapability.@resolve,
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

    // To reset @ownerReadableStream it must be set to null instead of undefined because there is no way to distinguish
    // between a non-existent slot and an slot set to undefined.
    return @isObject(reader) && reader.@ownerReadableStream !== @undefined;
}

function isReadableStreamController(controller)
{
    "use strict";

    return @isObject(controller) && !!controller.@controlledReadableStream;
}

function errorReadableStream(stream, error)
{
    "use strict";

    @assert(stream.@state === @streamReadable);
    stream.@queue = @newQueue();
    stream.@storedError = error;
    stream.@state = @streamErrored;

    if (!stream.@reader)
        return;
    const reader = stream.@reader;

    const requests = reader.@readRequests;
    for (let index = 0, length = requests.length; index < length; ++index)
        requests[index].@reject.@call(@undefined, error);
    reader.@readRequests = [];

    reader.@closedPromiseCapability.@reject.@call(@undefined, error);
}

function requestReadableStreamPull(stream)
{
    "use strict";

    if (stream.@state === @streamClosed || stream.@state === @streamErrored)
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

    @promiseInvokeOrNoop(stream.@underlyingSource, "pull", [stream.@controller]).@then(function() {
        stream.@pulling = false;
        if (stream.@pullAgain) {
            stream.@pullAgain = false;
            @requestReadableStreamPull(stream);
        }
    }, function(error) {
        if (stream.@state === @streamReadable)
            @errorReadableStream(stream, error);
    });
}

function isReadableStreamLocked(stream)
{
   "use strict";

    @assert(@isReadableStream(stream));
    return !!stream.@reader;
}

function getReadableStreamDesiredSize(stream)
{
   "use strict";

   return stream.@strategy.highWaterMark - stream.@queue.size;
}

function cancelReadableStream(stream, reason)
{
    "use strict";

    stream.@disturbed = true;
    if (stream.@state === @streamClosed)
        return @Promise.@resolve();
    if (stream.@state === @streamErrored)
        return @Promise.@reject(stream.@storedError);
    stream.@queue = @newQueue();
    @finishClosingReadableStream(stream);
    return @promiseInvokeOrNoop(stream.@underlyingSource, "cancel", [reason]).@then(function() { });
}

function finishClosingReadableStream(stream)
{
    "use strict";

    @assert(stream.@state ===  @streamReadable);
    stream.@state = @streamClosed;
    const reader = stream.@reader;
    if (!reader)
        return;

    const requests = reader.@readRequests;
    for (let index = 0, length = requests.length; index < length; ++index)
        requests[index].@resolve.@call(@undefined, {value:@undefined, done: true});
    reader.@readRequests = [];
    reader.@closedPromiseCapability.@resolve.@call();
}

function closeReadableStream(stream)
{
    "use strict";

    @assert(!stream.@closeRequested);
    @assert(stream.@state !== @streamErrored);
    if (stream.@state === @streamClosed)
        return; 
    stream.@closeRequested = true;
    if (!stream.@queue.content.length)
        @finishClosingReadableStream(stream);
}

function enqueueInReadableStream(stream, chunk)
{
    "use strict";

    @assert(!stream.@closeRequested);
    @assert(stream.@state !== @streamErrored);
    if (stream.@state === @streamClosed)
        return;
    if (@isReadableStreamLocked(stream) && stream.@reader.@readRequests.length) {
        stream.@reader.@readRequests.@shift().@resolve.@call(@undefined, {value: chunk, done: false});
        @requestReadableStreamPull(stream);
        return;
    }
    try {
        let size = 1;
        if (stream.@strategy.size) {
            size = @Number(stream.@strategy.size(chunk));
            if (!@isFinite(size) || size < 0)
                throw new @RangeError("Chunk size is not valid");
        }
        @enqueueValueWithSize(stream.@queue, chunk, size);
    }
    catch(error) {
        if (stream.@state === @streamReadable)
            @errorReadableStream(stream, error);
        throw error;
    }
    @requestReadableStreamPull(stream);
}

function readFromReadableStreamReader(reader)
{
    "use strict";

    const stream = reader.@ownerReadableStream;
    @assert(!!stream);
    stream.@disturbed = true;
    if (stream.@state === @streamClosed)
        return @Promise.@resolve({value: @undefined, done: true});
    if (stream.@state === @streamErrored)
        return @Promise.@reject(stream.@storedError);
    @assert(stream.@state === @streamReadable);
    if (stream.@queue.content.length) {
        const chunk = @dequeueValue(stream.@queue);
        if (stream.@closeRequested && stream.@queue.content.length === 0)
            @finishClosingReadableStream(stream);
        else
            @requestReadableStreamPull(stream);
        return @Promise.@resolve({value: chunk, done: false});
    }
    const readPromiseCapability = @newPromiseCapability(@Promise);
    reader.@readRequests.@push(readPromiseCapability);
    @requestReadableStreamPull(stream);
    return readPromiseCapability.@promise;
}

function isReadableStreamDisturbed(stream)
{
    "use strict";

    @assert(@isReadableStream(stream));
    return stream.@disturbed;
}
