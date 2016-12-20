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
    this.@queue = @newQueue();
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
    this.@pendingPullIntos = @newQueue();

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
    // FIXME: Implement pull.

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

    if (controller.@pendingPullIntos.content.length > 0)
        controller.@pendingPullIntos[0].bytesFilled = 0;
    controller.@queue = @newQueue();
    controller.@totalQueuedBytes = 0;
    return @promiseInvokeOrNoop(controller.@underlyingByteSource, "cancel", [reason]);
}

function readableByteStreamControllerError(controller, e)
{
    "use strict";

    @assert(controller.@controlledReadableStream.@state === @streamReadable);
    @readableByteStreamControllerClearPendingPullIntos(controller);
    controller.@queue = @newQueue();
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

    if (controller.@pendingPullIntos.content.length > 0) {
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

function readableByteStreamControllerCallPullIfNeeded(controller)
{
    "use strict";

    const stream = controller.@controlledReadableStream;

    if (stream.@state !== @streamReadable)
        return;
    if (controller.@closeRequested)
        return;
    if (!@readableStreamHasDefaultReader(stream) || stream.@reader.@readRequests <= 0)
        if (!@readableStreamHasBYOBReader(stream) || stream.@reader.@readIntoRequests <= 0)
            if (@readableByteStreamControllerGetDesiredSize(controller) <= 0)
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
