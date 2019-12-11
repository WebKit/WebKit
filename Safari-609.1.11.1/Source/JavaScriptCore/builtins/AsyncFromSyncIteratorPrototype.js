/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

function next(value)
{
    "use strict";

    var promise = @newPromise();

    if (!@isObject(this) || !@isObject(@getByIdDirectPrivate(this, "syncIterator"))) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, @makeTypeError('Iterator is not an object.'));
        return promise;
    }

    var syncIterator = @getByIdDirectPrivate(this, "syncIterator");

    try {
        var nextResult = @getByIdDirectPrivate(this, "nextMethod").@call(syncIterator, value);
        var nextDone = !!nextResult.done;
        var nextValue = nextResult.value;
        @resolveWithoutPromise(nextValue,
            function (result) { @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, { value: result, done: nextDone }); },
            function (error) { @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, error); });
    } catch (e) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, e);
    }

    return promise;
}

function return(value)
{
    "use strict";

    var promise = @newPromise();

    if (!@isObject(this) || !@isObject(@getByIdDirectPrivate(this, "syncIterator"))) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, @makeTypeError('Iterator is not an object.'));
        return promise;
    }

    var syncIterator = @getByIdDirectPrivate(this, "syncIterator");

    var returnMethod;

    try {
        returnMethod = syncIterator.return;
    } catch (e) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, e);
        return promise;
    }

    if (returnMethod === @undefined) {
        @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, { value, done: true });
        return promise;
    }
    
    try {
        var returnResult = returnMethod.@call(syncIterator, value);

        if (!@isObject(returnResult)) {
            @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, @makeTypeError('Iterator result interface is not an object.'));
            return promise;
        }

        var resultDone = !!returnResult.done;
        var resultValue = returnResult.value;
        @resolveWithoutPromise(resultValue,
            function (result) { @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, { value: result, done: resultDone }); },
            function (error) { @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, error); });
    } catch (e) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, e);
    }

    return promise;
}

function throw(exception)
{
    "use strict";

    var promise = @newPromise();

    if (!@isObject(this) || !@isObject(@getByIdDirectPrivate(this, "syncIterator"))) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, @makeTypeError('Iterator is not an object.'));
        return promise;
    }

    var syncIterator = @getByIdDirectPrivate(this, "syncIterator");

    var throwMethod;

    try {
        throwMethod = syncIterator.throw;
    } catch (e) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, e);
        return promise;
    }

    if (throwMethod === @undefined) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, exception);
        return promise;
    }
    
    try {
        var throwResult = throwMethod.@call(syncIterator, exception);
        
        if (!@isObject(throwResult)) {
            @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, @makeTypeError('Iterator result interface is not an object.'));
            return promise;
        }
        
        var throwDone = !!throwResult.done;
        var throwValue = throwResult.value;
        @resolveWithoutPromise(throwValue,
            function (result) { @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, { value: result, done: throwDone }); },
            function (error) { @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, error); });
    } catch (e) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, e);
    }
    
    return promise;
}

@globalPrivate
function createAsyncFromSyncIterator(syncIterator, nextMethod)
{
    "use strict";

    if (!@isObject(syncIterator))
        @throwTypeError('Only objects can be wrapped by async-from-sync wrapper');

    return new @AsyncFromSyncIterator(syncIterator, nextMethod);
}

@globalPrivate
@constructor
function AsyncFromSyncIterator(syncIterator, nextMethod)
{
    "use strict";

    @putByIdDirectPrivate(this, "syncIterator", syncIterator);
    @putByIdDirectPrivate(this, "nextMethod", nextMethod);
}
