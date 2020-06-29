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

// @internal

function isWritableStream(stream)
{
    "use strict";

    return @isObject(stream) && !!@getByIdDirectPrivate(stream, "underlyingSink");
}

function syncWritableStreamStateWithQueue(stream)
{
    "use strict";

    const state = @getByIdDirectPrivate(stream, "state");
    if (state === @streamClosing)
        return;

    @assert(state === @streamWritable || state === @streamWaiting);

    const shouldApplyBackpressure = @getByIdDirectPrivate(stream, "queue").size > @getByIdDirectPrivate(stream, "strategy").highWaterMark;
    if (shouldApplyBackpressure && state === @streamWritable) {
        @putByIdDirectPrivate(stream, "state", @streamWaiting);
        @putByIdDirectPrivate(stream, "readyPromiseCapability", @newPromiseCapability(@Promise));
    }
    if (!shouldApplyBackpressure && state === @streamWaiting) {
        @putByIdDirectPrivate(stream, "state", @streamWritable);
        @getByIdDirectPrivate(stream, "readyPromiseCapability").@resolve.@call();
    }
}

function errorWritableStream(stream, e)
{
    "use strict";

    const state = @getByIdDirectPrivate(stream, "state");
    if (state === @streamClosed || state === @streamErrored)
        return;
    while (@getByIdDirectPrivate(stream, "queue").content.length > 0) {
        const writeRecord = @dequeueValue(@getByIdDirectPrivate(stream, "queue"));
        if (writeRecord !== "close")
            writeRecord.promiseCapability.@reject.@call(@undefined, e);
    }
    @putByIdDirectPrivate(stream, "storedError", e);
    if (state === @streamWaiting)
        @getByIdDirectPrivate(stream, "readyPromiseCapability").@resolve.@call();
    @getByIdDirectPrivate(stream, "closedPromiseCapability").@reject.@call(@undefined, e);
    @putByIdDirectPrivate(stream, "state", @streamErrored);
}

function callOrScheduleWritableStreamAdvanceQueue(stream)
{
    "use strict";

    if (!@getByIdDirectPrivate(stream, "started"))
        @getByIdDirectPrivate(stream, "startedPromise").@then(function() { @writableStreamAdvanceQueue(stream); });
    else
        @writableStreamAdvanceQueue(stream);
}

function writableStreamAdvanceQueue(stream)
{
    "use strict";

    if (@getByIdDirectPrivate(stream, "queue").content.length === 0 || @getByIdDirectPrivate(stream, "writing"))
        return;

    const writeRecord = @peekQueueValue(@getByIdDirectPrivate(stream, "queue"));
    if (writeRecord === "close") {
        @assert(@getByIdDirectPrivate(stream, "state") === @streamClosing);
        @dequeueValue(@getByIdDirectPrivate(stream, "queue"));
        @assert(@getByIdDirectPrivate(stream, "queue").content.length === 0);
        @closeWritableStream(stream);
        return;
    }

    @putByIdDirectPrivate(stream, "writing", true);
    @promiseInvokeOrNoop(@getByIdDirectPrivate(stream, "underlyingSink"), "write", [writeRecord.chunk]).@then(
        function() {
            if (@getByIdDirectPrivate(stream, "state") === @streamErrored)
                return;
            @putByIdDirectPrivate(stream, "writing", false);
            writeRecord.promiseCapability.@resolve.@call();
            @dequeueValue(@getByIdDirectPrivate(stream, "queue"));
            @syncWritableStreamStateWithQueue(stream);
            @writableStreamAdvanceQueue(stream);
        },
        function(r) {
            @errorWritableStream(stream, r);
        }
    );
}

function closeWritableStream(stream)
{
    "use strict";

    @assert(@getByIdDirectPrivate(stream, "state") === @streamClosing);
    @promiseInvokeOrNoop(@getByIdDirectPrivate(stream, "underlyingSink"), "close").@then(
        function() {
            if (@getByIdDirectPrivate(stream, "state") === @streamErrored)
                return;
            @assert(@getByIdDirectPrivate(stream, "state") === @streamClosing);
            @getByIdDirectPrivate(stream, "closedPromiseCapability").@resolve.@call();
            @putByIdDirectPrivate(stream, "state", @streamClosed);
        },
        function(r) {
            @errorWritableStream(stream, r);
        }
    );
}
