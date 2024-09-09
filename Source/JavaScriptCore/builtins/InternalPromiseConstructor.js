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

function internalAll(array)
{
    // This function is intended to be used in the JSC internals.
    // The implementation should take care not to perform the user
    // observable / trappable operations.
    //
    // 1. Don't use for-of and iterables. This function only accepts
    //    the dense array of the promises.
    // 2. Don't look up this.constructor / @@species. Always construct
    //    the plain Promise object.

    "use strict";

    var constructor = @InternalPromise;
    var promise = @createPromise(constructor, /* isInternalPromise */ true);
    var reject = (reason) => {
        return @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, reason);
    };

    var values = [];
    var index = 0;
    var remainingElementsCount = 0;

    try {
        if (array.length === 0)
            @fulfillPromiseWithFirstResolvingFunctionCallCheck(promise, values);
        else {
            for (var index = 0, length = array.length; index < length; ++index) {
                var value = array[index];
                @putByValDirect(values, index, @undefined);
                let currentIndex = index;
                ++remainingElementsCount;
                @resolveWithoutPromise(value, (argument) => {
                    if (currentIndex < 0)
                        return @undefined;

                    @putByValDirect(values, currentIndex, argument);
                    currentIndex = -1;

                    --remainingElementsCount;
                    if (remainingElementsCount === 0)
                        return @fulfillPromiseWithFirstResolvingFunctionCallCheck(promise, values);

                    return @undefined;
                }, reject, @undefined);
            }
        }
    } catch (error) {
        reject(error);
    }

    return promise;
}
