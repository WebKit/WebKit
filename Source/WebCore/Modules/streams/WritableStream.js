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

function initializeWritableStream(underlyingSink, strategy)
{
    "use strict";

    if (underlyingSink === @undefined)
        underlyingSink = { };

    if (strategy === @undefined)
        strategy = { };

    if (!@isObject(underlyingSink))
        @throwTypeError("WritableStream constructor takes an object as first argument");

    // CreateWriteStream code path.
    if (@getByIdDirectPrivate(underlyingSink, "WritableStream")) {
        @privateInitializeWritableStream(this, underlyingSink);

        const controller = new @WritableStreamDefaultController();

        @setUpWritableStreamDefaultController(this, controller, underlyingSink.startAlgorithm, underlyingSink.writeAlgorithm, underlyingSink.closeAlgorithm, underlyingSink.abortAlgorithm, strategy.highWaterMark, strategy.sizeAlgorithm);
        return this;
    }

    if ("type" in underlyingSink)
        @throwRangeError("Invalid type is specified");

    const sizeAlgorithm = @extractSizeAlgorithm(strategy);
    const highWaterMark = @extractHighWaterMark(strategy, 1);

    const underlyingSinkDict = { };
    if ("start" in underlyingSink) {
        underlyingSinkDict["start"] = underlyingSink["start"];
        if (typeof underlyingSinkDict["start"] !== "function")
            @throwTypeError("underlyingSink.start should be a function");
    }
    if ("write" in underlyingSink) {
        underlyingSinkDict["write"] = underlyingSink["write"];
        if (typeof underlyingSinkDict["write"] !== "function")
            @throwTypeError("underlyingSink.write should be a function");
    }
    if ("close" in underlyingSink) {
        underlyingSinkDict["close"] = underlyingSink["close"];
        if (typeof underlyingSinkDict["close"] !== "function")
            @throwTypeError("underlyingSink.close should be a function");
    }
    if ("abort" in underlyingSink) {
        underlyingSinkDict["abort"] = underlyingSink["abort"];
        if (typeof underlyingSinkDict["abort"] !== "function")
            @throwTypeError("underlyingSink.abort should be a function");
    }

    @privateInitializeWritableStream(this, underlyingSink);
    @setUpWritableStreamDefaultControllerFromUnderlyingSink(this, underlyingSink, underlyingSinkDict, highWaterMark, sizeAlgorithm);

    return this;
}

@getter
function locked()
{
    "use strict";

    if (!@isWritableStream(this))
        throw @makeThisTypeError("WritableStream", "locked");

    return @isWritableStreamLocked(this);
}

function abort(reason)
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(@makeThisTypeError("WritableStream", "abort"));

    if (@isWritableStreamLocked(this))
        return @Promise.@reject(@makeTypeError("WritableStream.abort method can only be used on non locked WritableStream"));

    return @writableStreamAbort(this, reason);
}

function close()
{
    "use strict";

    if (!@isWritableStream(this))
        return @Promise.@reject(@makeThisTypeError("WritableStream", "close"));

    if (@isWritableStreamLocked(this))
        return @Promise.@reject(@makeTypeError("WritableStream.close method can only be used on non locked WritableStream"));

    if (@writableStreamCloseQueuedOrInFlight(this))
        return @Promise.@reject(@makeTypeError("WritableStream.close method can only be used on a being close WritableStream"));

    return @writableStreamClose(this);
}

function getWriter()
{
    "use strict";

    if (!@isWritableStream(this))
        throw @makeThisTypeError("WritableStream", "getWriter");

    return @acquireWritableStreamDefaultWriter(this);
}
