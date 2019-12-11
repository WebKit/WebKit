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

@constructor
@globalPrivate
function ArrayIterator(iteratedObject, kind, iterationFunction)
{
    "use strict";

    @putByIdDirectPrivate(this, "iteratedObject", iteratedObject);
    @putByIdDirectPrivate(this, "arrayIteratorKind", kind);
    @putByIdDirectPrivate(this, "arrayIteratorNextIndex", 0);
    @putByIdDirectPrivate(this, "arrayIteratorNext", iterationFunction);
    @putByIdDirectPrivate(this, "arrayIteratorIsDone", false);
}

function values()
{
    "use strict";

    return new @ArrayIterator(@toObject(this, "Array.prototype.values requires that |this| not be null or undefined"), "value", @arrayIteratorValueNext);
}

function keys()
{
    "use strict";

    return new @ArrayIterator(@toObject(this, "Array.prototype.keys requires that |this| not be null or undefined"), "key", @arrayIteratorKeyNext);
}

function entries()
{
    "use strict";

    return new @ArrayIterator(@toObject(this, "Array.prototype.entries requires that |this| not be null or undefined"), "key+value", @arrayIteratorKeyValueNext);
}

function reduce(callback /*, initialValue */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.reduce requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (typeof callback !== "function")
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

    if (typeof callback !== "function")
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

    if (typeof callback !== "function")
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

    if (typeof callback !== "function")
        @throwTypeError("Array.prototype.forEach callback must be a function");
    
    var thisArg = @argument(1);
    
    for (var i = 0; i < length; i++) {
        if (i in array)
            callback.@call(thisArg, array[i], i, array);
    }
}

