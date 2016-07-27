/*
 * Copyright (C) 2016 Canon Inc.
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
 * THIS SOFTWARE IS PROVIDED BY CANON INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CANON INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// @conditional=ENABLE(FETCH_API)

function initializeFetchResponse(body, init)
{
    "use strict";

    if (init === @undefined)
        init = { };
    else if (!@isObject(init))
        throw new @TypeError("Response init must be an object");

    let status = (init.status !== @undefined) ? @toNumber(init.status) : 200;
    if (status < 200  || status > 599)
        throw new @RangeError("Status must be between 200 and 599");

    let statusText = (init.statusText !== @undefined) ? init.statusText : "OK";

    this.@setStatus(status, statusText);

    if (init.headers !== @undefined)
        @fillFetchHeaders(this.headers, init.headers);

    if (body !== @undefined && body !== null) {
        if (status == 101 || status == 204 || status == 205 || status == 304)
            throw new @TypeError("Response cannot have a body with the given status");
        this.@initializeWith(body);
    }

    return this;
}

function body()
{
    if (!(this instanceof @Response))
        throw @makeGetterTypeError("Response", "body");

    if (!this.@body) {
        if (@Response.prototype.@isDisturbed.@call(this)) {
            this.@body = new @ReadableStream();
            // Get reader to lock it.
            new @ReadableStreamReader(this.@body);
        } else {
            var source = @Response.prototype.@createReadableStreamSource.@call(this);
            this.@body = source ? new @ReadableStream(source) : null;
        }
    }
    return this.@body;
}
