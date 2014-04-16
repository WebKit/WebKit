/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

function every(callback /*, thisArg */) {
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.every requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.every requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = array.length >>> 0;
    
    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.every callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    
    for (var i = 0; i < length; i++) {
        if (!(i in array))
            continue;
        if (!callback.@call(thisArg, array[i], i, array))
            return false;
    }
    
    return true;
}

function forEach(callback /*, thisArg */) {
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.forEach requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.forEach requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = array.length >>> 0;
    
    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.forEach callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    
    for (var i = 0; i < length; i++) {
        if (i in array)
            callback.@call(thisArg, array[i], i, array);
    }
}

function filter(callback /*, thisArg */) {
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.filter requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.filter requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = array.length >>> 0;
    
    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.filter callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    var result = [];
    var nextIndex = 0;
    for (var i = 0; i < length; i++) {
        if (!(i in array))
            continue;
        var current = array[i]
        if (callback.@call(thisArg, current, i, array))
            result[nextIndex++] = current;
    }
    return result;
}

function map(callback /*, thisArg */) {
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.map requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.map requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = array.length >>> 0;
    
    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.map callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    var result = [];
    result.length = length;
    var nextIndex = 0;
    for (var i = 0; i < length; i++) {
        if (!(i in array))
            continue;
        result[i] = callback.@call(thisArg, array[i], i, array)
    }
    return result;
}

function some(callback /*, thisArg */) {
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.some requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.some requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = array.length >>> 0;
    
    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.some callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    for (var i = 0; i < length; i++) {
        if (!(i in array))
            continue;
        if (callback.@call(thisArg, array[i], i, array))
            return true;
    }
    return false;
}

function fill(value /* [, start [, end]] */)
{
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.fill requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.fill requires that |this| not be undefined");
    var O = @Object(this);
    var len = O.length >>> 0;
    var relativeStart = 0;
    if (arguments.length > 1 && arguments[1] !== undefined)
        relativeStart = arguments[1] | 0;
    var k = 0;
    if (relativeStart < 0) {
        k = len + relativeStart;
        if (k < 0)
            k = 0;
    } else {
        k = relativeStart;
        if (k > len)
            k = len;
    }
    var relativeEnd = len;
    if (arguments.length > 2 && arguments[2] !== undefined)
        relativeEnd = arguments[2] | 0;
    var final = 0;
    if (relativeEnd < 0) {
        final = len + relativeEnd;
        if (final < 0)
            final = 0;
    } else {
        final = relativeEnd;
        if (final > len)
            final = len;
    }
    for (; k < final; k++)
        O[k] = value;
    return O;
}

