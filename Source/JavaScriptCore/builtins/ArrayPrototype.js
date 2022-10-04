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

function group(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.group requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.group callback must be a function");

    var thisArg = @argument(1);

    var groups = @Object.@create(null);
    for (var i = 0; i < length; ++i) {
        var value = array[i];
        var key = @toPropertyKey(callback.@call(thisArg, value, i, array));
        var group = groups[key];
        if (!group) {
            group = [];
            @putByValDirect(groups, key, group);
        }
        @putByValDirect(group, group.length, value);
    }
    return groups;
}

function groupToMap(callback /*, thisArg */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.groupToMap requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    if (!@isCallable(callback))
        @throwTypeError("Array.prototype.groupToMap callback must be a function");

    var thisArg = @argument(1);

    var groups = new @Map;
    for (var i = 0; i < length; ++i) {
        var value = array[i];
        var key = callback.@call(thisArg, value, i, array);
        var group = groups.@get(key);
        if (!group) {
            group = [];
            groups.@set(key, group);
        }
        @putByValDirect(group, group.length, value);
    }
    return groups;
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

function fill(value /* [, start [, end]] */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.fill requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    var relativeStart = @toIntegerOrInfinity(@argument(1));
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
        relativeEnd = @toIntegerOrInfinity(end);
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
        fromIndex = @toIntegerOrInfinity(from);

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

@linkTimeConstant
function sortStringComparator(a, b)
{
    "use strict";

    var aString = a.string;
    var bString = b.string;

    if (aString === bString)
        return 0;

    return aString > bString ? 1 : -1;
}

@linkTimeConstant
function sortCompact(receiver, receiverLength, compacted, isStringSort)
{
    "use strict";

    var undefinedCount = 0;
    var compactedIndex = 0;

    for (var i = 0; i < receiverLength; ++i) {
        if (i in receiver) {
            var value = receiver[i];
            if (value === @undefined)
                ++undefinedCount;
            else {
                @putByValDirect(compacted, compactedIndex,
                    isStringSort ? {string: @toString(value), value} : value);
                ++compactedIndex;
            }
        }
    }

    return undefinedCount;
}

@linkTimeConstant
function sortCommit(receiver, receiverLength, sorted, undefinedCount)
{
    "use strict";

    // Move undefineds and holes to the end of an array. Result is [values..., undefineds..., holes...].

    @assert(@isJSArray(sorted));
    var sortedLength = sorted.length;
    @assert(sortedLength + undefinedCount <= receiverLength);

    var i = 0;
    if (@isJSArray(receiver) && sortedLength >= 64 && typeof sorted[0] !== "number") { // heuristic
        @appendMemcpy(receiver, sorted, 0);
        i = sortedLength;
    } else {
        for (; i < sortedLength; ++i)
            receiver[i] = sorted[i];
    }

    for (; i < sortedLength + undefinedCount; ++i)
        receiver[i] = @undefined;

    for (; i < receiverLength; ++i)
        delete receiver[i];
}

@linkTimeConstant
function sortMerge(dst, src, srcIndex, srcEnd, width, comparator)
{
    "use strict";

    var left = srcIndex;
    var leftEnd = @min(left + width, srcEnd);
    var right = leftEnd;
    var rightEnd = @min(right + width, srcEnd);

    for (var dstIndex = left; dstIndex < rightEnd; ++dstIndex) {
        if (right < rightEnd) {
            if (left >= leftEnd) {
                @putByValDirect(dst, dstIndex, src[right]);
                ++right;
                continue;
            }

            // See https://bugs.webkit.org/show_bug.cgi?id=47825 on boolean special-casing
            var comparisonResult = comparator(src[right], src[left]);
            if (comparisonResult === false || comparisonResult < 0) {
                @putByValDirect(dst, dstIndex, src[right]);
                ++right;
                continue;
            }

        }

        @putByValDirect(dst, dstIndex, src[left]);
        ++left;
    }
}

@linkTimeConstant
function sortMergeSort(array, comparator)
{
    "use strict";

    var valueCount = array.length;
    var buffer = @newArrayWithSize(valueCount);

    var dst = buffer;
    var src = array;
    for (var width = 1; width < valueCount; width *= 2) {
        for (var srcIndex = 0; srcIndex < valueCount; srcIndex += 2 * width)
            @sortMerge(dst, src, srcIndex, valueCount, width, comparator);

        var tmp = src;
        src = dst;
        dst = tmp;
    }

    return src;
}

@linkTimeConstant
function sortBucketSort(array, dst, bucket, depth)
{
    "use strict";

    if (bucket.length < 32 || depth > 32) {
        var sorted = @sortMergeSort(bucket, @sortStringComparator);
        for (var i = 0; i < sorted.length; ++i) {
            @putByValDirect(array, dst, sorted[i].value);
            ++dst;
        }
        return dst;
    }

    var buckets = [ ];
    @setPrototypeDirect.@call(buckets, null);
    for (var i = 0; i < bucket.length; ++i) {
        var entry = bucket[i];
        var string = entry.string;
        if (string.length == depth) {
            @putByValDirect(array, dst, entry.value);
            ++dst;
            continue;
        }

        var c = string.@charCodeAt(depth);
        var cBucket = buckets[c];
        if (cBucket)
            @arrayPush(cBucket, entry);
        else
            @putByValDirect(buckets, c, [ entry ]);
    }

    for (var i = 0; i < buckets.length; ++i) {
        if (!buckets[i])
            continue;
        dst = @sortBucketSort(array, dst, buckets[i], depth + 1);
    }

    return dst;
}

function sort(comparator)
{
    "use strict";

    var isStringSort = false;
    if (comparator === @undefined)
        isStringSort = true;
    else if (!@isCallable(comparator))
        @throwTypeError("Array.prototype.sort requires the comparator argument to be a function or undefined");

    var receiver = @toObject(this, "Array.prototype.sort requires that |this| not be null or undefined");
    var receiverLength = @toLength(receiver.length);

    // For compatibility with Firefox and Chrome, do nothing observable
    // to the target array if it has 0 or 1 sortable properties.
    if (receiverLength < 2)
        return receiver;

    var compacted = [ ];
    var sorted = null;
    var undefinedCount = @sortCompact(receiver, receiverLength, compacted, isStringSort);

    if (isStringSort) {
        sorted = @newArrayWithSize(compacted.length);
        @sortBucketSort(sorted, 0, compacted, 0);
    } else
        sorted = @sortMergeSort(compacted, comparator);

    @sortCommit(receiver, receiverLength, sorted, undefinedCount);
    return receiver;
}

@linkTimeConstant
function concatSlowPath()
{
    "use strict";

    var currentElement = @toObject(this, "Array.prototype.concat requires that |this| not be null or undefined");
    var argCount = arguments.length;

    var result = @newArrayWithSpecies(0, currentElement);
    var resultIsArray = @isJSArray(result);

    var resultIndex = 0;
    var argIndex = 0;

    do {
        var spreadable = @isObject(currentElement) && currentElement.@@isConcatSpreadable;
        if ((spreadable === @undefined && @isArray(currentElement)) || spreadable) {
            var length = @toLength(currentElement.length);
            if (length + resultIndex > @MAX_SAFE_INTEGER)
                @throwTypeError("Length exceeded the maximum array length");
            if (resultIsArray && @isJSArray(currentElement) && length + resultIndex <= @MAX_ARRAY_INDEX) {
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
            if (resultIndex >= @MAX_SAFE_INTEGER)
                @throwTypeError("Length exceeded the maximum array length");
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
        && @tryGetByIdWithWellKnownSymbol(this, "isConcatSpreadable") === @undefined
        && (!@isObject(first) || @tryGetByIdWithWellKnownSymbol(first, "isConcatSpreadable") === @undefined)) {

        var result = @concatMemcpy(this, first);
        if (result !== null)
            return result;
    }

    return @tailCallForwardArguments(@concatSlowPath, this);
}

@linkTimeConstant
function maxWithPositives(a, b)
{
    "use strict";

    return (a < b) ? b : a;
}

@linkTimeConstant
function minWithMaybeNegativeZeroAndPositive(maybeNegativeZero, positive)
{
    "use strict";

    return (maybeNegativeZero < positive) ? maybeNegativeZero : positive;
}

function copyWithin(target, start /*, end */)
{
    "use strict";

    var array = @toObject(this, "Array.prototype.copyWithin requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    var relativeTarget = @toIntegerOrInfinity(target);
    var to = (relativeTarget < 0) ? @maxWithPositives(length + relativeTarget, 0) : @minWithMaybeNegativeZeroAndPositive(relativeTarget, length);

    var relativeStart = @toIntegerOrInfinity(start);
    var from = (relativeStart < 0) ? @maxWithPositives(length + relativeStart, 0) : @minWithMaybeNegativeZeroAndPositive(relativeStart, length);

    var relativeEnd;
    var end = @argument(2);
    if (end === @undefined)
        relativeEnd = length;
    else
        relativeEnd = @toIntegerOrInfinity(end);

    var finalValue = (relativeEnd < 0) ? @maxWithPositives(length + relativeEnd, 0) : @minWithMaybeNegativeZeroAndPositive(relativeEnd, length);

    var count = @minWithMaybeNegativeZeroAndPositive(finalValue - from, length - to);

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

function flat()
{
    "use strict";

    var array = @toObject(this, "Array.prototype.flat requires that |this| not be null or undefined");
    var length = @toLength(array.length);

    var depthNum = 1;
    var depth = @argument(0);
    if (depth !== @undefined)
        depthNum = @toIntegerOrInfinity(depth);

    var result = @newArrayWithSpecies(0, array);

    @flatIntoArray(result, array, length, 0, depthNum);
    return result;
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

function toReversed()
{
    "use strict";

    // Step 1.
    var array = @toObject(this, "Array.prototype.toReversed requires that |this| not be null or undefined");

    // Step 2.
    var length = @toLength(array.length);

    // Step 3.
    var result = @newArrayWithSize(length);

    // Step 4-5.
    for (var k = 0; k < length; k++) {
        var fromValue = array[length - k - 1];
        @putByValDirect(result, k, fromValue);
    }

    return result;
}

function toSorted(comparator)
{
    "use strict";

    // Step 1.
    if (comparator !== @undefined && !@isCallable(comparator))
        @throwTypeError("Array.prototype.toSorted requires the comparator argument to be a function or undefined");

    // Step 2.
    var array = @toObject(this, "Array.prototype.toSorted requires that |this| not be null or undefined");

    // Step 3.
    var length = @toLength(array.length);

    // Step 4.
    var result = @newArrayWithSize(length);

    // Step 8.
    for (var k = 0; k < length; k++)
        @putByValDirect(result, k, array[k]);

    // Step 6.
    @arraySort.@call(result, comparator);

    return result;
}

function toSpliced(start, deleteCount /*, ...items */)
{
    "use strict"

    // Step 1.
    var array = @toObject(this, "Array.prototype.toSpliced requires that |this| not be null or undefined");

    // Step 2.
    var length = @toLength(array.length);

    // Step 3.
    var relativeStart = @toIntegerOrInfinity(start);

    var actualStart;
    // Step 4-6.
    if (relativeStart === -@Infinity)
        actualStart = 0;
    else if (relativeStart < 0)
        actualStart = length + relativeStart > 0 ? length + relativeStart : 0;
    else
        actualStart = @min(relativeStart, length);

    // Step 7.
    var insertCount = 0;
    var actualDeleteCount;

    // Step 8-10.
    if (arguments.length === 0)
        actualDeleteCount = 0;
    else if (arguments.length === 1)
        actualDeleteCount = length - actualStart;
    else {
        insertCount = arguments.length - 2;
        var tempDeleteCount = @toIntegerOrInfinity(deleteCount);
        tempDeleteCount = tempDeleteCount > 0 ? tempDeleteCount : 0;
        actualDeleteCount = @min(tempDeleteCount, length - actualStart);
    }

    // Step 11.
    var newLen = length + insertCount - actualDeleteCount;

    // Step 12.
    if (newLen >= @MAX_SAFE_INTEGER)
        @throwTypeError("Array length exceeds 2**53 - 1");

    // Step 13.
    var result = @newArrayWithSize(newLen);

    // Step 14.
    var k = 0;

    // Step 16.
    for (; k < actualStart; k++)
        @putByValDirect(result, k, array[k]);

    // Step 17.
    for (var i = 0; i < insertCount; i++, k++)
        @putByValDirect(result, k, arguments[i + 2]);

    // Step 18.
    for (; k < newLen; k++) {
        var from = k + actualDeleteCount - insertCount;
        @putByValDirect(result, k, array[from]);
    }

    return result;

}

function with(index, value)
{
    "use strict";

    // Step 1.
    var array = @toObject(this, "Array.prototype.with requires that |this| not be null or undefined");

    // Step 2.
    var length = @toLength(array.length);

    // Step 3.
    var relativeIndex = @toIntegerOrInfinity(index);

    // Step 4-5.
    var actualIndex;
    if (relativeIndex >= 0)
        actualIndex = relativeIndex;
    else
        actualIndex = length + relativeIndex;

    // Step 6.
    if (actualIndex >= length || actualIndex < 0)
        @throwRangeError("Array index out of Range");

    // Step 7.
    var result = @newArrayWithSize(length);

    // Step 8-9
    for (var k = 0; k < length; k++) {
        if (k === actualIndex)
            @putByValDirect(result, k, value);
        else
            @putByValDirect(result, k, array[k]);
    }

    return result;
}
