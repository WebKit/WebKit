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

function initializeReadableStream(underlyingSource, strategy)
{
    "use strict";

     if (underlyingSource === @undefined)
         underlyingSource = { };
     if (strategy === @undefined)
         strategy = { };

    if (!@isObject(underlyingSource))
        @throwTypeError("ReadableStream constructor takes an object as first argument");

    if (strategy !== @undefined && !@isObject(strategy))
        @throwTypeError("ReadableStream constructor takes an object as second argument, if any");

    @putByIdDirectPrivate(this, "state", @streamReadable);
    @putByIdDirectPrivate(this, "reader", @undefined);
    @putByIdDirectPrivate(this, "storedError", @undefined);
    @putByIdDirectPrivate(this, "disturbed", false);
    // Initialized with null value to enable distinction with undefined case.
    @putByIdDirectPrivate(this, "readableStreamController", null);

    // FIXME: We should introduce https://streams.spec.whatwg.org/#create-readable-stream.
    // For now, we emulate this with underlyingSource with private properties.
    if (@getByIdDirectPrivate(underlyingSource, "pull") !== @undefined) {
        const size = @getByIdDirectPrivate(strategy, "size");
        const highWaterMark = @getByIdDirectPrivate(strategy, "highWaterMark");
        @setupReadableStreamDefaultController(this, underlyingSource, size, highWaterMark ? highWaterMark : 1, @getByIdDirectPrivate(underlyingSource, "start"), @getByIdDirectPrivate(underlyingSource, "pull"), @getByIdDirectPrivate(underlyingSource, "cancel"));
        return this;
    }

    const type = underlyingSource.type;
    const typeString = @toString(type);

    if (typeString === "bytes") {
        if (!@readableByteStreamAPIEnabled())
            @throwTypeError("ReadableByteStreamController is not implemented");

        if (strategy.highWaterMark === @undefined)
            strategy.highWaterMark = 0;
        if (strategy.size !== @undefined)
            @throwRangeError("Strategy for a ReadableByteStreamController cannot have a size");

        let readableByteStreamControllerConstructor = @ReadableByteStreamController;
        @putByIdDirectPrivate(this, "readableStreamController", new @ReadableByteStreamController(this, underlyingSource, strategy.highWaterMark, @isReadableStream));
    } else if (type === @undefined) {
        if (strategy.highWaterMark === @undefined)
            strategy.highWaterMark = 1;

        @setupReadableStreamDefaultController(this, underlyingSource, strategy.size, strategy.highWaterMark, underlyingSource.start, underlyingSource.pull, underlyingSource.cancel);
    } else
        @throwRangeError("Invalid type for underlying source");

    return this;
}

function cancel(reason)
{
    "use strict";

    if (!@isReadableStream(this))
        return @Promise.@reject(@makeThisTypeError("ReadableStream", "cancel"));

    if (@isReadableStreamLocked(this))
        return @Promise.@reject(@makeTypeError("ReadableStream is locked"));

    return @readableStreamCancel(this, reason);
}

function getReader(options)
{
    "use strict";

    if (!@isReadableStream(this))
        throw @makeThisTypeError("ReadableStream", "getReader");

    if (options === @undefined)
         options = { };

    if (options.mode === @undefined)
        return new @ReadableStreamDefaultReader(this);

    // String conversion is required by spec, hence double equals.
    if (options.mode == 'byob')
        return new @ReadableStreamBYOBReader(this);

    @throwRangeError("Invalid mode is specified");
}

function pipeThrough(streams, options)
{
    "use strict";

    if (@writableStreamAPIEnabled()) {
        if (!@isReadableStream(this))
            throw @makeThisTypeError("ReadableStream", "pipeThrough");

        if (@isReadableStreamLocked(this))
            throw @makeTypeError("ReadableStream is locked");

        const transforms = streams;

        const readable = transforms["readable"];
        if (!@isReadableStream(readable))
            throw @makeTypeError("readable should be ReadableStream");

        const writable = transforms["writable"];
        if (!@isWritableStream(writable))
            throw @makeTypeError("writable should be WritableStream");

        if (options === @undefined)
            options = { };

        let signal;
        if ("signal" in options) {
            signal = options["signal"];
            if (!(signal instanceof @AbortSignal))
                throw @makeTypeError("options.signal must be AbortSignal");
        }

        const preventClose = !!options["preventClose"];
        const preventAbort = !!options["preventAbort"];
        const preventCancel = !!options["preventCancel"];

        if (@isWritableStreamLocked(writable))
            throw @makeTypeError("WritableStream is locked");

        @readableStreamPipeToWritableStream(this, writable, preventClose, preventAbort, preventCancel, signal);

        return readable;
    }

    const writable = streams.writable;
    const readable = streams.readable;
    const promise = this.pipeTo(writable, options);
    if (@isPromise(promise))
        @markPromiseAsHandled(promise);
    return readable;
}

