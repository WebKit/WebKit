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

// @optional=STREAMS_API
// @internals

function isWritableStream(stream)
{
    "use strict";

    return @isObject(stream) && !!stream.@underlyingSink;
}

function syncWritableStreamStateWithQueue(stream)
{
    "use strict";

    if (stream.@state === "closing")
        return undefined;

    // TODO
    // assert(stream.@state === "writable" || stream.@state === "waiting" || stream.@state === undefined);

    if (stream.@queue.size > stream.@strategy.highWaterMark) {
        stream.@state = "waiting";
        stream.@readyPromise = @createNewStreamsPromise();
    } else {
        stream.@state = "writable";
        @resolveStreamsPromise(stream.@readyPromise, undefined);
    }

    return undefined;
}

function errorWritableStream(e)
{
    "use strict";

    if (this.@state === "closed" || this.@state === "errored")
        return undefined;
    while (this.@queue.content.length > 0) {
        var writeRecord = @dequeueValue(this.@queue);
        if (writeRecord !== "close" && writeRecord.promise)
            @rejectStreamsPromise(writeRecord.promise, e);
    }
    this.@storedError = e;
    if (this.@state === "waiting")
        @resolveStreamsPromise(this.@readyPromise, undefined);
    @rejectStreamsPromise(this.@closedPromise, e);
    this.@state = "errored";
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
        // assert(stream.@state === "closing");
        @dequeueValue(stream.@queue);
        // FIXME
        // assert(stream.@queue.content.length === 0);
        @closeWritableStream(stream);
        return undefined;
    }

    stream.@writing = true;
    @promiseInvokeOrNoop(stream.@underlyingSink, "write", [writeRecord.chunk]).then(
        function() {
            if (stream.@state === "errored")
                return;
            stream.@writing = false;
            @resolveStreamsPromise(writeRecord.promise, undefined);
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
    // assert(stream.@state === "closing");
    @promiseInvokeOrNoop(stream.@underlyingSink, "close").then(
        function() {
            if (stream.@state === "errored")
                return;
            // FIXME
            // assert(stream.@state === "closing");
            @resolveStreamsPromise(stream.@closedPromise, undefined);
            stream.@state = "closed";
        },
        function(r) {
            @errorWritableStream.@apply(stream, [r]);
        }
    );

    return undefined;
}
