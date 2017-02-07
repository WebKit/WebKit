/*
 * Copyright (C) 2016 Canon Inc. All rights reserved.
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

// @conditional=ENABLE(READABLE_STREAM_API) && ENABLE(READABLE_BYTE_STREAM_API)
// @internal

function privateInitializeReadableByteStreamController(stream, underlyingByteSource, highWaterMark)
{
    "use strict";

    if (!@isReadableStream(stream))
        @throwTypeError("ReadableByteStreamController needs a ReadableStream");

    // readableStreamController is initialized with null value.
    if (stream.@readableStreamController !== null)
        @throwTypeError("ReadableStream already has a controller");

    this.@controlledReadableStream = stream;
    this.@underlyingByteSource = underlyingByteSource;
    this.@pullAgain = false;
    this.@pulling = false;
    @readableByteStreamControllerClearPendingPullIntos(this);
    this.@queue = [];
    this.@totalQueuedBytes = 0;
    this.@started = false;
    this.@closeRequested = false;

    let hwm = @Number(highWaterMark);
    if (@isNaN(hwm) || hwm < 0)
        @throwRangeError("highWaterMark value is negative or not a number");
    this.@strategyHWM = hwm;

    let autoAllocateChunkSize = underlyingByteSource.autoAllocateChunkSize;
    if (autoAllocateChunkSize !== @undefined) {
        autoAllocateChunkSize = @Number(autoAllocateChunkSize);
        if (autoAllocateChunkSize <= 0 || autoAllocateChunkSize === @Number.POSITIVE_INFINITY || autoAllocateChunkSize === @Number.NEGATIVE_INFINITY)
            @throwRangeError("autoAllocateChunkSize value is negative or equal to positive or negative infinity");
    }
    this.@autoAllocateChunkSize = autoAllocateChunkSize;
    this.@pendingPullIntos = [];

    const controller = this;
    const startResult = @promiseInvokeOrNoopNoCatch(underlyingByteSource, "start", [this]).@then(() => {
        controller.@started = true;
        @assert(!controller.@pulling);
        @assert(!controller.@pullAgain);
        @readableByteStreamControllerCallPullIfNeeded(controller);
    }, (error) => {
        if (stream.@state === @streamReadable)
            @readableByteStreamControllerError(controller, error);
    });

    this.@cancel = @readableByteStreamControllerCancel;
    this.@pull = @readableByteStreamControllerPull;

    return this;
}

function isReadableByteStreamController(controller)
{
    "use strict";

    // Same test mechanism as in isReadableStreamDefaultController (ReadableStreamInternals.js).
    // See corresponding function for explanations.
    return @isObject(controller) && !!controller.@underlyingByteSource;
}

function isReadableStreamBYOBReader(reader)
{
    "use strict";

    // FIXME: Since BYOBReader is not yet implemented, always return false.
    // To be implemented at the same time as BYOBReader (see isReadableStreamDefaultReader
    // to apply same model).
    return false;
}

function readableByteStreamControllerCancel(controller, reason)
{
    "use strict";

    if (controller.@pendingPullIntos.length > 0)
        controller.@pendingPullIntos[0].bytesFilled = 0;
    controller.@queue = [];
    controller.@totalQueuedBytes = 0;
    return @promiseInvokeOrNoop(controller.@underlyingByteSource, "cancel", [reason]);
}

function readableByteStreamControllerError(controller, e)
{
    "use strict";

    @assert(controller.@controlledReadableStream.@state === @streamReadable);
    @readableByteStreamControllerClearPendingPullIntos(controller);
    controller.@queue = [];
    @readableStreamError(controller.@controlledReadableStream, e);
}

function readableByteStreamControllerClose(controller)
{
    "use strict";

    @assert(!controller.@closeRequested);
    @assert(controller.@controlledReadableStream.@state === @streamReadable);

    if (controller.@totalQueuedBytes > 0) {
        controller.@closeRequested = true;
        return;
    }

    if (controller.@pendingPullIntos.length > 0) {
        if (controller.@pendingPullIntos[0].bytesFilled > 0) {
            const e = new @TypeError("Close requested while there remain pending bytes");
            @readableByteStreamControllerError(controller, e);
            throw e;
        }
    }

    @readableStreamClose(controller.@controlledReadableStream);
}

function readableByteStreamControllerClearPendingPullIntos(controller)
{
    "use strict";

    // FIXME: To be implemented in conjunction with ReadableStreamBYOBRequest.
}

function readableByteStreamControllerGetDesiredSize(controller)
{
   "use strict";

   return controller.@strategyHWM - controller.@totalQueuedBytes;
}

function readableStreamHasBYOBReader(stream)
{
    "use strict";

    return stream.@reader !== @undefined && @isReadableStreamBYOBReader(stream.@reader);
}

function readableStreamHasDefaultReader(stream)
{
    "use strict";

    return stream.@reader !== @undefined && @isReadableStreamDefaultReader(stream.@reader);
}

function readableByteStreamControllerHandleQueueDrain(controller) {

    "use strict";

    @assert(controller.@controlledReadableStream.@state === @streamReadable);
    if (!controller.@totalQueuedBytes && controller.@closeRequested)
        @readableStreamClose(controller.@controlledReadableStream);
    else
        @readableByteStreamControllerCallPullIfNeeded(controller);
}

function readableByteStreamControllerPull(controller)
{
    "use strict";

    const stream = controller.@controlledReadableStream;
    @assert(@readableStreamHasDefaultReader(stream));

    if (controller.@totalQueuedBytes > 0) {
        @assert(stream.@reader.@readRequests.length === 0);
        const entry = controller.@queue.@shift();
        controller.@totalQueuedBytes -= entry.byteLength;
        @readableByteStreamControllerHandleQueueDrain(controller);
        let view;
        try {
            view = new @Uint8Array(entry.buffer, entry.byteOffset, entry.byteLength);
        } catch (error) {
            return @Promise.@reject(error);
        }
        return @Promise.@resolve({value: view, done: false});
    }

    if (controller.@autoAllocateChunkSize !== @undefined) {
        let buffer;
        try {
            buffer = new @ArrayBuffer(controller.@autoAllocateChunkSize);
        } catch (error) {
            return @Promise.@reject(error);
        }
        const pullIntoDescriptor = {
            buffer,
            byteOffset: 0,
            byteLength: controller.@autoAllocateChunkSize,
            bytesFilled: 0,
            elementSize: 1,
            ctor: @Uint8Array,
            readerType: 'default'
        };
        controller.@pendingPullIntos.@push(pullIntoDescriptor);
    }

    const promise = @readableStreamAddReadRequest(stream);
    @readableByteStreamControllerCallPullIfNeeded(controller);
    return promise;
}

function readableByteStreamControllerShouldCallPull(controller)
{
    "use strict";

    const stream = controller.@controlledReadableStream;

    if (stream.@state !== @streamReadable)
        return false;
    if (controller.@closeRequested)
        return false;
    if (!controller.@started)
        return false;
    if (@readableStreamHasDefaultReader(stream) && stream.@reader.@readRequests.length > 0)
        return true;
    if (@readableStreamHasBYOBReader(stream) && stream.@reader.@readIntoRequests.length > 0)
        return true;
    if (@readableByteStreamControllerGetDesiredSize(controller) > 0)
        return true;
    return false;
}

function readableByteStreamControllerCallPullIfNeeded(controller)
{
    "use strict";

    if (!@readableByteStreamControllerShouldCallPull(controller))
        return;

    if (controller.@pulling) {
        controller.@pullAgain = true;
        return;
    }

    @assert(!controller.@pullAgain);
    controller.@pulling = true;
    @promiseInvokeOrNoop(controller.@underlyingByteSource, "pull", [controller]).@then(() => {
        controller.@pulling = false;
        if (controller.@pullAgain) {
            controller.@pullAgain = false;
            @readableByteStreamControllerCallPullIfNeeded(controller);
        }
    }, (error) => {
        if (controller.@controlledReadableStream.@state === @streamReadable)
            @readableByteStreamControllerError(controller, error);
    });
}

function transferBufferToCurrentRealm(buffer)
{
    "use strict";

    // FIXME: Determine what should be done here exactly (what is already existing in current
    // codebase and what has to be added). According to spec, Transfer operation should be
    // performed in order to transfer buffer to current realm. For the moment, simply return
    // received buffer.
    return buffer;
}

function readableByteStreamControllerEnqueue(controller, chunk)
{
    "use strict";

    const stream = controller.@controlledReadableStream;
    @assert(!controller.@closeRequested);
    @assert(stream.@state === @streamReadable);
    const buffer = chunk.buffer;
    const byteOffset = chunk.byteOffset;
    const byteLength = chunk.byteLength;
    const transferredBuffer = @transferBufferToCurrentRealm(buffer);

    if (@readableStreamHasDefaultReader(stream)) {
        if (!stream.@reader.@readRequests.length)
            @readableByteStreamControllerEnqueueChunkToQueue(controller, transferredBuffer, byteOffset, byteLength);
        else {
            @assert(!controller.@queue.length);
            let transferredView = new @Uint8Array(transferredBuffer, byteOffset, byteLength);
            @readableStreamFulfillReadRequest(stream, transferredView, false);
        }
        return;
    }

    if (@readableStreamHasBYOBReader(stream)) {
        // FIXME: To be implemented once ReadableStreamBYOBReader has been implemented (for the moment,
        // test cannot be true).
        @throwTypeError("ReadableByteStreamController enqueue operation has no support for BYOB reader");
        return;
    }

    @assert(!@isReadableStreamLocked(stream));
    @readableByteStreamControllerEnqueueChunkToQueue(controller, transferredBuffer, byteOffset, byteLength);
}

function readableByteStreamControllerEnqueueChunkToQueue(controller, buffer, byteOffset, byteLength)
{
    "use strict";

    controller.@queue.@push({
        buffer: buffer,
        byteOffset: byteOffset,
        byteLength: byteLength
    });
    controller.@totalQueuedBytes += byteLength;
}
