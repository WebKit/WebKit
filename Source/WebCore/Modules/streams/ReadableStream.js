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

function initializeReadableStream(underlyingSource, strategy)
{
    "use strict";

     if (typeof underlyingSource === "undefined")
         underlyingSource = { };
     if (typeof strategy === "undefined")
         strategy = { highWaterMark: 1, size: function() { return 1; } };

    if (!@isObject(underlyingSource))
        throw new @TypeError("ReadableStream constructor takes an object as first argument");

    if (strategy !== undefined && !@isObject(strategy))
        throw new @TypeError("ReadableStream constructor takes an object as second argument, if any");

    this.@underlyingSource = underlyingSource;

    this.@queue = [];
    this.@queueSize = 0;
    this.@state = @readableStreamReadable;
    this.@started = false;
    this.@closeRequested = false;
    this.@pullAgain = false;
    this.@pulling = false;
    this.@reader = undefined;
    this.@storedError = undefined;
    this.@controller = new @ReadableStreamController(this);
    this.@strategySize = strategy.size;
    this.@highWaterMark = Number(strategy.highWaterMark);

    if (Number.isNaN(this.@highWaterMark))
        throw new TypeError("highWaterMark parameter is not correct");
    if (this.@highWaterMark < 0)
        throw new RangeError("highWaterMark is negative");

    var result = @invokeOrNoop(underlyingSource, "start", [this.@controller]);
    var _this = this;
    Promise.resolve(result).then(function() {
        _this.@started = true;
        @requestReadableStreamPull(_this);
    }, function(error) {
        if (_this.@state === @readableStreamReadable)
            @errorReadableStream(_this, error);
    });

    return this;
}

function cancel(reason)
{
    "use strict";

    if (!@isReadableStream(this))
        return Promise.reject(new @TypeError("Function should be called on a ReadableStream"));

    if (@isReadableStreamLocked(this))
        return Promise.reject(new @TypeError("ReadableStream is locked"));

    return @cancelReadableStream(this, reason);
}

function getReader()
{
    "use strict";

    if (!@isReadableStream(this))
        throw new @TypeError("Function should be called on a ReadableStream");

    if (@isReadableStreamLocked(this))
        throw new @TypeError("ReadableStream is locked");

    return new @ReadableStreamReader(this);
}

function pipeThrough(streams, options)
{
    "use strict";

    this.pipeTo(streams.writable, options);
    return streams.readable;
}

function pipeTo(dest)
{
    "use strict";

    throw new @TypeError("pipeTo is not implemented");
}

function tee()
{
    "use strict";

    if (!@isReadableStream(this))
        throw new @TypeError("Function should be called on a ReadableStream");

    throw new @TypeError("tee is not implemented");
}

function locked()
{
    "use strict";

    if (!@isReadableStream(this))
        throw new @TypeError("Function should be called on a ReadableStream");

    return @isReadableStreamLocked(this);
}
