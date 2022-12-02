/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

function of(/* items... */)
{
    "use strict";

    var length = arguments.length;
    var array = this !== @Array && @isConstructor(this) ? new this(length) : @newArrayWithSize(length);
    for (var k = 0; k < length; ++k)
        @putByValDirect(array, k, arguments[k]);
    array.length = length;
    return array;
}

function from(items /*, mapFn, thisArg */)
{
    "use strict";

    var mapFn = @argument(1);

    var thisArg;

    if (mapFn !== @undefined) {
        if (!@isCallable(mapFn))
            @throwTypeError("Array.from requires that the second argument, when provided, be a function");

        thisArg = @argument(2);
    }

    var arrayLike = @toObject(items, "Array.from requires an array-like object - not null or undefined");

    var iteratorMethod = items.@@iterator;
    if (!@isUndefinedOrNull(iteratorMethod)) {
        if (!@isCallable(iteratorMethod))
            @throwTypeError("Array.from requires that the property of the first argument, items[Symbol.iterator], when exists, be a function");

        var result = this !== @Array && @isConstructor(this) ? new this() : [];

        var k = 0;
        var iterator = iteratorMethod.@call(items);

        // Since for-of loop once more looks up the @@iterator property of a given iterable,
        // it could be observable if the user defines a getter for @@iterator.
        // To avoid this situation, we define a wrapper object that @@iterator just returns a given iterator.
        var wrapper = {
            @@iterator: function () { return iterator; }
        };

        for (var value of wrapper) {
            if (k >= @MAX_SAFE_INTEGER)
                @throwTypeError("Length exceeded the maximum array length");
            if (mapFn)
                @putByValDirect(result, k, thisArg === @undefined ? mapFn(value, k) : mapFn.@call(thisArg, value, k));
            else
                @putByValDirect(result, k, value);
            k += 1;
        }

        result.length = k;
        return result;
    }

    var arrayLikeLength = @toLength(arrayLike.length);

    var result = this !== @Array && @isConstructor(this) ? new this(arrayLikeLength) : @newArrayWithSize(arrayLikeLength);

    var k = 0;
    while (k < arrayLikeLength) {
        var value = arrayLike[k];
        if (mapFn)
            @putByValDirect(result, k, thisArg === @undefined ? mapFn(value, k) : mapFn.@call(thisArg, value, k));
        else
            @putByValDirect(result, k, value);
        k += 1;
    }

    result.length = arrayLikeLength;
    return result;
}

function isArray(array)
{
    "use strict";

    if (@isJSArray(array) || @isDerivedArray(array))
        return true;
    if (!@isProxyObject(array))
        return false;
    return @isArraySlow(array);
}

@linkTimeConstant
@visibility=PrivateRecursive
async function defaultAsyncFromAsyncIterator(result, iterator, mapFn, thisArg)
{
    "use strict";

    var k = 0;

    // Since for-of loop once more looks up the @@iterator property of a given iterable,
    // it could be observable if the user defines a getter for @@iterator.
    // To avoid this situation, we define a wrapper object that @@iterator just returns a given iterator.
    var wrapper = {
        @@asyncIterator: function () { return iterator; }
    };

    for await (var value of wrapper) {
        if (k >= @MAX_SAFE_INTEGER)
            @throwTypeError("Length exceeded the maximum array length");
        if (mapFn)
            @putByValDirect(result, k, await (thisArg === @undefined ? mapFn(value, k) : mapFn.@call(thisArg, value, k)));
        else
            @putByValDirect(result, k, value);
        k += 1;
    }

    result.length = k;
    return result;
}

@linkTimeConstant
@visibility=PrivateRecursive
async function defaultAsyncFromAsyncArrayLike(asyncItems, mapFn, thisArg)
{
    "use strict";

    var arrayLike = @toObject(asyncItems, "Array.fromAsync requires an array-like object - not null or undefined");

    var arrayLikeLength = @toLength(arrayLike.length);

    var result = this !== @Array && @isConstructor(this) ? new this(arrayLikeLength) : @newArrayWithSize(arrayLikeLength);

    var k = 0;
    while (k < arrayLikeLength) {
        var value = await arrayLike[k];
        if (mapFn)
            @putByValDirect(result, k, await (thisArg === @undefined ? mapFn(value, k) : mapFn.@call(thisArg, value, k)));
        else
            @putByValDirect(result, k, value);
        k += 1;
    }

    result.length = arrayLikeLength;
    return result;
}

function fromAsync(asyncItems  /*, mapFn, thisArg */)
{
    "use strict";

    try {
        var mapFn = @argument(1);

        var thisArg;

        if (mapFn !== @undefined) {
            if (!@isCallable(mapFn))
                @throwTypeError("Array.fromAsync requires that the second argument, when provided, be a function");

            thisArg = @argument(2);
        }

        var usingSyncIterator;
        var usingAsyncIterator = asyncItems.@@asyncIterator;
        if (!@isUndefinedOrNull(usingAsyncIterator)) {
            if (!@isCallable(usingAsyncIterator))
                @throwTypeError("Array.fromAsync requires that the property of the first argument, items[Symbol.asyncIterator], when exists, be a function");
        } else {
            usingSyncIterator = asyncItems.@@iterator;
            if (!@isUndefinedOrNull(usingSyncIterator)) {
                if (!@isCallable(usingSyncIterator))
                    @throwTypeError("Array.fromAsync requires that the property of the first argument, items[Symbol.iterator], when exists, be a function");
            }
        }

        var result = this !== @Array && @isConstructor(this) ? new this() : [];

        if (!@isUndefinedOrNull(usingAsyncIterator))
            return @defaultAsyncFromAsyncIterator(result, usingAsyncIterator.@call(asyncItems), mapFn, thisArg);

        if (!@isUndefinedOrNull(usingSyncIterator)) {
            var iterator = usingSyncIterator.@call(asyncItems);
            return @defaultAsyncFromAsyncIterator(result, @createAsyncFromSyncIterator(iterator, iterator.next), mapFn, thisArg);
        }

        return @defaultAsyncFromAsyncArrayLike.@call(this, asyncItems, mapFn, thisArg);
    } catch (reason) {
        var promise = @newPromise();
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, reason);
        return promise;
    }
}