function pipeTo(destination)
{
    "use strict";

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=159869.
    // Built-in generator should be able to parse function signature to compute the function length correctly.
    let options = arguments[1];

    if (@writableStreamAPIEnabled()) {
        if (!@isReadableStream(this))
            return @Promise.@reject(@makeThisTypeError("ReadableStream", "pipeTo"));

        if (!@isWritableStream(destination))
            return @Promise.@reject(@makeTypeError("ReadableStream pipeTo requires a WritableStream"));

        if (options === @undefined)
            options = { };

        // FIXME. We should catch exceptions and reject.
        let signal;
        if ("signal" in options) {
            signal = options["signal"];
            if (!(signal instanceof @AbortSignal))
                return @Promise.@reject(@makeTypeError("options.signal must be AbortSignal"));
        }

        const preventClose = !!options["preventClose"];
        const preventAbort = !!options["preventAbort"];
        const preventCancel = !!options["preventCancel"];

        if (@isReadableStreamLocked(this))
            return @Promise.@reject(@makeTypeError("ReadableStream is locked"));

        if (@isWritableStreamLocked(destination))
            return @Promise.@reject(@makeTypeError("WritableStream is locked"));

        return @readableStreamPipeToWritableStream(this, destination, preventClose, preventAbort, preventCancel, signal);
    }

    // FIXME: rewrite pipeTo so as to require to have 'this' as a ReadableStream and destination be a WritableStream.
    // See https://github.com/whatwg/streams/issues/407.
    // We should shield the pipeTo implementation at the same time.

    const preventClose = @isObject(options) && !!options.preventClose;
    const preventAbort = @isObject(options) && !!options.preventAbort;
    const preventCancel = @isObject(options) && !!options.preventCancel;

    const source = this;

    let reader;
    let lastRead;
    let lastWrite;
    let closedPurposefully = false;
    let promiseCapability;

    function doPipe() {
        lastRead = reader.read();
        @Promise.prototype.@then.@call(@Promise.all([lastRead, destination.ready]), function([{ value, done }]) {
            if (done)
                closeDestination();
            else if (destination.state === "writable") {
                lastWrite = destination.write(value);
                doPipe();
            }
        }, function(e) {
            throw e;
        });
    }

    function cancelSource(reason) {
        if (!preventCancel) {
            reader.cancel(reason);
            reader.releaseLock();
            promiseCapability.@reject.@call(@undefined, reason);
        } else {
            @Promise.prototype.@then.@call(lastRead, function() {
                reader.releaseLock();
                promiseCapability.@reject.@call(@undefined, reason);
            });
        }
    }

    function closeDestination() {
        reader.releaseLock();

        const destinationState = destination.state;
        if (!preventClose && (destinationState === "waiting" || destinationState === "writable")) {
            closedPurposefully = true;
            @Promise.prototype.@then.@call(destination.close(), promiseCapability.@resolve, promiseCapability.@reject);
        } else if (lastWrite !== @undefined)
            @Promise.prototype.@then.@call(lastWrite, promiseCapability.@resolve, promiseCapability.@reject);
        else
            promiseCapability.@resolve.@call();

    }

    function abortDestination(reason) {
        reader.releaseLock();

        if (!preventAbort)
            destination.abort(reason);
        promiseCapability.@reject.@call(@undefined, reason);
    }

    promiseCapability = @newPromiseCapability(@Promise);

    reader = source.getReader();

    @Promise.prototype.@then.@call(reader.closed, @undefined, abortDestination);
    @Promise.prototype.@then.@call(destination.closed,
        function() {
            if (!closedPurposefully)
                cancelSource(@makeTypeError('destination is closing or closed and cannot be piped to anymore'));
        },
        cancelSource
    );

    doPipe();

    return promiseCapability.@promise;
}

function tee()
{
    "use strict";

    if (!@isReadableStream(this))
        throw @makeThisTypeError("ReadableStream", "tee");

    return @readableStreamTee(this, false);
}

@getter
function locked()
{
    "use strict";

    if (!@isReadableStream(this))
        throw @makeGetterTypeError("ReadableStream", "locked");

    return @isReadableStreamLocked(this);
}
