/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
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

    const promiseCapability = @newPromiseCapability(@Promise);

    if (!@isObject(this) || !@isObject(this.@syncIterator)) {
        promiseCapability.@reject.@call(@undefined, new @TypeError('Iterator is not an object.'));
        return promiseCapability.@promise;
    }

    const syncIterator = this.@syncIterator;

    try {
        const { done: nextDone, value: nextValue } = this.@nextMethod.@call(syncIterator, value);
        const valueWrapperCapability = @newPromiseCapability(@Promise);
        valueWrapperCapability.@resolve.@call(@undefined, nextValue);
        valueWrapperCapability.@promise.@then(
            function (result) { promiseCapability.@resolve.@call(@undefined, { done: !!nextDone, value: result }); },
            function (error) { promiseCapability.@reject.@call(@undefined, error); });
     } catch(e) {
         promiseCapability.@reject.@call(@undefined, e);
     }

    return promiseCapability.@promise;
}

function return(value)
{
    "use strict";

    const promiseCapability = @newPromiseCapability(@Promise);

    if (!@isObject(this) || !@isObject(this.@syncIterator)) {
        promiseCapability.@reject.@call(@undefined, new @TypeError('Iterator is not an object.'));
        return promiseCapability.@promise;
    }

    const syncIterator = this.@syncIterator;

    let returnMethod;

    try {
        returnMethod = syncIterator.return;
    } catch (e) {
        promiseCapability.@reject.@call(@undefined, e);
        return promiseCapability.@promise;
    }

    if (returnMethod === @undefined) {
        promiseCapability.@resolve.@call(@undefined, { value, done: true });
        return promiseCapability.@promise;
    }
    
    try {
        const returnResult = returnMethod.@call(syncIterator, value);

        if (!@isObject(returnResult)) {
            promiseCapability.@reject.@call(@undefined, new @TypeError('Iterator result interface is not an object.'));
            return promiseCapability.@promise;
        }

        const { done: resultDone, value: resultValue } = returnResult;
        const valueWrapperCapability = @newPromiseCapability(@Promise);

        valueWrapperCapability.@resolve.@call(@undefined, resultValue);
        valueWrapperCapability.@promise.@then(
            function (result) { promiseCapability.@resolve.@call(@undefined, { done: resultDone, value: result }); },
            function (error) { promiseCapability.@reject.@call(@undefined, error); });
    } catch (e) {
        promiseCapability.@reject.@call(@undefined, e);
    }

    return promiseCapability.@promise;
}

function throw(exception)
{
    "use strict";

    const promiseCapability = @newPromiseCapability(@Promise);

    if (!@isObject(this) || !@isObject(this.@syncIterator)) {
        promiseCapability.@reject.@call(@undefined, new @TypeError('Iterator is not an object.'));
        return promiseCapability.@promise;
    }

    const syncIterator = this.@syncIterator;

    let throwMethod;

    try {
        throwMethod = syncIterator.throw;
    } catch (e) {
        promiseCapability.@reject.@call(@undefined, e);
        return promiseCapability.@promise;
    }

    if (throwMethod === @undefined) {
        promiseCapability.@reject.@call(@undefined, exception);
        return promiseCapability.@promise;
    }
    
    try {
        const throwResult = throwMethod.@call(syncIterator, exception);
        
        if (!@isObject(throwResult)) {
            promiseCapability.@reject.@call(@undefined, new @TypeError('Iterator result interface is not an object.'));
            return promiseCapability.@promise;
        }
        
        const { done: throwDone, value: throwValue } = throwResult;
        const valueWrapperCapability = @newPromiseCapability(@Promise);
        
        valueWrapperCapability.@resolve.@call(@undefined, throwValue);
        valueWrapperCapability.@promise.@then(
            function (result) { promiseCapability.@resolve.@call(@undefined, { done: throwDone, value: result }); },
            function (error) { promiseCapability.@reject.@call(@undefined, error); });
    } catch (e) {
        promiseCapability.@reject.@call(@undefined, e);
    }
    
    return promiseCapability.@promise;
}

@globalPrivate
function createAsyncFromSyncIterator(syncIterator, nextMethod)
{
    if (!@isObject(syncIterator))
        @throwTypeError('Only objects can be wrapped by async-from-sync wrapper');

    return new @AsyncFromSyncIteratorConstructor(syncIterator, nextMethod);
}

@globalPrivate
@constructor
function AsyncFromSyncIteratorConstructor(syncIterator, nextMethod)
{
    this.@syncIterator = syncIterator;
    this.@nextMethod = nextMethod;
}
