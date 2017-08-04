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
       @throwTypeError("ReadableStreamDefaultReader needs a ReadableStream");
    if (@isReadableStreamLocked(stream))
       @throwTypeError("ReadableStream is locked");

    @readableStreamReaderGenericInitialize(this, stream);
    this.@readRequests = [];

    return this;
}

function readableStreamReaderGenericInitialize(reader, stream)
{
    "use strict";

    reader.@ownerReadableStream = stream;
    stream.@reader = reader;
    if (stream.@state === @streamReadable)
        reader.@closedPromiseCapability = @newPromiseCapability(@Promise);
    else if (stream.@state === @streamClosed)
        reader.@closedPromiseCapability = { @promise: @Promise.@resolve() };
    else {
        @assert(stream.@state === @streamErrored);
        reader.@closedPromiseCapability = { @promise: @newHandledRejectedPromise(stream.@storedError) };
    }
}

function privateInitializeReadableStreamDefaultController(stream, underlyingSource, size, highWaterMark)
{
    "use strict";

    if (!@isReadableStream(stream))
        @throwTypeError("ReadableStreamDefaultController needs a ReadableStream");

    // readableStreamController is initialized with null value.
    if (stream.@readableStreamController !== null)
        @throwTypeError("ReadableStream already has a controller");

    this.@controlledReadableStream = stream;
    this.@underlyingSource = underlyingSource;
    this.@queue = @newQueue();
    this.@started = false;
    this.@closeRequested = false;
    this.@pullAgain = false;
    this.@pulling = false;
    this.@strategy = @validateAndNormalizeQueuingStrategy(size, highWaterMark);

    const controller = this;
    @promiseInvokeOrNoopNoCatch(underlyingSource, "start", [this]).@then(() => {
        controller.@started = true;
        @assert(!controller.@pulling);
        @assert(!controller.@pullAgain);
        @readableStreamDefaultControllerCallPullIfNeeded(controller);
    }, (error) => {
        if (stream.@state === @streamReadable)
            @readableStreamDefaultControllerError(controller, error);
    });

    this.@cancel = @readableStreamDefaultControllerCancel;

    this.@pull = @readableStreamDefaultControllerPull;

    return this;
}

function readableStreamDefaultControllerError(controller, error)
{
    "use strict";

    const stream = controller.@controlledReadableStream;
    @assert(stream.@state === @streamReadable);
    controller.@queue = @newQueue();
    @readableStreamError(stream, error);
}

