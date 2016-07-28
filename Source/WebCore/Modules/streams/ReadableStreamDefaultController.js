/*
 * Copyright (C) 2015 Canon Inc.
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

function enqueue(chunk)
{
    "use strict";

    if (!@isReadableStreamDefaultController(this))
        throw @makeThisTypeError("ReadableStreamDefaultController", "enqueue");

    const stream = this.@controlledReadableStream;

    if (stream.@closeRequested)
        throw new @TypeError("ReadableStream is requested to close");

    if (stream.@state !== @streamReadable)
        throw new @TypeError("ReadableStream is not readable");

    return @enqueueInReadableStream(stream, chunk);
}

function error(error)
{
    "use strict";

    if (!@isReadableStreamDefaultController(this))
        throw @makeThisTypeError("ReadableStreamDefaultController", "error");

    const stream = this.@controlledReadableStream;
    if (stream.@state !== @streamReadable)
        throw new @TypeError("ReadableStream is not readable");

    @errorReadableStream(stream, error);
}

function close()
{
    "use strict";

    if (!@isReadableStreamDefaultController(this))
        throw @makeThisTypeError("ReadableStreamDefaultController", "close");

    const stream = this.@controlledReadableStream;
    if (stream.@closeRequested)
        throw new @TypeError("ReadableStream is already requested to close");

    if (stream.@state !== @streamReadable)
        throw new @TypeError("ReadableStream is not readable");

    @closeReadableStream(stream);
}

function desiredSize()
{
    "use strict";

    if (!@isReadableStreamDefaultController(this))
        throw @makeGetterTypeError("ReadableStreamDefaultController", "desiredSize");

    return @getReadableStreamDesiredSize(this.@controlledReadableStream);
}
