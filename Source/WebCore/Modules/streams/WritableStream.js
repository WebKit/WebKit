/*
 * Copyright (C) 2015 Canon Inc.
 * Copyright (C) 2015 Igalia
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

function initializeWritableStream(underlyingSink, strategy)
{
    "use strict";

    if (underlyingSink === @undefined)
        underlyingSink = { };
    if (strategy === @undefined)
        strategy = { highWaterMark: 0, size: function() { return 1; } };

    if (!@isObject(underlyingSink))
        @throwTypeError("WritableStream constructor takes an object as first argument");

    if (!@isObject(strategy))
        @throwTypeError("WritableStream constructor takes an object as second argument, if any");

    this.@underlyingSink = underlyingSink;
    this.@closedPromiseCapability = @newPromiseCapability(@Promise);
    this.@readyPromiseCapability = { @promise: @Promise.@resolve() };
    this.@queue = @newQueue();
    this.@state = @streamWritable;
    this.@started = false;
    this.@writing = false;

    this.@strategy = @validateAndNormalizeQueuingStrategy(strategy.size, strategy.highWaterMark);

    @syncWritableStreamStateWithQueue(this);

    const errorFunction = (e) => {
        @errorWritableStream(this, e);
    };
    this.@startedPromise = @promiseInvokeOrNoopNoCatch(underlyingSink, "start", [errorFunction]);
    this.@startedPromise.@then(() => {
        this.@started = true;
        this.@startedPromise = @undefined;
    }, errorFunction);

    return this;
}

function abort(reason)
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(new @TypeError("The WritableStream.abort method can only be used on instances of WritableStream"));

    if (this.@state === @streamClosed)
        return @Promise.@resolve();

    if (this.@state === @streamErrored)
        return @Promise.@reject(this.@storedError);

    @errorWritableStream(this, reason);

    return @promiseInvokeOrFallbackOrNoop(this.@underlyingSink, "abort", [reason], "close", []).@then(function() { });
}

function close()
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(new @TypeError("The WritableStream.close method can only be used on instances of WritableStream"));

    if (this.@state === @streamClosed || this.@state === @streamClosing)
        return @Promise.@reject(new @TypeError("Cannot close a WritableString that is closed or closing"));

    if (this.@state === @streamErrored)
        return @Promise.@reject(this.@storedError);

    if (this.@state === @streamWaiting)
        this.@readyPromiseCapability.@resolve.@call();

    this.@state = @streamClosing;
    @enqueueValueWithSize(this.@queue, "close", 0);
    @callOrScheduleWritableStreamAdvanceQueue(this);

    return this.@closedPromiseCapability.@promise;
}

function write(chunk)
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(new @TypeError("The WritableStream.write method can only be used on instances of WritableStream"));

    if (this.@state === @streamClosed || this.@state === @streamClosing)
        return @Promise.@reject(new @TypeError("Cannot write on a WritableString that is closed or closing"));

    if (this.@state === @streamErrored)
        return @Promise.@reject(this.@storedError);

    @assert(this.@state === @streamWritable || this.@state === @streamWaiting);

    let chunkSize = 1;
    if (this.@strategy.size !== @undefined) {
        try {
            chunkSize = this.@strategy.size.@call(@undefined, chunk);
        } catch(e) {
            @errorWritableStream(this, e);
            return @Promise.@reject(e);
        }
    }

    const promiseCapability = @newPromiseCapability(@Promise);
    try {
        @enqueueValueWithSize(this.@queue, { promiseCapability: promiseCapability, chunk: chunk }, chunkSize);
    } catch (e) {
        @errorWritableStream(this, e);
        return @Promise.@reject(e);
    }

    @syncWritableStreamStateWithQueue(this);
    @callOrScheduleWritableStreamAdvanceQueue(this);

    return promiseCapability.@promise;
}

@getter
function closed()
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(new @TypeError("The WritableStream.closed getter can only be used on instances of WritableStream"));

    return this.@closedPromiseCapability.@promise;
}

@getter
function ready()
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(new @TypeError("The WritableStream.ready getter can only be used on instances of WritableStream"));

    return this.@readyPromiseCapability.@promise;
}

@getter
function state()
{
    "use strict";

    if (!@isWritableStream(this))
        @throwTypeError("The WritableStream.state getter can only be used on instances of WritableStream");

    switch(this.@state) {
    case @streamClosed:
        return "closed";
    case @streamClosing:
        return "closing";
    case @streamErrored:
        return "errored";
    case @streamWaiting:
        return "waiting";
    case @streamWritable:
        return "writable";
    }

    @assert(false);
}
