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

    @putByIdDirectPrivate(this, "underlyingSink", underlyingSink);
    @putByIdDirectPrivate(this, "closedPromiseCapability", @newPromiseCapability(@Promise));
    @putByIdDirectPrivate(this, "readyPromiseCapability", { @promise: @Promise.@resolve() });
    @putByIdDirectPrivate(this, "queue", @newQueue());
    @putByIdDirectPrivate(this, "state", @streamWritable);
    @putByIdDirectPrivate(this, "started", false);
    @putByIdDirectPrivate(this, "writing", false);

    @putByIdDirectPrivate(this, "strategy", @validateAndNormalizeQueuingStrategy(strategy.size, strategy.highWaterMark));

    @syncWritableStreamStateWithQueue(this);

    const errorFunction = (e) => {
        @errorWritableStream(this, e);
    };
    @putByIdDirectPrivate(this, "startedPromise", @promiseInvokeOrNoopNoCatch(underlyingSink, "start", [errorFunction]));
    @getByIdDirectPrivate(this, "startedPromise").@then(() => {
        @putByIdDirectPrivate(this, "started", true);
        @putByIdDirectPrivate(this, "startedPromise", @undefined);
    }, errorFunction);

    return this;
}

function abort(reason)
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(@makeTypeError("The WritableStream.abort method can only be used on instances of WritableStream"));

    const state = @getByIdDirectPrivate(this, "state");
    if (state === @streamClosed)
        return @Promise.@resolve();

    if (state === @streamErrored)
        return @Promise.@reject(@getByIdDirectPrivate(this, "storedError"));

    @errorWritableStream(this, reason);

    return @promiseInvokeOrFallbackOrNoop(@getByIdDirectPrivate(this, "underlyingSink"), "abort", [reason], "close", []).@then(function() { });
}

function close()
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(@makeTypeError("The WritableStream.close method can only be used on instances of WritableStream"));

    const state = @getByIdDirectPrivate(this, "state");
    if (state === @streamClosed || state === @streamClosing)
        return @Promise.@reject(@makeTypeError("Cannot close a WritableString that is closed or closing"));

    if (state === @streamErrored)
        return @Promise.@reject(@getByIdDirectPrivate(this, "storedError"));

    if (state === @streamWaiting)
        @getByIdDirectPrivate(this, "readyPromiseCapability").@resolve.@call();

    @putByIdDirectPrivate(this, "state", @streamClosing);
    @enqueueValueWithSize(@getByIdDirectPrivate(this, "queue"), "close", 0);
    @callOrScheduleWritableStreamAdvanceQueue(this);

    return @getByIdDirectPrivate(this, "closedPromiseCapability").@promise;
}

function write(chunk)
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(@makeTypeError("The WritableStream.write method can only be used on instances of WritableStream"));

    const state = @getByIdDirectPrivate(this, "state");
    if (state === @streamClosed || state === @streamClosing)
        return @Promise.@reject(@makeTypeError("Cannot write on a WritableString that is closed or closing"));

    if (state === @streamErrored)
        return @Promise.@reject(this.@storedError);

    @assert(state === @streamWritable || state === @streamWaiting);

    let chunkSize = 1;
    if (@getByIdDirectPrivate(this, "strategy").size !== @undefined) {
        try {
            chunkSize = @getByIdDirectPrivate(this, "strategy").size.@call(@undefined, chunk);
        } catch(e) {
            @errorWritableStream(this, e);
            return @Promise.@reject(e);
        }
    }

    const promiseCapability = @newPromiseCapability(@Promise);
    try {
        @enqueueValueWithSize(@getByIdDirectPrivate(this, "queue"), { promiseCapability: promiseCapability, chunk: chunk }, chunkSize);
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
        return @Promise.@reject(@makeGetterTypeError("WritableStream", "closed"));

    return @getByIdDirectPrivate(this, "closedPromiseCapability").@promise;
}

@getter
function ready()
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(@makeGetterTypeError("WritableStream", "ready"));

    return @getByIdDirectPrivate(this, "readyPromiseCapability").@promise;
}

@getter
function state()
{
    "use strict";

    if (!@isWritableStream(this))
        @throwTypeError("The WritableStream.state getter can only be used on instances of WritableStream");

    switch(@getByIdDirectPrivate(this, "state")) {
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
