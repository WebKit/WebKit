/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

// @internal

@linkTimeConstant
function newPromiseCapabilitySlow(constructor)
{
    "use strict";

    var promiseCapability = {
        resolve: @undefined,
        reject: @undefined,
        promise: @undefined,
    };

    var promise = new constructor((resolve, reject) => {
        if (promiseCapability.resolve !== @undefined)
            @throwTypeError("resolve function is already set");
        if (promiseCapability.reject !== @undefined)
            @throwTypeError("reject function is already set");

        promiseCapability.resolve = resolve;
        promiseCapability.reject = reject;
    });

    if (!@isCallable(promiseCapability.resolve))
        @throwTypeError("executor did not take a resolve function");

    if (!@isCallable(promiseCapability.reject))
        @throwTypeError("executor did not take a reject function");

    promiseCapability.promise = promise;

    return promiseCapability;
}

@linkTimeConstant
function newPromiseCapability(constructor)
{
    "use strict";

    if (constructor === @Promise) {
        var promise = @newPromise();
        var capturedPromise = promise;
        return {
            resolve: (0, (value) => {
                return @resolvePromiseWithFirstResolvingFunctionCallCheck(capturedPromise, value);
            }),
            reject: (0, (value) => {
                return @rejectPromiseWithFirstResolvingFunctionCallCheck(capturedPromise, value);
            }),
            promise
        };
    }

    return @newPromiseCapabilitySlow(constructor);
}
