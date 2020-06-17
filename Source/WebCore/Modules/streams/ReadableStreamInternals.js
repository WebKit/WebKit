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

function readableStreamReaderGenericInitialize(reader, stream)
{
    "use strict";

    @putByIdDirectPrivate(reader, "ownerReadableStream", stream);
    @putByIdDirectPrivate(stream, "reader", reader);
    if (@getByIdDirectPrivate(stream, "state") === @streamReadable)
        @putByIdDirectPrivate(reader, "closedPromiseCapability", @newPromiseCapability(@Promise));
    else if (@getByIdDirectPrivate(stream, "state") === @streamClosed)
        @putByIdDirectPrivate(reader, "closedPromiseCapability", { @promise: @Promise.@resolve() });
    else {
        @assert(@getByIdDirectPrivate(stream, "state") === @streamErrored);
        @putByIdDirectPrivate(reader, "closedPromiseCapability", { @promise: @newHandledRejectedPromise(@getByIdDirectPrivate(stream, "storedError")) });
    }
}

function privateInitializeReadableStreamDefaultController(stream, underlyingSource, size, highWaterMark)
{
    "use strict";

    if (!@isReadableStream(stream))
        @throwTypeError("ReadableStreamDefaultController needs a ReadableStream");

    // readableStreamController is initialized with null value.
    if (@getByIdDirectPrivate(stream, "readableStreamController") !== null)
        @throwTypeError("ReadableStream already has a controller");

    @putByIdDirectPrivate(this, "controlledReadableStream", stream);
    @putByIdDirectPrivate(this, "underlyingSource", underlyingSource);
    @putByIdDirectPrivate(this, "queue", @newQueue());
    @putByIdDirectPrivate(this, "started", false);
    @putByIdDirectPrivate(this, "closeRequested", false);
    @putByIdDirectPrivate(this, "pullAgain", false);
    @putByIdDirectPrivate(this, "pulling", false);
    @putByIdDirectPrivate(this, "strategy", @validateAndNormalizeQueuingStrategy(size, highWaterMark));

    return this;
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller, starting from step 6.
// The other part is implemented in privateInitializeReadableStreamDefaultController.
function setupReadableStreamDefaultController(stream, underlyingSource, size, highWaterMark, startMethod, pullMethod, cancelMethod)
{
    "use strict";
    const controller = new @ReadableStreamDefaultController(stream, underlyingSource, size, highWaterMark, @isReadableStream);
    const startAlgorithm = () => @promiseInvokeOrNoopMethodNoCatch(underlyingSource, startMethod, [controller]);
    const pullAlgorithm = () => @promiseInvokeOrNoopMethod(underlyingSource, pullMethod, [controller]);
    const cancelAlgorithm = (reason) => @promiseInvokeOrNoopMethod(underlyingSource, cancelMethod, [reason]);

    @putByIdDirectPrivate(controller, "pullAlgorithm", pullAlgorithm);
    @putByIdDirectPrivate(controller, "cancelAlgorithm", cancelAlgorithm);
    @putByIdDirectPrivate(controller, "pull", @readableStreamDefaultControllerPull);
    @putByIdDirectPrivate(controller, "cancel", @readableStreamDefaultControllerCancel);
    @putByIdDirectPrivate(stream, "readableStreamController", controller);

    startAlgorithm().@then(() => {
        @putByIdDirectPrivate(controller, "started", true);
        @assert(!@getByIdDirectPrivate(controller, "pulling"));
        @assert(!@getByIdDirectPrivate(controller, "pullAgain"));
        @readableStreamDefaultControllerCallPullIfNeeded(controller);
    }, (error) => {
        @readableStreamDefaultControllerError(controller, error);
    });
}

function readableStreamDefaultControllerError(controller, error)
{
    "use strict";

    const stream = @getByIdDirectPrivate(controller, "controlledReadableStream");
    if (@getByIdDirectPrivate(stream, "state") !== @streamReadable)
        return;
    @putByIdDirectPrivate(controller, "queue", @newQueue());
    @readableStreamError(stream, error);
}

function readableStreamPipeTo(stream, sink)
{
    "use strict";
    @assert(@isReadableStream(stream));

    const reader = new @ReadableStreamDefaultReader(stream);

    @getByIdDirectPrivate(reader, "closedPromiseCapability").@promise.@then(() => { }, (e) => { sink.error(e); });

    function doPipe() {
        @readableStreamDefaultReaderRead(reader).@then(function(result) {
            if (result.done) {
                sink.close();
                return;
            }
            try {
                sink.enqueue(result.value);
            } catch (e) {
                sink.error("ReadableStream chunk enqueueing in the sink failed");
                return;
            }
            doPipe();
        }, function(e) {
            sink.error(e);
        });
    }
    doPipe();
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

    teeState.cancelPromiseCapability = @newPromiseCapability(@Promise);

    const pullFunction = @readableStreamTeePullFunction(teeState, reader, shouldClone);

    const branch1Source = { };
    @putByIdDirectPrivate(branch1Source, "pull", pullFunction);
    @putByIdDirectPrivate(branch1Source, "cancel", @readableStreamTeeBranch1CancelFunction(teeState, stream));

    const branch2Source = { };
    @putByIdDirectPrivate(branch2Source, "pull", pullFunction);
    @putByIdDirectPrivate(branch2Source, "cancel", @readableStreamTeeBranch2CancelFunction(teeState, stream));

    const branch1 = new @ReadableStream(branch1Source);
    const branch2 = new @ReadableStream(branch2Source);

    @getByIdDirectPrivate(reader, "closedPromiseCapability").@promise.@then(@undefined, function(e) {
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
    return @isObject(stream) && @getByIdDirectPrivate(stream, "readableStreamController") !== @undefined;
}

function isReadableStreamDefaultReader(reader)
{
    "use strict";

    // Spec tells to return true only if reader has a readRequests internal slot.
    // However, since it is a private slot, it cannot be checked using hasOwnProperty().
    // Since readRequests is initialized with an empty array, the following test is ok.
    return @isObject(reader) && !!@getByIdDirectPrivate(reader, "readRequests");
}

function isReadableStreamDefaultController(controller)
{
    "use strict";

    // Spec tells to return true only if controller has an underlyingSource internal slot.
    // However, since it is a private slot, it cannot be checked using hasOwnProperty().
    // underlyingSource is obtained in ReadableStream constructor: if undefined, it is set
    // to an empty object. Therefore, following test is ok.
    return @isObject(controller) && !!@getByIdDirectPrivate(controller, "underlyingSource");
}

function readableStreamError(stream, error)
{
    "use strict";

    @assert(@isReadableStream(stream));
    @assert(@getByIdDirectPrivate(stream, "state") === @streamReadable);
    @putByIdDirectPrivate(stream, "state", @streamErrored);
    @putByIdDirectPrivate(stream, "storedError", error);

    if (!@getByIdDirectPrivate(stream, "reader"))
        return;

    const reader = @getByIdDirectPrivate(stream, "reader");

    if (@isReadableStreamDefaultReader(reader)) {
        const requests = @getByIdDirectPrivate(reader, "readRequests");
        for (let index = 0, length = requests.length; index < length; ++index)
            requests[index].@reject.@call(@undefined, error);
        @putByIdDirectPrivate(reader, "readRequests", []);
    } else {
        @assert(@isReadableStreamBYOBReader(reader));
        const requests = @getByIdDirectPrivate(reader, "readIntoRequests");
        for (let index = 0, length = requests.length; index < length; ++index)
            requests[index].@reject.@call(@undefined, error);
        @putByIdDirectPrivate(reader, "readIntoRequests", []);
    }

    @getByIdDirectPrivate(reader, "closedPromiseCapability").@reject.@call(@undefined, error);
    const promise = @getByIdDirectPrivate(reader, "closedPromiseCapability").@promise;
    @putPromiseInternalField(promise, @promiseFieldFlags, @getPromiseInternalField(promise, @promiseFieldFlags) | @promiseFlagsIsHandled);
}

function readableStreamDefaultControllerCallPullIfNeeded(controller)
{
    "use strict";

    const stream = @getByIdDirectPrivate(controller, "controlledReadableStream");

    if (!@readableStreamDefaultControllerCanCloseOrEnqueue(controller))
        return;
    if (!@getByIdDirectPrivate(controller, "started"))
        return;
    if ((!@isReadableStreamLocked(stream) || !@getByIdDirectPrivate(@getByIdDirectPrivate(stream, "reader"), "readRequests").length) && @readableStreamDefaultControllerGetDesiredSize(controller) <= 0)
        return;

    if (@getByIdDirectPrivate(controller, "pulling")) {
        @putByIdDirectPrivate(controller, "pullAgain", true);
        return;
    }

    @assert(!@getByIdDirectPrivate(controller, "pullAgain"));
    @putByIdDirectPrivate(controller, "pulling", true);

    @getByIdDirectPrivate(controller, "pullAlgorithm").@call(@undefined).@then(function() {
        @putByIdDirectPrivate(controller, "pulling", false);
        if (@getByIdDirectPrivate(controller, "pullAgain")) {
            @putByIdDirectPrivate(controller, "pullAgain", false);
            @readableStreamDefaultControllerCallPullIfNeeded(controller);
        }
    }, function(error) {
        @readableStreamDefaultControllerError(controller, error);
    });
}

function isReadableStreamLocked(stream)
{
   "use strict";

    @assert(@isReadableStream(stream));
    return !!@getByIdDirectPrivate(stream, "reader");
}

function readableStreamDefaultControllerGetDesiredSize(controller)
{
   "use strict";

    const stream = @getByIdDirectPrivate(controller, "controlledReadableStream");
    const state = @getByIdDirectPrivate(stream, "state");

    if (state === @streamErrored)
        return null;
    if (state === @streamClosed)
        return 0;

    return @getByIdDirectPrivate(controller, "strategy").highWaterMark - @getByIdDirectPrivate(controller, "queue").size;
}


function readableStreamReaderGenericCancel(reader, reason)
{
    "use strict";

    const stream = @getByIdDirectPrivate(reader, "ownerReadableStream");
    @assert(!!stream);
    return @readableStreamCancel(stream, reason);
}

function readableStreamCancel(stream, reason)
{
    "use strict";

    @putByIdDirectPrivate(stream, "disturbed", true);
    const state = @getByIdDirectPrivate(stream, "state");
    if (state === @streamClosed)
        return @Promise.@resolve();
    if (state === @streamErrored)
        return @Promise.@reject(@getByIdDirectPrivate(stream, "storedError"));
    @readableStreamClose(stream);
    return @getByIdDirectPrivate(stream, "readableStreamController").@cancel(@getByIdDirectPrivate(stream, "readableStreamController"), reason).@then(function() {  });
}

function readableStreamDefaultControllerCancel(controller, reason)
{
    "use strict";

    @putByIdDirectPrivate(controller, "queue", @newQueue());
    return @getByIdDirectPrivate(controller, "cancelAlgorithm").@call(@undefined, reason);
}

function readableStreamDefaultControllerPull(controller)
{
    "use strict";

    const stream = @getByIdDirectPrivate(controller, "controlledReadableStream");
    if (@getByIdDirectPrivate(controller, "queue").content.length) {
        const chunk = @dequeueValue(@getByIdDirectPrivate(controller, "queue"));
        if (@getByIdDirectPrivate(controller, "closeRequested") && @getByIdDirectPrivate(controller, "queue").content.length === 0)
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

    @assert(@readableStreamDefaultControllerCanCloseOrEnqueue(controller));
    @putByIdDirectPrivate(controller, "closeRequested", true);
    if (@getByIdDirectPrivate(controller, "queue").content.length === 0)
        @readableStreamClose(@getByIdDirectPrivate(controller, "controlledReadableStream"));
}

function readableStreamClose(stream)
{
    "use strict";

    @assert(@getByIdDirectPrivate(stream, "state") === @streamReadable);
    @putByIdDirectPrivate(stream, "state", @streamClosed);
    const reader = @getByIdDirectPrivate(stream, "reader");

    if (!reader)
        return;

    if (@isReadableStreamDefaultReader(reader)) {
        const requests = @getByIdDirectPrivate(reader, "readRequests");
        for (let index = 0, length = requests.length; index < length; ++index)
            requests[index].@resolve.@call(@undefined, {value:@undefined, done: true});
        @putByIdDirectPrivate(reader, "readRequests", []);
    }

    @getByIdDirectPrivate(reader, "closedPromiseCapability").@resolve.@call();
}

function readableStreamFulfillReadRequest(stream, chunk, done)
{
    "use strict";

    @getByIdDirectPrivate(@getByIdDirectPrivate(stream, "reader"), "readRequests").@shift().@resolve.@call(@undefined, {value: chunk, done: done});
}

function readableStreamDefaultControllerEnqueue(controller, chunk)
{
    "use strict";

    const stream = @getByIdDirectPrivate(controller, "controlledReadableStream");
    @assert(@readableStreamDefaultControllerCanCloseOrEnqueue(controller));

    if (@isReadableStreamLocked(stream) && @getByIdDirectPrivate(@getByIdDirectPrivate(stream, "reader"), "readRequests").length) {
        @readableStreamFulfillReadRequest(stream, chunk, false);
        @readableStreamDefaultControllerCallPullIfNeeded(controller);
        return;
    }

    try {
        let chunkSize = 1;
        if (@getByIdDirectPrivate(controller, "strategy").size !== @undefined)
            chunkSize = @getByIdDirectPrivate(controller, "strategy").size(chunk);
        @enqueueValueWithSize(@getByIdDirectPrivate(controller, "queue"), chunk, chunkSize);
    }
    catch(error) {
        @readableStreamDefaultControllerError(controller, error);
        throw error;
    }
    @readableStreamDefaultControllerCallPullIfNeeded(controller);
}

function readableStreamDefaultReaderRead(reader)
{
    "use strict";

    const stream = @getByIdDirectPrivate(reader, "ownerReadableStream");
    @assert(!!stream);
    const state = @getByIdDirectPrivate(stream, "state");

    @putByIdDirectPrivate(stream, "disturbed", true);
    if (state === @streamClosed)
        return @Promise.@resolve({value: @undefined, done: true});
    if (state === @streamErrored)
        return @Promise.@reject(@getByIdDirectPrivate(stream, "storedError"));
    @assert(state === @streamReadable);

    return @getByIdDirectPrivate(stream, "readableStreamController").@pull(@getByIdDirectPrivate(stream, "readableStreamController"));
}

function readableStreamAddReadRequest(stream)
{
    "use strict";

    @assert(@isReadableStreamDefaultReader(@getByIdDirectPrivate(stream, "reader")));
    @assert(@getByIdDirectPrivate(stream, "state") == @streamReadable);

    const readRequest = @newPromiseCapability(@Promise);
    @getByIdDirectPrivate(@getByIdDirectPrivate(stream, "reader"), "readRequests").@push(readRequest);

    return readRequest.@promise;
}

function isReadableStreamDisturbed(stream)
{
    "use strict";

    @assert(@isReadableStream(stream));
    return @getByIdDirectPrivate(stream, "disturbed");
}

function readableStreamReaderGenericRelease(reader)
{
    "use strict";

    @assert(!!@getByIdDirectPrivate(reader, "ownerReadableStream"));
    @assert(@getByIdDirectPrivate(@getByIdDirectPrivate(reader, "ownerReadableStream"), "reader") === reader);

    if (@getByIdDirectPrivate(@getByIdDirectPrivate(reader, "ownerReadableStream"), "state") === @streamReadable)
        @getByIdDirectPrivate(reader, "closedPromiseCapability").@reject.@call(@undefined, @makeTypeError("releasing lock of reader whose stream is still in readable state"));
    else
        @putByIdDirectPrivate(reader, "closedPromiseCapability", { @promise: @newHandledRejectedPromise(@makeTypeError("reader released lock")) });

    const promise = @getByIdDirectPrivate(reader, "closedPromiseCapability").@promise;
    @putPromiseInternalField(promise, @promiseFieldFlags, @getPromiseInternalField(promise, @promiseFieldFlags) | @promiseFlagsIsHandled);
    @putByIdDirectPrivate(@getByIdDirectPrivate(reader, "ownerReadableStream"), "reader", @undefined);
    @putByIdDirectPrivate(reader, "ownerReadableStream", @undefined);
}

function readableStreamDefaultControllerCanCloseOrEnqueue(controller)
{
    "use strict";

    return !@getByIdDirectPrivate(controller, "closeRequested") && @getByIdDirectPrivate(@getByIdDirectPrivate(controller, "controlledReadableStream"), "state") === @streamReadable;
}
