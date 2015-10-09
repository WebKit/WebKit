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

// @optional=STREAMS_API

function cancel(reason)
{
    "use strict";

    if (!@isReadableStreamReader(this))
        return Promise.reject(new @TypeError("Function should be called on a ReadableStreamReader"));

    if (this.@state === @readableStreamClosed)
        return Promise.resolve();

    if (this.@state === @readableStreamErrored)
        return Promise.reject(this.@storedError);

    // TODO ASSERT(@isReadableStream(this.@ownerReadableStream));
    // TODO ASSERT(this.@ownerReadableStream.@state === @readableStreamReadable);
    return @cancelReadableStream(this.@ownerReadableStream, reason);
}

function read()
{
    "use strict";

    if (!@isReadableStreamReader(this))
        return Promise.reject(new @TypeError("Function should be called on a ReadableStreamReader"));

    return @readFromReadableStreamReader(this);
}

function releaseLock()
{
    "use strict";

    if (!@isReadableStreamReader(this))
        throw new @TypeError("Function should be called on a ReadableStreamReader");

    if (!this.@ownerReadableStream)
         return;

    if (this.@readRequests.length)
        throw new @TypeError();

    @closeReadableStreamReader(this);
}

function closed()
{
    "use strict";

    if (!@isReadableStreamReader(this))
        return Promise.reject(new @TypeError("Callee of closed is not a ReadableStreamReader"));

    return this.@closedPromise;
}
