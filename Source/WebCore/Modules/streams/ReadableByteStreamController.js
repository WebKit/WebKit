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

    if (!@isReadableByteStreamController(this))
        throw @makeThisTypeError("ReadableByteStreamController", "enqueue");

    if (this.@closeRequested)
        @throwTypeError("ReadableByteStreamController is requested to close");

    if (this.@controlledReadableStream.@state !== @streamReadable)
        @throwTypeError("ReadableStream is not readable");

    if (!@isObject(chunk))
        @throwTypeError("Provided chunk is not an object");

    if (!@ArrayBuffer.@isView(chunk))
        @throwTypeError("Provided chunk is not an ArrayBuffer view");

    return @readableByteStreamControllerEnqueue(this, chunk);
}

function error(error)
{
    "use strict";

    if (!@isReadableByteStreamController(this))
        throw @makeThisTypeError("ReadableByteStreamController", "error");

    if (this.@controlledReadableStream.@state !== @streamReadable)
        @throwTypeError("ReadableStream is not readable");

    @readableByteStreamControllerError(this, error);
}

function close()
{
    "use strict";

    if (!@isReadableByteStreamController(this))
        throw @makeThisTypeError("ReadableByteStreamController", "close");

    if (this.@closeRequested)
        @throwTypeError("Close has already been requested");

    if (this.@controlledReadableStream.@state !== @streamReadable)
        @throwTypeError("ReadableStream is not readable");

    @readableByteStreamControllerClose(this);
}

@getter
function byobRequest()
{
    "use strict";

    if (!@isReadableByteStreamController(this))
        throw @makeGetterTypeError("ReadableByteStreamController", "byobRequest");

    if (this.@byobRequest === @undefined && this.@pendingPullIntos.length) {
        const firstDescriptor = this.@pendingPullIntos[0];
        const view = new @Uint8Array(firstDescriptor.buffer,
            firstDescriptor.byteOffset + firstDescriptor.bytesFilled,
            firstDescriptor.byteLength - firstDescriptor.bytesFilled);
        this.@byobRequest = new @ReadableStreamBYOBRequest(this, view);
    }

    return this.@byobRequest;
}

@getter
function desiredSize()
{
    "use strict";

    if (!@isReadableByteStreamController(this))
        throw @makeGetterTypeError("ReadableByteStreamController", "desiredSize");

    return @readableByteStreamControllerGetDesiredSize(this);
}
