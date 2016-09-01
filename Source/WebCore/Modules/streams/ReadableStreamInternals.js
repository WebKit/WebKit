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

function privateInitializeReadableStreamDefaultReader(stream)
{
    "use strict";

    if (!@isReadableStream(stream))
       throw new @TypeError("ReadableStreamDefaultReader needs a ReadableStream");
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

function privateInitializeReadableStreamDefaultController(stream, underlyingSource, size, highWaterMark)
{
    "use strict";

    if (!@isReadableStream(stream))
        throw new @TypeError("ReadableStreamDefaultController needs a ReadableStream");

    // readableStreamController is initialized with null value.
    if (stream.@readableStreamController !== null)
        throw new @TypeError("ReadableStream already has a controller");

    this.@controlledReadableStream = stream;
    this.@underlyingSource = underlyingSource;
    this.@queue = @newQueue();
    this.@started = false;
    this.@closeRequested = false;
    this.@pullAgain = false;
    this.@pulling = false;
    this.@strategy = @validateAndNormalizeQueuingStrategy(size, highWaterMark);

    const controller = this;
    const startResult = @promiseInvokeOrNoopNoCatch(underlyingSource, "start", [this]).@then(() => {
        controller.@started = true;
        @requestReadableStreamPull(controller);
    }, (error) => {
        if (stream.@state === @streamReadable)
            @readableStreamDefaultControllerError(controller, error);
    });

    this.@pull = function() {
        "use strict";

        const stream = controller.@controlledReadableStream;
        if (controller.@queue.content.length) {
            const chunk = @dequeueValue(controller.@queue);
            if (controller.@closeRequested && controller.@queue.content.length === 0)
                @closeReadableStream(stream);
            else
                @requestReadableStreamPull(controller);
            return @Promise.@resolve({value: chunk, done: false});
        }
        const pendingPromise = @readableStreamAddReadRequest(stream);
        @requestReadableStreamPull(controller);
        return pendingPromise;
    }

    return this;
}

function readableStreamDefaultControllerError(controller, error)
{
    "use strict";

    const stream = controller.@controlledReadableStream;
    @assert(stream.@state === @streamReadable);
    controller.@queue = @newQueue();
    @errorReadableStream(stream, error);
}

function teeReadableStream(stream, shouldClone)
{
    "use strict";

    @assert(@isReadableStream(stream));
    @assert(typeof(shouldClone) === "boolean");

    const reader = new @ReadableStreamDefaultReader(stream);

    const teeState = {
        closedOrErrored: false,
        canceled1: false,
        canceled2: false,
        reason1: @undefined,
        reason2: @undefined,
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
        @readableStreamDefaultControllerError(branch1.@readableStreamController, e);
        @readableStreamDefaultControllerError(branch2.@readableStreamController, e);
        teeState.closedOrErrored = true;
    });

    // Additional fields compared to the spec, as they are needed within pull/cancel functions.
    teeState.branch1 = branch1;
    teeState.branch2 = branch2;

    return [branch1, branch2];
}

function doStructuredClone(object)
{
    "use strict";

    // FIXME: We should implement http://w3c.github.io/html/infrastructure.html#ref-for-structured-clone-4
    // Implementation is currently limited to ArrayBuffer/ArrayBufferView to meet Fetch API needs.

    if (object instanceof @ArrayBuffer)
        return @structuredCloneArrayBuffer(object);

    if (@ArrayBuffer.@isView(object))
        return @structuredCloneArrayBufferView(object);

    throw new @TypeError("structuredClone not implemented for: " + object);
}

