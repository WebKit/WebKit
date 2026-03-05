/*
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

function try(callback /*, ...args */)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("|this| is not an object");

    var args = [];
    for (var i = 1; i < @argumentCount(); i++)
        @putByValDirect(args, i - 1, arguments[i]);

    var promiseCapability = @newPromiseCapability(this);
    try {
        var value = callback.@apply(@undefined, args);
        promiseCapability.resolve.@call(@undefined, value);
    } catch (error) {
        promiseCapability.reject.@call(@undefined, error);
    }

    return promiseCapability.promise;
}

@nakedConstructor
function Promise(executor)
{
    "use strict";

    if (!@isCallable(executor))
        @throwTypeError("Promise constructor takes a function argument");

    var promise = @createPromise(this, /* isInternalPromise */ false);
    var capturedPromise = promise;

    try {
        executor(
            (resolution) => {
                return @resolvePromiseWithFirstResolvingFunctionCallCheck(capturedPromise, resolution);
            },
            (reason) => {
                return @rejectPromiseWithFirstResolvingFunctionCallCheck(capturedPromise, reason);
            });
    } catch (error) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, error);
    }

    return promise;
}

@nakedConstructor
function InternalPromise(executor)
{
    "use strict";

    if (!@isCallable(executor))
        @throwTypeError("InternalPromise constructor takes a function argument");

    var promise = @createPromise(this, /* isInternalPromise */ true);
    var capturedPromise = promise;

    try {
        executor(
            (resolution) => {
                return @resolvePromiseWithFirstResolvingFunctionCallCheck(capturedPromise, resolution);
            },
            (reason) => {
                return @rejectPromiseWithFirstResolvingFunctionCallCheck(capturedPromise, reason);
            });
    } catch (error) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, error);
    }

    return promise;
}
