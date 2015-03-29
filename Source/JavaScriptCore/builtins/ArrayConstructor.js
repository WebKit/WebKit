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

function from(arrayLike /*, mapFn, thisArg */) {
    "use strict";

    if (arrayLike == null)
        throw new @TypeError("Array.from requires an array-like object - not null or undefined");

    var thisObj = this;

    var items = @Object(arrayLike);
    var mapFn = arguments.length > 1 ? arguments[1] : undefined;

    var thisArg;

    if (mapFn !== undefined) {
        if (typeof mapFn !== "function")
            throw new @TypeError("Array.from requires that the second argument, when provided, be a function");

        if (arguments.length > 2)
            thisArg = arguments[2];
    }

    var maxSafeInteger = 9007199254740991;
    var numberValue = @Number(items.length);
    var lengthValue;
    if (numberValue != numberValue) {  // isNaN(numberValue)
        lengthValue = 0;
    } else if (numberValue === 0 || !@isFinite(numberValue)) {
        lengthValue = numberValue;
    } else {
        lengthValue = (numberValue > 0 ? 1 : -1) * @floor(@abs(numberValue));
    }
    // originally Math.min(Math.max(length, 0), maxSafeInteger));
    var itemsLength = lengthValue > 0 ? (lengthValue < maxSafeInteger ? lengthValue : maxSafeInteger) : 0;

    var result = (typeof thisObj === "function") ? @Object(new thisObj(itemsLength)) : new @Array(itemsLength);

    var k = 0;
    while (k < itemsLength) {
        var value = items[k];
        if (mapFn)
            result[k] = typeof thisArg === "undefined" ? mapFn(value, k) : mapFn.@call(thisArg, value, k);
        else
            result[k] = value;
        k += 1;
    }

    result.length = itemsLength;
    return result;
}