function filter(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.filter requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (typeof callback !== "function")
        @throwTypeError("Array.prototype.filter callback must be a function");
    
    var thisArg = @argument(1);
    var result = @arraySpeciesCreate(array, 0);

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

    if (typeof callback !== "function")
        @throwTypeError("Array.prototype.map callback must be a function");
    
    var thisArg = @argument(1);
    var result = @arraySpeciesCreate(array, length);

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

    if (typeof callback !== "function")
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

function fill(value /* [, start [, end]] */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.fill requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    var relativeStart = @toInteger(@argument(1));
    var k = 0;
    if (relativeStart < 0) {
        k = length + relativeStart;
        if (k < 0)
            k = 0;
    } else {
        k = relativeStart;
        if (k > length)
            k = length;
    }
    var relativeEnd = length;
    var end = @argument(2);
    if (end !== @undefined)
        relativeEnd = @toInteger(end);
    var final = 0;
    if (relativeEnd < 0) {
        final = length + relativeEnd;
        if (final < 0)
            final = 0;
    } else {
        final = relativeEnd;
        if (final > length)
            final = length;
    }
    for (; k < final; k++)
        array[k] = value;
    return array;
}

function find(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.find requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (typeof callback !== "function")
        @throwTypeError("Array.prototype.find callback must be a function");
    
    var thisArg = @argument(1);
    for (var i = 0; i < length; i++) {
        var kValue = array[i];
        if (callback.@call(thisArg, kValue, i, array))
            return kValue;
    }
    return @undefined;
}

function findIndex(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.findIndex requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (typeof callback !== "function")
        @throwTypeError("Array.prototype.findIndex callback must be a function");
    
    var thisArg = @argument(1);
    for (var i = 0; i < length; i++) {
        if (callback.@call(thisArg, array[i], i, array))
            return i;
    }
    return -1;
}

function includes(searchElement /*, fromIndex*/)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.includes requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (length === 0)
        return false;

    var fromIndex = 0;
    var from = @argument(1);
    if (from !== @undefined)
        fromIndex = @toInteger(from);

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
        let aString = a.string;
        let bString = b.string;

        let aLength = aString.length;
        let bLength = bString.length;
        let length = min(aLength, bLength);

        for (let i = 0; i < length; ++i) {
            let aCharCode = aString.@charCodeAt(i);
            let bCharCode = bString.@charCodeAt(i);

            if (aCharCode == bCharCode)
                continue;

            return aCharCode - bCharCode;
        }

        return aLength - bLength;
    }

    // Move undefineds and holes to the end of a sparse array. Result is [values..., undefineds..., holes...].
    function compactSparse(array, dst, src, length)
    {
        let values = [ ];
        let seen = { };
        let valueCount = 0;
        let undefinedCount = 0;

        // Clean up after the in-progress non-sparse compaction that failed.
        for (let i = dst; i < src; ++i)
            delete array[i];

        for (let object = array; object; object = @Object.@getPrototypeOf(object)) {
            let propertyNames = @Object.@getOwnPropertyNames(object);
            for (let i = 0; i < propertyNames.length; ++i) {
                let index = propertyNames[i];
                if (index < length) { // Exclude non-numeric properties and properties past length.
                    if (seen[index]) // Exclude duplicates.
                        continue;
                    seen[index] = 1;

                    let value = array[index];
                    delete array[index];

                    if (value === @undefined) {
                        ++undefinedCount;
                        continue;
                    }

                    array[valueCount++] = value;
                }
            }
        }

        for (let i = valueCount; i < valueCount + undefinedCount; ++i)
            array[i] = @undefined;

        return valueCount;
    }

    function compactSlow(array, length)
    {
        let holeCount = 0;

        let dst = 0;
        let src = 0;
        for (; src < length; ++src) {
            if (!(src in array)) {
                ++holeCount;
                if (holeCount < 256)
                    continue;
                return compactSparse(array, dst, src, length);
            }

            let value = array[src];
            if (value === @undefined)
                continue;

            array[dst++] = value;
        }

        let valueCount = dst;
        let undefinedCount = length - valueCount - holeCount;

        for (let i = valueCount; i < valueCount + undefinedCount; ++i)
            array[i] = @undefined;

        for (let i = valueCount + undefinedCount; i < length; ++i)
            delete array[i];

        return valueCount;
    }

    // Move undefineds and holes to the end of an array. Result is [values..., undefineds..., holes...].
    function compact(array, length)
    {
        for (let i = 0; i < array.length; ++i) {
            if (array[i] === @undefined)
                return compactSlow(array, length);
        }

        return length;
    }

    function merge(dst, src, srcIndex, srcEnd, width, comparator)
    {
        let left = srcIndex;
        let leftEnd = min(left + width, srcEnd);
        let right = leftEnd;
        let rightEnd = min(right + width, srcEnd);

        for (let dstIndex = left; dstIndex < rightEnd; ++dstIndex) {
            if (right < rightEnd) {
                if (left >= leftEnd) {
                    dst[dstIndex] = src[right++];
                    continue;
                }

                let comparisonResult = comparator(src[right], src[left]);
                if ((typeof comparisonResult === "boolean" && !comparisonResult) || comparisonResult < 0) {
                    dst[dstIndex] = src[right++];
                    continue;
                }

            }

            dst[dstIndex] = src[left++];
        }
    }

    function mergeSort(array, valueCount, comparator)
    {
        let buffer = [ ];
        buffer.length = valueCount;

        let dst = buffer;
        let src = array;
        for (let width = 1; width < valueCount; width *= 2) {
            for (let srcIndex = 0; srcIndex < valueCount; srcIndex += 2 * width)
                merge(dst, src, srcIndex, valueCount, width, comparator);

            let tmp = src;
            src = dst;
            dst = tmp;
        }

        if (src != array) {
            for(let i = 0; i < valueCount; i++)
                array[i] = src[i];
        }
    }

    function bucketSort(array, dst, bucket, depth)
    {
        if (bucket.length < 32 || depth > 32) {
            mergeSort(bucket, bucket.length, stringComparator);
            for (let i = 0; i < bucket.length; ++i)
                array[dst++] = bucket[i].value;
            return dst;
        }

        let buckets = [ ];
        for (let i = 0; i < bucket.length; ++i) {
            let entry = bucket[i];
            let string = entry.string;
            if (string.length == depth) {
                array[dst++] = entry.value;
                continue;
            }

            let c = string.@charCodeAt(depth);
            if (!buckets[c])
                buckets[c] = [ ];
            buckets[c][buckets[c].length] = entry;
        }

        for (let i = 0; i < buckets.length; ++i) {
            if (!buckets[i])
                continue;
            dst = bucketSort(array, dst, buckets[i], depth + 1);
        }

        return dst;
    }

    function comparatorSort(array, length, comparator)
    {
        let valueCount = compact(array, length);
        mergeSort(array, valueCount, comparator);
    }

    function stringSort(array, length)
    {
        let valueCount = compact(array, length);

        let strings = @newArrayWithSize(valueCount);
        for (let i = 0; i < valueCount; ++i)
            strings[i] = { string: @toString(array[i]), value: array[i] };

        bucketSort(array, 0, strings, 0);
    }

    let sortFunction;
    if (typeof comparator == "function")
        sortFunction = comparatorSort;
    else if (comparator === @undefined)
        sortFunction = stringSort;
    else
        @throwTypeError("Array.prototype.sort requires the comparsion function be a function or undefined");

    let array = @toObject(this, "Array.prototype.sort requires that |this| not be null or undefined");

    let length = @toLength(array.length);

    // For compatibility with Firefox and Chrome, do nothing observable
    // to the target array if it has 0 or 1 sortable properties.
    if (length < 2)
        return array;

    sortFunction(array, length, comparator);
    return array;
}

