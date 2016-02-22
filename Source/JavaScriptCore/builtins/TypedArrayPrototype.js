/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

// Note that the intrisic @typedArrayLength checks the that the argument passed is a typed array
// and throws if it is not.

function every(callback /*, thisArg */)
{
    "use strict";
    var length = @typedArrayLength(this);
    var thisArg = arguments.length > 1 ? arguments[1] : @undefined;

    if (typeof callback !== "function")
        throw new @TypeError("TypedArray.prototype.every callback must be a function");

    for (var i = 0; i < length; i++) {
        if (!callback.@call(thisArg, this[i], i, this))
            return false;
    }

    return true;
}

function find(callback /* [, thisArg] */)
{
    "use strict";
    var length = @typedArrayLength(this);
    var thisArg = arguments.length > 1 ? arguments[1] : @undefined;

    if (typeof callback !== "function")
        throw new @TypeError("TypedArray.prototype.find callback must be a function");

    for (var i = 0; i < length; i++) {
        let elem = this[i];
        if (callback.@call(thisArg, elem, i, this))
            return elem;
    }
    return @undefined;
}

function findIndex(callback /* [, thisArg] */)
{
    "use strict";
    var length = @typedArrayLength(this);
    var thisArg = arguments.length > 1 ? arguments[1] : @undefined;

    if (typeof callback !== "function")
        throw new @TypeError("TypedArray.prototype.findIndex callback must be a function");

    for (var i = 0; i < length; i++) {
        if (callback.@call(thisArg, this[i], i, this))
            return i;
    }
    return -1;
}

function forEach(callback /* [, thisArg] */)
{
    "use strict";
    var length = @typedArrayLength(this);
    var thisArg = arguments.length > 1 ? arguments[1] : @undefined;

    if (typeof callback !== "function")
        throw new @TypeError("TypedArray.prototype.forEach callback must be a function");

    for (var i = 0; i < length; i++)
        callback.@call(thisArg, this[i], i, this);
}

function some(callback /* [, thisArg] */)
{
    // 22.2.3.24
    "use strict";
    var length = @typedArrayLength(this);
    var thisArg = arguments.length > 1 ? arguments[1] : @undefined;

    if (typeof callback !== "function")
        throw new @TypeError("TypedArray.prototype.some callback must be a function");

    for (var i = 0; i < length; i++) {
        if (callback.@call(thisArg, this[i], i, this))
            return true;
    }

    return false;
}

function sort(comparator)
{
    // 22.2.3.25
    "use strict";

    function min(a, b)
    {
        return a < b ? a : b;
    }

    function merge(dst, src, srcIndex, srcEnd, width, comparator)
    {
        var left = srcIndex;
        var leftEnd = min(left + width, srcEnd);
        var right = leftEnd;
        var rightEnd = min(right + width, srcEnd);

        for (var dstIndex = left; dstIndex < rightEnd; ++dstIndex) {
            if (right < rightEnd) {
                if (left >= leftEnd || comparator(src[right], src[left]) < 0) {
                    dst[dstIndex] = src[right++];
                    continue;
                }
            }

            dst[dstIndex] = src[left++];
        }
    }

    function mergeSort(array, valueCount, comparator)
    {
        var buffer = [ ];
        buffer.length = valueCount;

        var dst = buffer;
        var src = array;

        for (var width = 1; width < valueCount; width *= 2) {
            for (var srcIndex = 0; srcIndex < valueCount; srcIndex += 2 * width)
                merge(dst, src, srcIndex, valueCount, width, comparator);

            var tmp = src;
            src = dst;
            dst = tmp;
        }

        if (src != array) {
            for(var i = 0; i < valueCount; i++)
                array[i] = src[i];
        }
    }

    var length = @typedArrayLength(this);

    if (length < 2)
        return;

    if (typeof comparator == "function")
        mergeSort(this, length, comparator);
    else
        @typedArraySort(this);
    
    return this;
}

function reduce(callback /* [, initialValue] */)
{
    // 22.2.3.19
    "use strict";

    var length = @typedArrayLength(this);

    if (typeof callback !== "function")
        throw new @TypeError("TypedArray.prototype.reduce callback must be a function");

    if (length === 0 && arguments.length < 2)
        throw new @TypeError("TypedArray.prototype.reduce of empty array with no initial value");

    var accumulator, k = 0;
    if (arguments.length > 1)
        accumulator = arguments[1];
    else
        accumulator = this[k++];

    for (; k < length; k++)
        accumulator = callback.@call(@undefined, accumulator, this[k], k, this);

    return accumulator;
}

function reduceRight(callback /* [, initialValue] */)
{
    // 22.2.3.20
    "use strict";

    var length = @typedArrayLength(this);

    if (typeof callback !== "function")
        throw new @TypeError("TypedArray.prototype.reduceRight callback must be a function");

    if (length === 0 && arguments.length < 2)
        throw new @TypeError("TypedArray.prototype.reduceRight of empty array with no initial value");

    var accumulator, k = length - 1;
    if (arguments.length > 1)
        accumulator = arguments[1];
    else
        accumulator = this[k--];

    for (; k >= 0; k--)
        accumulator = callback.@call(@undefined, accumulator, this[k], k, this);

    return accumulator;
}

function map(callback /*, thisArg */)
{
    // 22.2.3.18
    "use strict";

    var length = @typedArrayLength(this);

    if (typeof callback !== "function")
        throw new @TypeError("TypedArray.prototype.map callback must be a function");

    var thisArg = arguments.length > 1 ? arguments[1] : @undefined;

    // Do species construction
    var constructor = this.constructor;
    var result;
    if (constructor === @undefined)
        result = new (@typedArrayGetOriginalConstructor(this))(length);
    else {
        var speciesConstructor = @Object(constructor)[@symbolSpecies];
        if (speciesConstructor === null || speciesConstructor === @undefined)
            result = new (@typedArrayGetOriginalConstructor(this))(length);
        else {
            result = new speciesConstructor(length);
            // typedArrayLength throws if it doesn't get a view.
            @typedArrayLength(result);
        }
    }

    for (var i = 0; i < length; i++) {
        var mappedValue = callback.@call(thisArg, this[i], i, this);
        result[i] = mappedValue;
    }
    return result;
}

function filter(callback /*, thisArg */)
{
    "use strict";

    var length = @typedArrayLength(this);

    if (typeof callback !== "function")
        throw new @TypeError("TypedArray.prototype.filter callback must be a function");

    var thisArg = arguments.length > 1 ? arguments[1] : @undefined;
    var kept = [];

    for (var i = 0; i < length; i++) {
        var value = this[i];
        if (callback.@call(thisArg, value, i, this))
            kept.@push(value);
    }

    var constructor = this.constructor;
    var result;
    var resultLength = kept.length;
    if (constructor === @undefined)
        result = new (@typedArrayGetOriginalConstructor(this))(resultLength);
    else {
        var speciesConstructor = @Object(constructor)[@symbolSpecies];
        if (speciesConstructor === null || speciesConstructor === @undefined)
            result = new (@typedArrayGetOriginalConstructor(this))(resultLength);
        else {
            result = new speciesConstructor(resultLength);
            // typedArrayLength throws if it doesn't get a view.
            @typedArrayLength(result);
        }
    }

    for (var i = 0; i < kept.length; i++)
        result[i] = kept[i];

    return result;
}

function toLocaleString()
{
    "use strict";

    var length = @typedArrayLength(this);

    if (length == 0)
        return "";

    var string = this[0].toLocaleString();
    for (var i = 1; i < length; i++)
        string += "," + this[i].toLocaleString();

    return string;
}
