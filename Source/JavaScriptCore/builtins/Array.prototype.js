/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

function every(callback /*, thisArg */) {
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.every requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.every requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = @ToLength(array.length);

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
    var length = @ToLength(array.length);

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
    var length = @ToLength(array.length);

    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.filter callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    var result = [];
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

function map(callback /*, thisArg */) {
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.map requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.map requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = @ToLength(array.length);

    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.map callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    var result = [];
    result.length = length;
    var nextIndex = 0;
    for (var i = 0; i < length; i++) {
        if (!(i in array))
            continue;
        var mappedValue = callback.@call(thisArg, array[i], i, array);
        @putByValDirect(result, i, mappedValue);
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
    var length = @ToLength(array.length);

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
    var len = @ToLength(O.length);
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

function find(callback /*, thisArg */) {
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.find requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.find requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = @ToLength(array.length);

    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.find callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    for (var i = 0; i < length; i++) {
        var kValue = array[i];
        if (callback.@call(thisArg, kValue, i, array))
            return kValue;
    }
    return undefined;
}

function findIndex(callback /*, thisArg */) {
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.findIndex requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.findIndex requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = @ToLength(array.length);

    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.findIndex callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    for (var i = 0; i < length; i++) {
        if (callback.@call(thisArg, array[i], i, array))
            return i;
    }
    return -1;
}

function includes(searchElement /*, fromIndex*/) {
    "use strict";
    if (this === null)
        throw new @TypeError("Array.prototype.includes requires that |this| not be null");

    if (this === undefined)
        throw new @TypeError("Array.prototype.includes requires that |this| not be undefined");

    var array = @Object(this);
    var length = @ToLength(array.length);

    if (length === 0)
        return false;

    var fromIndex = 0;
    if (arguments.length > 1 && arguments[1] !== undefined)
        fromIndex = arguments[1] | 0;

    var index;
    if (fromIndex >= 0)
        index = fromIndex;
    else
        index = length + fromIndex;

    if (index < 0)
        index = 0;

    var currentElement;
    for (; index < length; ++index) {
        currentElement = array[index];
        // Use SameValueZero comparison, rather than just StrictEquals.
        if (searchElement === currentElement || (searchElement !== searchElement && currentElement !== currentElement))
            return true;
    }
    return false;
}

function sort(comparator)
{
    "use strict";

    function min(a, b)
    {
        return a < b ? a : b;
    }

    function stringComparator(a, b)
    {
        var aString = @toString(a);
        var bString = @toString(b);

        var aLength = aString.length;
        var bLength = bString.length;
        var length = min(aLength, bLength);

        for (var i = 0; i < length; ++i) {
            var aCharCode = aString.@charCodeAt(i);
            var bCharCode = bString.@charCodeAt(i);

            if (aCharCode == bCharCode)
                continue;

            if (aCharCode < bCharCode)
                return -1;

            return 1;
        }

        if (aLength == bLength)
            return 0;

        if (aLength < bLength)
            return -1;

        return 1;
    }

    // Move undefineds and holes to the end of a sparse array. Result is [values..., undefineds..., holes...].
    function compactSparse(array, dst, src, length)
    {
        var values = [ ];
        var seen = { };
        var valueCount = 0;
        var undefinedCount = 0;

        // Clean up after the in-progress non-sparse compaction that failed.
        for (var i = dst; i < src; ++i)
            delete array[i];

        for (var object = array; object; object = @Object.@getPrototypeOf(object)) {
            var propertyNames = @Object.@getOwnPropertyNames(object);
            for (var i = 0; i < propertyNames.length; ++i) {
                var index = propertyNames[i];
                if (index < length) { // Exclude non-numeric properties and properties past length.
                    if (seen[index]) // Exclude duplicates.
                        continue;
                    seen[index] = 1;

                    var value = array[index];
                    delete array[index];

                    if (value === undefined) {
                        ++undefinedCount;
                        continue;
                    }

                    array[valueCount++] = value;
                }
            }
        }

        for (var i = valueCount; i < valueCount + undefinedCount; ++i)
            array[i] = undefined;

        return valueCount;
    }

    // Move undefineds and holes to the end of an array. Result is [values..., undefineds..., holes...].
    function compact(array, length)
    {
        var holeCount = 0;

        for (var dst = 0, src = 0; src < length; ++src) {
            if (!(src in array)) {
                ++holeCount;
                if (holeCount < 256)
                    continue;
                return compactSparse(array, dst, src, length);
            }

            var value = array[src];
            if (value === undefined)
                continue;
            array[dst++] = value;
        }

        var valueCount = dst;
        var undefinedCount = length - valueCount - holeCount;

        for (var i = valueCount; i < valueCount + undefinedCount; ++i)
            array[i] = undefined;

        for (var i = valueCount + undefinedCount; i < length; ++i)
            delete array[i];

        return valueCount;
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

    function comparatorSort(array, comparator)
    {
        var length = array.length >>> 0;

        // For compatibility with Firefox and Chrome, do nothing observable
        // to the target array if it has 0 or 1 sortable properties.
        if (length < 2)
            return;

        var valueCount = compact(array, length);
        mergeSort(array, valueCount, comparator);
    }

    if (this === null)
        throw new @TypeError("Array.prototype.sort requires that |this| not be null");

    if (this === undefined)
        throw new @TypeError("Array.prototype.sort requires that |this| not be undefined");

    if (typeof this == "string")
        throw new @TypeError("Attempted to assign to readonly property.");

    if (typeof comparator !== "function")
        comparator = stringComparator;

    var array = @Object(this);
    comparatorSort(array, comparator);
    return array;
}

function copyWithin(target, start /*, end */)
{
    "use strict";

    function maxWithPositives(a, b)
    {
        return (a < b) ? b : a;
    }

    function minWithMaybeNegativeZeroAndPositive(maybeNegativeZero, positive)
    {
        return (maybeNegativeZero < positive) ? maybeNegativeZero : positive;
    }

    if (this === null || this === undefined)
        throw new @TypeError("Array.copyWithin requires that |this| not be null or undefined");
    var thisObject = @Object(this);

    var length = @ToLength(thisObject.length);

    var relativeTarget = @ToInteger(target);
    var to = (relativeTarget < 0) ? maxWithPositives(length + relativeTarget, 0) : minWithMaybeNegativeZeroAndPositive(relativeTarget, length);

    var relativeStart = @ToInteger(start);
    var from = (relativeStart < 0) ? maxWithPositives(length + relativeStart, 0) : minWithMaybeNegativeZeroAndPositive(relativeStart, length);

    var relativeEnd;
    if (arguments.length >= 3) {
        var end = arguments[2];
        if (end === undefined)
            relativeEnd = length;
        else
            relativeEnd = @ToInteger(end);
    } else
        relativeEnd = length;

    var finalValue = (relativeEnd < 0) ? maxWithPositives(length + relativeEnd, 0) : minWithMaybeNegativeZeroAndPositive(relativeEnd, length);

    var count = minWithMaybeNegativeZeroAndPositive(finalValue - from, length - to);

    var direction = 1;
    if (from < to && to < from + count) {
        direction = -1;
        from = from + count - 1;
        to = to + count - 1;
    }

    for (var i = 0; i < count; ++i, from += direction, to += direction) {
        if (from in thisObject)
            thisObject[to] = thisObject[from];
        else
            delete thisObject[to];
    }

    return thisObject;
}
