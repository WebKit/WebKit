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
// @internal

function isWritableStream(stream)
{
    "use strict";

    return @isObject(stream) && !!stream.@underlyingSink;
}

function syncWritableStreamStateWithQueue(stream)
{
    "use strict";

    if (stream.@state === @streamClosing)
        return;

    // FIXME
    // assert(stream.@state === @streamWritable || stream.@state === @streamWaiting);

    const shouldApplyBackpressure = stream.@queue.size > stream.@strategy.highWaterMark;
    if (shouldApplyBackpressure && stream.@state === @streamWritable) {
        stream.@state = @streamWaiting;
        stream.@readyPromiseCapability = @newPromiseCapability(@Promise);
    }
    if (!shouldApplyBackpressure && stream.@state === @streamWaiting) {
        stream.@state = @streamWritable;
        stream.@readyPromiseCapability.@resolve.@call(undefined, undefined);
    }
}

function errorWritableStream(e)
{
    "use strict";

    if (this.@state === @streamClosed || this.@state === @streamErrored)
        return undefined;
    while (this.@queue.content.length > 0) {
        var writeRecord = @dequeueValue(this.@queue);
        if (writeRecord !== "close")
            writeRecord.promiseCapability.@reject.@call(undefined, e);
    }
    this.@storedError = e;
    if (this.@state === @streamWaiting)
        this.@readyPromiseCapability.@resolve.@call(undefined, undefined);
    this.@closedPromiseCapability.@reject.@call(undefined, e);
    this.@state = @streamErrored;
    return undefined;
}

function callOrScheduleWritableStreamAdvanceQueue(stream)
{
    if (!stream.@started)
        stream.@startedPromise.then(function() { @writableStreamAdvanceQueue(stream); });
    else
        @writableStreamAdvanceQueue(stream);

    return undefined;
}

function writableStreamAdvanceQueue(stream)
{
    if (stream.@queue.content.length === 0 || stream.@writing)
        return undefined;

    const writeRecord = @peekQueueValue(stream.@queue);
    if (writeRecord === "close") {
        // FIXME
        // assert(stream.@state === @streamClosing);
        @dequeueValue(stream.@queue);
        // FIXME
        // assert(stream.@queue.content.length === 0);
        @closeWritableStream(stream);
        return undefined;
    }

    stream.@writing = true;
    @promiseInvokeOrNoop(stream.@underlyingSink, "write", [writeRecord.chunk]).then(
        function() {
            if (stream.@state === @streamErrored)
                return;
            stream.@writing = false;
            writeRecord.promiseCapability.@resolve.@call(undefined, undefined);
            @dequeueValue(stream.@queue);
            @syncWritableStreamStateWithQueue(stream);
            @writableStreamAdvanceQueue(stream);
        },
        function(r) {
            @errorWritableStream.@apply(stream, [r]);
        }
    );

    return undefined;
}

function closeWritableStream(stream)
{
    // FIXME
    // assert(stream.@state === @streamClosing);
    @promiseInvokeOrNoop(stream.@underlyingSink, "close").then(
        function() {
            if (stream.@state === @streamErrored)
                return;
            // FIXME
            // assert(stream.@state === @streamClosing);
            stream.@closedPromiseCapability.@resolve.@call(undefined, undefined);
            stream.@state = @streamClosed;
        },
        function(r) {
            @errorWritableStream.@apply(stream, [r]);
        }
    );

    return undefined;
}