function readableStreamTee(stream, shouldClone)
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

    const pullFunction = @readableStreamTeePullFunction(teeState, reader, shouldClone);

    const branch1 = new @ReadableStream({
        "pull": pullFunction,
        "cancel": @readableStreamTeeBranch1CancelFunction(teeState, stream)
    });
    const branch2 = new @ReadableStream({
        "pull": pullFunction,
        "cancel": @readableStreamTeeBranch2CancelFunction(teeState, stream)
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

    @throwTypeError("structuredClone not implemented for: " + object);
}

function readableStreamTeePullFunction(teeState, reader, shouldClone)
{
    "use strict";

    return function() {
        @Promise.prototype.@then.@call(@readableStreamDefaultReaderRead(reader), function(result) {
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
                @readableStreamDefaultControllerEnqueue(teeState.branch1.@readableStreamController, result.value);
            if (!teeState.canceled2)
                @readableStreamDefaultControllerEnqueue(teeState.branch2.@readableStreamController, shouldClone ? @doStructuredClone(result.value) : result.value);
        });
    }
}

function readableStreamTeeBranch1CancelFunction(teeState, stream)
{
    "use strict";

    return function(r) {
        teeState.canceled1 = true;
        teeState.reason1 = r;
        if (teeState.canceled2) {
            @readableStreamCancel(stream, [teeState.reason1, teeState.reason2]).@then(
                teeState.cancelPromiseCapability.@resolve,
                teeState.cancelPromiseCapability.@reject);
        }
        return teeState.cancelPromiseCapability.@promise;
    }
}

function readableStreamTeeBranch2CancelFunction(teeState, stream)
{
    "use strict";

    return function(r) {
        teeState.canceled2 = true;
        teeState.reason2 = r;
        if (teeState.canceled1) {
            @readableStreamCancel(stream, [teeState.reason1, teeState.reason2]).@then(
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

function readableStreamError(stream, error)
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
    } else {
        @assert(@isReadableStreamBYOBReader(reader));
        const requests = reader.@readIntoRequests;
        for (let index = 0, length = requests.length; index < length; ++index)
            requests[index].@reject.@call(@undefined, error);
        reader.@readIntoRequests = [];
    }

    reader.@closedPromiseCapability.@reject.@call(@undefined, error);
    reader.@closedPromiseCapability.@promise.@promiseIsHandled = true;
}

function readableStreamDefaultControllerCallPullIfNeeded(controller)
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

    @assert(!controller.@pullAgain);
    controller.@pulling = true;

    @promiseInvokeOrNoop(controller.@underlyingSource, "pull", [controller]).@then(function() {
        controller.@pulling = false;
        if (controller.@pullAgain) {
            controller.@pullAgain = false;
            @readableStreamDefaultControllerCallPullIfNeeded(controller);
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

   const stream = controller.@controlledReadableStream;

   if (stream.@state === @streamErrored)
       return null;
   if (stream.@state === @streamClosed)
       return 0;

   return controller.@strategy.highWaterMark - controller.@queue.size;
}


function readableStreamReaderGenericCancel(reader, reason)
{
    "use strict";

    const stream = reader.@ownerReadableStream;
    @assert(!!stream);
    return @readableStreamCancel(stream, reason);
}

function readableStreamCancel(stream, reason)
{
    "use strict";

    stream.@disturbed = true;
    if (stream.@state === @streamClosed)
        return @Promise.@resolve();
    if (stream.@state === @streamErrored)
        return @Promise.@reject(stream.@storedError);
    @readableStreamClose(stream);
    return stream.@readableStreamController.@cancel(stream.@readableStreamController, reason).@then(function() {  });
}

function readableStreamDefaultControllerCancel(controller, reason)
{
    "use strict";

    controller.@queue = @newQueue();
    return @promiseInvokeOrNoop(controller.@underlyingSource, "cancel", [reason]);
}

function readableStreamDefaultControllerPull(controller)
{
    "use strict";

    const stream = controller.@controlledReadableStream;
    if (controller.@queue.content.length) {
        const chunk = @dequeueValue(controller.@queue);
        if (controller.@closeRequested && controller.@queue.content.length === 0)
            @readableStreamClose(stream);
        else
            @readableStreamDefaultControllerCallPullIfNeeded(controller);
        return @Promise.@resolve({value: chunk, done: false});
    }
    const pendingPromise = @readableStreamAddReadRequest(stream);
    @readableStreamDefaultControllerCallPullIfNeeded(controller);
    return pendingPromise;
}

function readableStreamDefaultControllerClose(controller)
{
    "use strict";

    const stream = controller.@controlledReadableStream;
    @assert(!controller.@closeRequested);
    @assert(stream.@state === @streamReadable);
    controller.@closeRequested = true;
    if (controller.@queue.content.length === 0)
        @readableStreamClose(stream);
}

function readableStreamClose(stream)
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

function readableStreamFulfillReadRequest(stream, chunk, done)
{
    "use strict";

    stream.@reader.@readRequests.@shift().@resolve.@call(@undefined, {value: chunk, done: done});
}

function readableStreamDefaultControllerEnqueue(controller, chunk)
{
    "use strict";

    const stream = controller.@controlledReadableStream;
    @assert(!controller.@closeRequested);
    @assert(stream.@state === @streamReadable);

    if (@isReadableStreamLocked(stream) && stream.@reader.@readRequests.length) {
        @readableStreamFulfillReadRequest(stream, chunk, false);
        @readableStreamDefaultControllerCallPullIfNeeded(controller);
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
    @readableStreamDefaultControllerCallPullIfNeeded(controller);
}

function readableStreamDefaultReaderRead(reader)
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

    return stream.@readableStreamController.@pull(stream.@readableStreamController);
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

function readableStreamReaderGenericRelease(reader)
{
    "use strict";

    @assert(!!reader.@ownerReadableStream);
    @assert(reader.@ownerReadableStream.@reader === reader);

    if (reader.@ownerReadableStream.@state === @streamReadable)
        reader.@closedPromiseCapability.@reject.@call(@undefined, new @TypeError("releasing lock of reader whose stream is still in readable state"));
    else
        reader.@closedPromiseCapability = { @promise: @newHandledRejectedPromise(new @TypeError("reader released lock")) };

    reader.@closedPromiseCapability.@promise.@promiseIsHandled = true;
    reader.@ownerReadableStream.@reader = @undefined;
    reader.@ownerReadableStream = @undefined;
}
