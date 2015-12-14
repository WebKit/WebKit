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

// @conditional=ENABLE(STREAMS_API)

function initializeReadableStream(underlyingSource, strategy)
{
    "use strict";

     if (typeof underlyingSource === "undefined")
         underlyingSource = { };
     if (typeof strategy === "undefined")
         strategy = { highWaterMark: 1, size: function() { return 1; } };

    if (!@isObject(underlyingSource))
        throw new @TypeError("ReadableStream constructor takes an object as first argument");

    if (strategy !== undefined && !@isObject(strategy))
        throw new @TypeError("ReadableStream constructor takes an object as second argument, if any");

    this.@underlyingSource = underlyingSource;

    this.@queue = @newQueue();
    this.@state = @streamReadable;
    this.@started = false;
    this.@closeRequested = false;
    this.@pullAgain = false;
    this.@pulling = false;
    this.@reader = undefined;
    this.@storedError = undefined;
    this.@disturbed = false;
    this.@controller = new @ReadableStreamController(this);
    this.@strategy = @validateAndNormalizeQueuingStrategy(strategy.size, strategy.highWaterMark);

    @promiseInvokeOrNoopNoCatch(underlyingSource, "start", [this.@controller]).@then(() => {
        this.@started = true;
        @requestReadableStreamPull(this);
    }, (error) => {
        if (this.@state === @streamReadable)
            @errorReadableStream(this, error);
    });

    return this;
}

function cancel(reason)
{
    "use strict";

    if (!@isReadableStream(this))
        return @Promise.@reject(new @TypeError("Function should be called on a ReadableStream"));

    if (@isReadableStreamLocked(this))
        return @Promise.@reject(new @TypeError("ReadableStream is locked"));

    return @cancelReadableStream(this, reason);
}

function getReader()
{
    "use strict";

    if (!@isReadableStream(this))
        throw new @TypeError("Function should be called on a ReadableStream");

    return new @ReadableStreamReader(this);
}

function pipeThrough(streams, options)
{
    "use strict";

    const writable = streams.writable;
    const readable = streams.readable;
    this.pipeTo(writable, options);
    return readable;
}

function pipeTo(destination, options)
{
    "use strict";

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
            promiseCapability.@reject.@call(undefined, reason);
        } else {
            @Promise.prototype.@then.@call(lastRead, function() {
                reader.releaseLock();
                promiseCapability.@reject.@call(undefined, reason);
            });
        }
    }

    function closeDestination() {
        reader.releaseLock();

        const destinationState = destination.state;
        if (!preventClose && (destinationState === "waiting" || destinationState === "writable")) {
            closedPurposefully = true;
            @Promise.prototype.@then.@call(destination.close(), promiseCapability.@resolve, promiseCapability.@reject);
        } else if (lastWrite !== undefined)
            @Promise.prototype.@then.@call(lastWrite, promiseCapability.@resolve, promiseCapability.@reject);
        else
            promiseCapability.@resolve.@call();

    }

    function abortDestination(reason) {
        reader.releaseLock();

        if (!preventAbort)
            destination.abort(reason);
        promiseCapability.@reject.@call(undefined, reason);
    }

    promiseCapability = @newPromiseCapability(@Promise);

    reader = source.getReader();

    @Promise.prototype.@then.@call(reader.closed, undefined, abortDestination);
    @Promise.prototype.@then.@call(destination.closed,
        function() {
            if (!closedPurposefully)
                cancelSource(new @TypeError('destination is closing or closed and cannot be piped to anymore'));
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
        throw new @TypeError("Function should be called on a ReadableStream");

    return @teeReadableStream(this, false);
}

function locked()
{
    "use strict";

    if (!@isReadableStream(this))
        throw new @TypeError("Function should be called on a ReadableStream");

    return @isReadableStreamLocked(this);
}
