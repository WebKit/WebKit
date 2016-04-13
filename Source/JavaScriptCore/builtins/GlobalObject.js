/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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

function toInteger(target)
{
    "use strict";

    var numberValue = @Number(target);

    // isNaN(numberValue)
    if (numberValue !== numberValue)
        return 0;
    return @trunc(numberValue);
}

function toLength(target)
{
    "use strict";

    var maxSafeInteger = 0x1FFFFFFFFFFFFF;
    var length = @toInteger(target);
    // originally Math.min(Math.max(length, 0), maxSafeInteger));
    return length > 0 ? (length < maxSafeInteger ? length : maxSafeInteger) : 0;
}

function isDictionary(object)
{
    "use strict";

    return object === @undefined || object == null || typeof object === "object";
}

// FIXME: this needs to have it's name changed to "get [Symbol.species]".
// see: https://bugs.webkit.org/show_bug.cgi?id=151363
function speciesGetter()
{
    return this;
}
