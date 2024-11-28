/*
* Copyright (C) 2022 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1.  Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
* 2.  Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

function initializeDecompressionStream(format)
{
    "use strict";

    const errorMessage = "DecompressionStream requires a single argument with the value 'brotli', 'deflate', 'deflate-raw', or 'gzip'.";

    if (arguments.length < 1)
        @throwTypeError(errorMessage);

    const algorithms = ['brotli', 'gzip', 'deflate', 'deflate-raw'];
    const lowercaseFormat = @toString(arguments[0]).toLowerCase();
    const findAlgorithm = (element) => element === lowercaseFormat;

    // Pass the index to our new DecompressionStreamDecoder, so we do not need to reparse the string.
    // We need to ensure that the Formats.h and this file stay in sync.
    const index = algorithms.findIndex(findAlgorithm);

    if (index === -1)
        @throwTypeError(errorMessage);

    // Setup Transform and Flush Algorithms
    const startAlgorithm = () => {
        return @Promise.@resolve();
    };
    const transformAlgorithm = (chunk) => {
        if (!@isObject(chunk) || (!(chunk instanceof @ArrayBuffer) && !(chunk.buffer instanceof @ArrayBuffer)))
            return @Promise.@reject(@makeTypeError("Invalid type should be ArrayBuffer"));

        try {
            const decoder = @getByIdDirectPrivate(this, "DecompressionStreamDecoder");
            const buffer = decoder.@decode(chunk);

            if (buffer) {
                const transformStream = @getByIdDirectPrivate(this, "DecompressionStreamTransform");
                const controller = @getByIdDirectPrivate(transformStream, "controller");
                @transformStreamDefaultControllerEnqueue(controller, buffer);
            }
        } catch (e) {
            return @Promise.@reject(@makeTypeError(e.message));
        }

        return @Promise.@resolve();
    };
    const flushAlgorithm = () => {        
        try {
            const decoder = @getByIdDirectPrivate(this, "DecompressionStreamDecoder");
            const buffer = decoder.@flush();

            if (buffer) {
                const transformStream = @getByIdDirectPrivate(this, "DecompressionStreamTransform");
                const controller = @getByIdDirectPrivate(transformStream, "controller");
                @transformStreamDefaultControllerEnqueue(controller, buffer);
            }
        } catch (e) {
            return @Promise.@reject(@makeTypeError(e.message));
        }

        return @Promise.@resolve();
    };

    const [transform, readable, writable] = @createTransformStream(startAlgorithm, transformAlgorithm, flushAlgorithm);
    @putByIdDirectPrivate(this, "DecompressionStreamTransform", transform);
    @putByIdDirectPrivate(this, "readable", readable);
    @putByIdDirectPrivate(this, "writable", writable);
    @putByIdDirectPrivate(this, "DecompressionStreamDecoder", new @DecompressionStreamDecoder(index));
    return this;
}

@getter
function readable()
{
    "use strict";

    const transform = @getByIdDirectPrivate(this, "DecompressionStreamTransform");
    if (!transform)
        throw @makeThisTypeError("DecompressionStreamDecoder", "readable");

    return @getByIdDirectPrivate(this, "readable");
}

@getter
function writable()
{
    "use strict";

    const transform = @getByIdDirectPrivate(this, "DecompressionStreamTransform");
    if (!transform)
        throw @makeThisTypeError("DecompressionStreamDecoder", "writable");

    return @getByIdDirectPrivate(this, "writable");
}
