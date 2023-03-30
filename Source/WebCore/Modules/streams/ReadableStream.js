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

    @putByIdDirectPrivate(this, "globalObject", @getGlobalObject());
    @putByIdDirectPrivate(this, "state", @streamReadable);
    @putByIdDirectPrivate(this, "reader", @undefined);
    @putByIdDirectPrivate(this, "storedError", @undefined);
    @putByIdDirectPrivate(this, "disturbed", false);
    // Initialized with null value to enable distinction with undefined case.
    @putByIdDirectPrivate(this, "readableStreamController", null);

    // FIXME: We should introduce https://streams.spec.whatwg.org/#create-readable-stream.
    // For now, we emulate this with underlyingSource with private properties.
    if (underlyingSource.@pull !== @undefined) {
        const size = @getByIdDirectPrivate(strategy, "size");
        const highWaterMark = @getByIdDirectPrivate(strategy, "highWaterMark");
        @setupReadableStreamDefaultController(this, underlyingSource, size, highWaterMark !== @undefined ? highWaterMark : 1, underlyingSource.@start, underlyingSource.@pull, underlyingSource.@cancel);
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
        let highWaterMark = strategy.highWaterMark;
        if (highWaterMark !== @undefined)
            highWaterMark = @toNumber(highWaterMark);
        else
            highWaterMark = 1;
        const size = strategy.size;
        if (size !== @undefined && !@isCallable(size))
            @throwTypeError("size parameter must be a function");

        const cancel = underlyingSource.cancel;
        if (cancel !== @undefined && !@isCallable(cancel))
            @throwTypeError("underlyingSource cancel must be a function");
        const pull = underlyingSource.pull;
        if (pull !== @undefined && !@isCallable(pull))
            @throwTypeError("underlyingSource pull must be a function");
        const start = underlyingSource.start;
        if (start !== @undefined && !@isCallable(start))
            @throwTypeError("underlyingSource start must be a function");

        @setupReadableStreamDefaultController(this, underlyingSource, size, highWaterMark, start, pull, cancel);
    } else
        @throwTypeError("Invalid type for underlying source");

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

    const mode = @toDictionary(options, { }, "ReadableStream.getReader takes an object as first argument").mode;
    const readableStreamGlobalObject = @getByIdDirectPrivate(this, "globalObject");
    if (mode === @undefined) {
        const readableStreamDefaultReaderConstructor = @getByIdDirectPrivate(readableStreamGlobalObject, "ReadableStreamDefaultReader");
        return new readableStreamDefaultReaderConstructor(this);
    }

    // String conversion is required by spec, hence double equals.
    if (mode == 'byob') {
        const readableStreamBYOBReaderConstructor = @getByIdDirectPrivate(readableStreamGlobalObject, "ReadableStreamBYOBReader");
        return new @ReadableStreamBYOBReader(this);
    }

    @throwTypeError("Invalid mode is specified");
}

function pipeThrough(streams, options)
{
    "use strict";

    const transforms = streams;

    const readable = transforms["readable"];
    if (!@isReadableStream(readable))
        throw @makeTypeError("readable should be ReadableStream");

    const writable = transforms["writable"];
    const internalWritable = @getInternalWritableStream(writable);
    if (!@isWritableStream(internalWritable))
        throw @makeTypeError("writable should be WritableStream");

    let preventClose = false;
    let preventAbort = false;
    let preventCancel = false;
    let signal;
    if (!@isUndefinedOrNull(options)) {
        if (!@isObject(options))
            throw @makeTypeError("options must be an object");

        preventAbort = !!options["preventAbort"];
        preventCancel = !!options["preventCancel"];
        preventClose = !!options["preventClose"];

        signal = options["signal"];
        if (signal !== @undefined && !@isAbortSignal(signal))
            throw @makeTypeError("options.signal must be AbortSignal");
    }

    if (!@isReadableStream(this))
        throw @makeThisTypeError("ReadableStream", "pipeThrough");

    if (@isReadableStreamLocked(this))
        throw @makeTypeError("ReadableStream is locked");

    if (@isWritableStreamLocked(internalWritable))
        throw @makeTypeError("WritableStream is locked");

    const promise = @readableStreamPipeToWritableStream(this, internalWritable, preventClose, preventAbort, preventCancel, signal);
    @markPromiseAsHandled(promise);

    return readable;
}

function pipeTo(destination)
{
    "use strict";

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=159869.
    // Built-in generator should be able to parse function signature to compute the function length correctly.
    let options = arguments[1];

    let preventClose = false;
    let preventAbort = false;
    let preventCancel = false;
    let signal;
    if (!@isUndefinedOrNull(options)) {
        if (!@isObject(options))
            return @Promise.@reject(@makeTypeError("options must be an object"));

        try {
            preventAbort = !!options["preventAbort"];
            preventCancel = !!options["preventCancel"];
            preventClose = !!options["preventClose"];

            signal = options["signal"];
        } catch(e) {
            return @Promise.@reject(e);
        }

        if (signal !== @undefined && !@isAbortSignal(signal))
            return @Promise.@reject(@makeTypeError("options.signal must be AbortSignal"));
    }

    const internalDestination = @getInternalWritableStream(destination);
    if (!@isWritableStream(internalDestination))
        return @Promise.@reject(@makeTypeError("ReadableStream pipeTo requires a WritableStream"));

    if (!@isReadableStream(this))
        return @Promise.@reject(@makeThisTypeError("ReadableStream", "pipeTo"));

    if (@isReadableStreamLocked(this))
        return @Promise.@reject(@makeTypeError("ReadableStream is locked"));

    if (@isWritableStreamLocked(internalDestination))
        return @Promise.@reject(@makeTypeError("WritableStream is locked"));

    return @readableStreamPipeToWritableStream(this, internalDestination, preventClose, preventAbort, preventCancel, signal);
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