function teeReadableStreamPullFunction(teeState, reader, shouldClone)
{
    "use strict";

    return function() {
        @Promise.prototype.@then.@call(@readFromReadableStreamDefaultReader(reader), function(result) {
            @assert(@isObject(result));
            @assert(typeof result.done === "boolean");
            if (result.done && !teeState.closedOrErrored) {
                if (!teeState.canceled1)
                    @readableStreamDefaultControllerClose(teeState.branch1.@readableStreamController);
                if (!teeState.canceled2)
                    @readableStreamDefaultControllerClose(teeState.branch2.@readableStreamController);
                teeState.closedOrErrored = true;
            }
            if (teeState.closedOrErrored)
                return;
            if (!teeState.canceled1)
                @enqueueInReadableStream(teeState.branch1.@readableStreamController, shouldClone ? @doStructuredClone(result.value) : result.value);
            if (!teeState.canceled2)
                @enqueueInReadableStream(teeState.branch2.@readableStreamController, shouldClone ? @doStructuredClone(result.value) : result.value);
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

    // Spec tells to return true only if stream has a readableStreamController internal slot.
    // However, since it is a private slot, it cannot be checked using hasOwnProperty().
    // Therefore, readableStreamController is initialized with null value.
    return @isObject(stream) && stream.@readableStreamController !== @undefined;
}

function isReadableStreamDefaultReader(reader)
{
    "use strict";

    // Spec tells to return true only if reader has a readRequests internal slot.
    // However, since it is a private slot, it cannot be checked using hasOwnProperty().
    // Since readRequests is initialized with an empty array, the following test is ok.
    return @isObject(reader) && !!reader.@readRequests;
}

function isReadableStreamDefaultController(controller)
{
    "use strict";

    // Spec tells to return true only if controller has an underlyingSource internal slot.
    // However, since it is a private slot, it cannot be checked using hasOwnProperty().
    // underlyingSource is obtained in ReadableStream constructor: if undefined, it is set
    // to an empty object. Therefore, following test is ok.
    return @isObject(controller) && !!controller.@underlyingSource;
}

function errorReadableStream(stream, error)
{
    "use strict";

    @assert(@isReadableStream(stream));
    @assert(stream.@state === @streamReadable);
    stream.@state = @streamErrored;
    stream.@storedError = error;

    if (!stream.@reader)
        return;

    const reader = stream.@reader;

    if (@isReadableStreamDefaultReader(reader)) {
        const requests = reader.@readRequests;
        for (let index = 0, length = requests.length; index < length; ++index)
            requests[index].@reject.@call(@undefined, error);
        reader.@readRequests = [];
    } else
        // FIXME: Implement ReadableStreamBYOBReader.
        throw new @TypeError("Only ReadableStreamDefaultReader is currently supported");

    reader.@closedPromiseCapability.@reject.@call(@undefined, error);
}

function requestReadableStreamPull(controller)
{
    "use strict";

    const stream = controller.@controlledReadableStream;

    if (stream.@state === @streamClosed || stream.@state === @streamErrored)
        return;
    if (controller.@closeRequested)
        return;
    if (!controller.@started)
        return;
    if ((!@isReadableStreamLocked(stream) || !stream.@reader.@readRequests.length) && @readableStreamDefaultControllerGetDesiredSize(controller) <= 0)
        return;

    if (controller.@pulling) {
        controller.@pullAgain = true;
        return;
    }

    controller.@pulling = true;

    @promiseInvokeOrNoop(controller.@underlyingSource, "pull", [controller]).@then(function() {
        controller.@pulling = false;
        if (controller.@pullAgain) {
            controller.@pullAgain = false;
            @requestReadableStreamPull(controller);
        }
    }, function(error) {
        if (stream.@state === @streamReadable)
            @readableStreamDefaultControllerError(controller, error);
    });
}

function isReadableStreamLocked(stream)
{
   "use strict";

    @assert(@isReadableStream(stream));
    return !!stream.@reader;
}

function readableStreamDefaultControllerGetDesiredSize(controller)
{
   "use strict";

   return controller.@strategy.highWaterMark - controller.@queue.size;
}

function cancelReadableStream(stream, reason)
{
    "use strict";

    stream.@disturbed = true;
    if (stream.@state === @streamClosed)
        return @Promise.@resolve();
    if (stream.@state === @streamErrored)
        return @Promise.@reject(stream.@storedError);
    @closeReadableStream(stream);
    // FIXME: Fix below, which is a temporary solution to the case where controller is undefined.
    // This issue is due to the fact that in previous version of the spec, cancel was associated
    // to underlyingSource, whereas in new version it is associated to controller. As this patch
    // does not yet fully implement the new version, this solution is used.
    const controller = stream.@readableStreamController;
    var underlyingSource = @undefined;
    if (controller !== @undefined)
        underlyingSource = controller.@underlyingSource;
    return @promiseInvokeOrNoop(underlyingSource, "cancel", [reason]).@then(function() { });
}


function readableStreamDefaultControllerClose(controller)
{
    "use strict";

    const stream = controller.@controlledReadableStream;
    @assert(!controller.@closeRequested);
    @assert(stream.@state === @streamReadable);
    controller.@closeRequested = true;
    if (controller.@queue.content.length === 0)
        @closeReadableStream(stream);
}

function closeReadableStream(stream)
{
    "use strict";

    @assert(stream.@state === @streamReadable);
    stream.@state = @streamClosed;
    const reader = stream.@reader;

    if (!reader)
        return;

    if (@isReadableStreamDefaultReader(reader)) {
        const requests = reader.@readRequests;
        for (let index = 0, length = requests.length; index < length; ++index)
            requests[index].@resolve.@call(@undefined, {value:@undefined, done: true});
        reader.@readRequests = [];
    }

    reader.@closedPromiseCapability.@resolve.@call();
}

function enqueueInReadableStream(controller, chunk)
{
    "use strict";

    const stream = controller.@controlledReadableStream;
    @assert(!controller.@closeRequested);
    @assert(stream.@state === @streamReadable);

    if (@isReadableStreamLocked(stream) && stream.@reader.@readRequests.length) {
        stream.@reader.@readRequests.@shift().@resolve.@call(@undefined, {value: chunk, done: false});
        @requestReadableStreamPull(controller);
        return;
    }

    try {
        let chunkSize = 1;
        if (controller.@strategy.size !== @undefined)
            chunkSize = controller.@strategy.size(chunk);
        @enqueueValueWithSize(controller.@queue, chunk, chunkSize);
    }
    catch(error) {
        if (stream.@state === @streamReadable)
            @readableStreamDefaultControllerError(controller, error);
        throw error;
    }
    @requestReadableStreamPull(controller);
}

function readFromReadableStreamDefaultReader(reader)
{
    "use strict";

    const stream = reader.@ownerReadableStream;
    @assert(!!stream);

    // Native sources may want to start enqueueing at the time of the first read request.
    if (!stream.@disturbed && stream.@state === @streamReadable && stream.@readableStreamController.@underlyingSource.@firstReadCallback)
        stream.@readableStreamController.@underlyingSource.@firstReadCallback();

    stream.@disturbed = true;
    if (stream.@state === @streamClosed)
        return @Promise.@resolve({value: @undefined, done: true});
    if (stream.@state === @streamErrored)
        return @Promise.@reject(stream.@storedError);
    @assert(stream.@state === @streamReadable);

    return stream.@readableStreamController.@pull();
}

function readableStreamAddReadRequest(stream)
{
    "use strict";

    @assert(@isReadableStreamDefaultReader(stream.@reader));
    @assert(stream.@state == @streamReadable);

    const readRequest = @newPromiseCapability(@Promise);
    stream.@reader.@readRequests.@push(readRequest);

    return readRequest.@promise;
}

function isReadableStreamDisturbed(stream)
{
    "use strict";

    @assert(@isReadableStream(stream));
    return stream.@disturbed;
}