@globalPrivate
function concatSlowPath()
{
    "use strict";

    var currentElement = @toObject(this, "Array.prototype.concat requires that |this| not be null or undefined");
    var argCount = arguments.length;

    var result = @arraySpeciesCreate(currentElement, 0);
    var resultIsArray = @isJSArray(result);

    var resultIndex = 0;
    var argIndex = 0;

    do {
        let spreadable = @isObject(currentElement) && currentElement.@isConcatSpreadableSymbol;
        if ((spreadable === @undefined && @isArray(currentElement)) || spreadable) {
            let length = @toLength(currentElement.length);
            if (length + resultIndex > @MAX_ARRAY_INDEX)
                @throwRangeError("Length exceeded the maximum array length");
            if (resultIsArray && @isJSArray(currentElement)) {
                @appendMemcpy(result, currentElement, resultIndex);
                resultIndex += length;
            } else {
                for (var i = 0; i < length; i++) {
                    if (i in currentElement)
                        @putByValDirect(result, resultIndex, currentElement[i]);
                    resultIndex++;
                }
            }
        } else {
            if (resultIndex >= @MAX_ARRAY_INDEX)
                @throwRangeError("Length exceeded the maximum array length");
            @putByValDirect(result, resultIndex++, currentElement);
        }
        currentElement = arguments[argIndex];
    } while (argIndex++ < argCount);

    result.length = resultIndex;
    return result;
}

function concat(first)
{
    "use strict";

    if (@argumentCount() === 1
        && @isJSArray(this)
        && this.@isConcatSpreadableSymbol === @undefined
        && (!@isObject(first) || (!@isProxyObject(first) && first.@isConcatSpreadableSymbol === @undefined))) {

        let result = @concatMemcpy(this, first);
        if (result !== null)
            return result;
    }

    return @tailCallForwardArguments(@concatSlowPath, this);
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

    var array = @toObject(this, "Array.prototype.copyWithin requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    var relativeTarget = @toInteger(target);
    var to = (relativeTarget < 0) ? maxWithPositives(length + relativeTarget, 0) : minWithMaybeNegativeZeroAndPositive(relativeTarget, length);

    var relativeStart = @toInteger(start);
    var from = (relativeStart < 0) ? maxWithPositives(length + relativeStart, 0) : minWithMaybeNegativeZeroAndPositive(relativeStart, length);

    var relativeEnd;
    var end = @argument(2);
    if (end === @undefined)
        relativeEnd = length;
    else
        relativeEnd = @toInteger(end);

    var finalValue = (relativeEnd < 0) ? maxWithPositives(length + relativeEnd, 0) : minWithMaybeNegativeZeroAndPositive(relativeEnd, length);

    var count = minWithMaybeNegativeZeroAndPositive(finalValue - from, length - to);

    var direction = 1;
    if (from < to && to < from + count) {
        direction = -1;
        from = from + count - 1;
        to = to + count - 1;
    }

    for (var i = 0; i < count; ++i, from += direction, to += direction) {
        if (from in array)
            array[to] = array[from];
        else
            delete array[to];
    }

    return array;
}

@globalPrivate
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

function flat()
{
    "use strict";

    var array = @toObject(this, "Array.prototype.flat requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    var depthNum = 1;
    var depth = @argument(0);
    if (depth !== @undefined)
        depthNum = @toInteger(depth);

    var result = @arraySpeciesCreate(array, 0);

    @flatIntoArray(result, array, length, 0, depthNum);
    return result;
}

@globalPrivate
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

    if (typeof callback !== "function")
        @throwTypeError("Array.prototype.flatMap callback must be a function");

    var thisArg = @argument(1);

    var result = @arraySpeciesCreate(array, 0);

    return @flatIntoArrayWithCallback(result, array, length, 0, callback, thisArg);
}
