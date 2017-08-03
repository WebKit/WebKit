/*
 * Copyright (C) 2016 Apple Inc.
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

function fillFetchHeaders(headers, headersInit)
{
    "use strict";

    if (headersInit === @undefined)
        return;

    if (headersInit instanceof @Headers) {
        @Headers.prototype.@fillFromJS.@call(headers, headersInit);
        return;
    }

    if (@isArray(headersInit)) {
        for (let i = 0; i < headersInit.length; i++) {
            let header = headersInit[i];
            if (header.length !== 2)
                @throwTypeError("headersInit sequence items should contain two values");
            @Headers.prototype.@appendFromJS.@call(headers, header[0], header[1]);
        }
        return this;
    }

    let propertyNames = @Object.@getOwnPropertyNames(headersInit);
    for (let i = 0; i < propertyNames.length; ++i) {
        let name = propertyNames[i];
        @Headers.prototype.@appendFromJS.@call(headers, name, headersInit[name]);
    }
}

function consumeStream(response, type)
{
    @assert(response instanceof @Response);
    @assert(response.@body instanceof @ReadableStream);

    if (@isReadableStreamDisturbed(response.@body))
        return @Promise.@reject(new @TypeError("Cannot consume a disturbed Response body ReadableStream"));

    try {
        let reader = new @ReadableStreamDefaultReader(response.@body);

        @Response.prototype.@startConsumingStream.@call(response, type);
        let pull = (result) => {
            if (result.done)
                return @Response.prototype.@finishConsumingStream.@call(response);
            @Response.prototype.@consumeChunk.@call(response, result.value);
            return @Promise.prototype.@then.@call(@readableStreamDefaultReaderRead(reader), pull);
        }
        return @Promise.prototype.@then.@call(@readableStreamDefaultReaderRead(reader), pull);
    } catch (e) {
        return @Promise.@reject(e);
    }
}
