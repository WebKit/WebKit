/*
 * Copyright (C) 2025 Sosuke Suzuki <aosukeke@gmail.com>.
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

@linkTimeConstant
function promiseReturnUndefinedOnFulfilled(argument)
{
    "use strict";

    return @undefined;
}

// https://tc39.es/proposal-explicit-resource-management/#sec-%asynciteratorprototype%-@@asyncdispose
@overriddenName="[Symbol.asyncDispose]"
function asyncDispose()
{
    'use strict';

    var promise = @newPromise();
    var returnMethod;
    try {
        returnMethod = this.return;
    } catch (e) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, e);
        return promise;
    }

    if (returnMethod === @undefined)
        @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, @undefined);
    else {
        var result;
        try {
            result = returnMethod.@call(this);
        } catch (e) {
            @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, e);
            return promise;
        }
        var resultWrapper;
        try {
            if (@isPromise(result) && result.constructor == @Promise)
                resultWrapper = result;
            else {
                resultWrapper = @newPromise();
                @resolvePromiseWithFirstResolvingFunctionCallCheck(resultWrapper, result);
            }
        } catch (e) {
            @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, e);
            return promise;
        }
        @performPromiseThen(resultWrapper, @promiseReturnUndefinedOnFulfilled, @undefined, promise, @undefined);
    }

    return promise;
}
