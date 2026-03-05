/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

function reduce(callback /*, initialValue */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.reduce requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.reduce callback must be a function");

    var argumentCount = @argumentCount();
    if (length === 0 && argumentCount < 2)
        @throwTypeError("reduce of empty array with no initial value");

    var accumulator, k = 0;
    if (argumentCount > 1)
        accumulator = @argument(1);
    else {
        while (k < length && !(k in array))
            k += 1;
        if (k >= length)
            @throwTypeError("reduce of empty array with no initial value");
        accumulator = array[k++];
    }

    while (k < length) {
        if (k in array)
            accumulator = callback.@call(@undefined, accumulator, array[k], k, array);
        k += 1;
    }
    return accumulator;
}

function reduceRight(callback /*, initialValue */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.reduceRight requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.reduceRight callback must be a function");

    var argumentCount = @argumentCount();
    if (length === 0 && argumentCount < 2)
        @throwTypeError("reduceRight of empty array with no initial value");

    var accumulator, k = length - 1;
    if (argumentCount > 1)
        accumulator = @argument(1);
    else {
        while (k >= 0 && !(k in array))
            k -= 1;
        if (k < 0)
            @throwTypeError("reduceRight of empty array with no initial value");
        accumulator = array[k--];
    }

    while (k >= 0) {
        if (k in array)
            accumulator = callback.@call(@undefined, accumulator, array[k], k, array);
        k -= 1;
    }
    return accumulator;
}

function every(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.every requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.every callback must be a function");
    
    var thisArg = @argument(1);
    
    for (var i = 0; i < length; i++) {
        if (!(i in array))
            continue;
        if (!callback.@call(thisArg, array[i], i, array))
            return false;
    }
    
    return true;
}

function forEach(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.forEach requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.forEach callback must be a function");
    
    var thisArg = @argument(1);
    
    for (var i = 0; i < length; i++) {
        if (i in array)
            callback.@call(thisArg, array[i], i, array);
    }
}

@alwaysInline
function filter(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.filter requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.filter callback must be a function");
    
    var thisArg = @argument(1);
    var result = @newArrayWithSpecies(0, array);

    var nextIndex = 0;
    for (var i = 0; i < length; i++) {
        if (!(i in array))
            continue;
        var current = array[i]
        if (callback.@call(thisArg, current, i, array)) {
            @putByValDirect(result, nextIndex, current);
            ++nextIndex;
        }
    }
    return result;
}

function map(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.map requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.map callback must be a function");
    
    var thisArg = @argument(1);
    var result = @newArrayWithSpecies(length, array);

    for (var i = 0; i < length; i++) {
        if (!(i in array))
            continue;
        var mappedValue = callback.@call(thisArg, array[i], i, array);
        @putByValDirect(result, i, mappedValue);
    }
    return result;
}

function some(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.some requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.some callback must be a function");
    
    var thisArg = @argument(1);
    for (var i = 0; i < length; i++) {
        if (!(i in array))
            continue;
        if (callback.@call(thisArg, array[i], i, array))
            return true;
    }
    return false;
}

function find(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.find requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.find callback must be a function");
    
    var thisArg = @argument(1);
    for (var i = 0; i < length; i++) {
        var kValue = array[i];
        if (callback.@call(thisArg, kValue, i, array))
            return kValue;
    }
    return @undefined;
}

function findLast(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.findLast requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.findLast callback must be a function");

    var thisArg = @argument(1);
    for (var i = length - 1; i >= 0; i--) {
        var element = array[i];
        if (callback.@call(thisArg, element, i, array))
            return element;
    }
    return @undefined;
}

function findIndex(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.findIndex requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.findIndex callback must be a function");
    
    var thisArg = @argument(1);
    for (var i = 0; i < length; i++) {
        if (callback.@call(thisArg, array[i], i, array))
            return i;
    }
    return -1;
}

function findLastIndex(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.findLastIndex requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.findLastIndex callback must be a function");

    var thisArg = @argument(1);
    for (var i = length - 1; i >= 0; i--) {
        if (callback.@call(thisArg, array[i], i, array))
            return i;
    }
    return -1;
}

@linkTimeConstant
function flatIntoArray(target, source, sourceLength, targetIndex, depth)
{
    "use strict";

    for (var sourceIndex = 0; sourceIndex < sourceLength; ++sourceIndex) {
        if (sourceIndex in source) {
            var element = source[sourceIndex];
            if (depth > 0 && @isArray(element))
                targetIndex = @flatIntoArray(target, element, @toLength(element.length), targetIndex, depth - 1);
            else {
                if (targetIndex >= @MAX_SAFE_INTEGER)
                    @throwTypeError("flatten array exceeds 2**53 - 1");
                @putByValDirect(target, targetIndex, element);
                ++targetIndex;
            }
        }
    }
    return targetIndex;
}

@linkTimeConstant
function flatIntoArrayWithCallback(target, source, sourceLength, targetIndex, callback, thisArg)
{
    "use strict";

    for (var sourceIndex = 0; sourceIndex < sourceLength; ++sourceIndex) {
        if (sourceIndex in source) {
            var element = callback.@call(thisArg, source[sourceIndex], sourceIndex, source);
            if (@isArray(element))
                targetIndex = @flatIntoArray(target, element, @toLength(element.length), targetIndex, 0);
            else {
                if (targetIndex >= @MAX_SAFE_INTEGER)
                    @throwTypeError("flatten array exceeds 2**53 - 1");
                @putByValDirect(target, targetIndex, element);
                ++targetIndex;
            }
        }
    }
    return target;
}

function flatMap(callback)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.flatMap requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.flatMap callback must be a function");

    var thisArg = @argument(1);

    var result = @newArrayWithSpecies(0, array);

    return @flatIntoArrayWithCallback(result, array, length, 0, callback, thisArg);
}

function at(index)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.at requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    var k = @toIntegerOrInfinity(index);
    if (k < 0)
        k += length;

    return (k >= 0 && k < length) ? array[k] : @undefined;
}
