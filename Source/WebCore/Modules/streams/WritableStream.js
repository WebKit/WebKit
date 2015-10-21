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

function initializeWritableStream(underlyingSink, strategy)
{
    "use strict";

    if (typeof underlyingSink === "undefined")
        underlyingSink = { };
    if (typeof strategy === "undefined")
        strategy = { highWaterMark: 0, size: function() { return 1; } };

    if (!@isObject(underlyingSink))
        throw new @TypeError("WritableStream constructor takes an object as first argument");

    if (!@isObject(strategy))
        throw new @TypeError("WritableStream constructor takes an object as second argument, if any");

    this.@underlyingSink = underlyingSink;
    this.@closedPromise = @createNewStreamsPromise();
    this.@readyPromise = Promise.resolve(undefined);
    this.@queue = @newQueue();
    this.@started = false;
    this.@writing = false;

    this.@strategy = @validateAndNormalizeQueuingStrategy(strategy.size, strategy.highWaterMark);

    @syncWritableStreamStateWithQueue(this);

    var error = @errorWritableStream.bind(this);
    var startResult = @invokeOrNoop(underlyingSink, "start", [error]);
    this.@startedPromise = Promise.resolve(startResult);
    var _this = this;
    this.@startedPromise.then(function() {
        _this.@started = true;
        _this.@startedPromise = undefined;
    }, function(r) {
        error(r);
    });

    return this;
}

function abort(reason)
{
    "use strict";

    throw new EvalError("abort not implemented");
}

function close()
{
    "use strict";

    throw new EvalError("close not implemented");
}

function write(chunk)
{
    "use strict";

    throw new EvalError("write not implemented");
}

function closed()
{
    "use strict";

    throw new EvalError("closed not implemented");
}

function ready()
{
    "use strict";

    throw new EvalError("ready not implemented");
}

function state()
{
    "use strict";

    throw new EvalError("state not implemented");
}
