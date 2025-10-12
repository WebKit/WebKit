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

@linkTimeConstant
function promiseAllSlow(iterable)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("|this| is not an object");

    var promiseCapability = @newPromiseCapability(this);
    var resolve = promiseCapability.resolve;
    var reject = promiseCapability.reject;
    var promise = promiseCapability.promise;

    var values = [];
    var index = 0;
    var remainingElementsCount = 1;

    try {
        var promiseResolve = this.resolve;
        if (!@isCallable(promiseResolve))
            @throwTypeError("Promise resolve is not a function");

        for (var value of iterable) {
            @putByValDirect(values, index, @undefined);
            var nextPromise = promiseResolve.@call(this, value);
            let currentIndex = index++;
            ++remainingElementsCount;
            nextPromise.then((argument) => {
                if (currentIndex < 0)
                    return @undefined;

                @putByValDirect(values, currentIndex, argument);
                currentIndex = -1;

                --remainingElementsCount;
                if (remainingElementsCount === 0)
                    return resolve.@call(@undefined, values);

                return @undefined;
            }, reject);
        }

        --remainingElementsCount;
        if (remainingElementsCount === 0)
            resolve.@call(@undefined, values);
    } catch (error) {
        reject.@call(@undefined, error);
    }

    return promise;
}

@linkTimeConstant
function promiseNewOnRejected(promise)
{
    "use strict";

    return function @reject(reason) {
        return @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, reason);
    };
}

@linkTimeConstant
function promiseAllNewResolveElement(context)
{
    "use strict";

    @assert(@isPromiseAllContext(context));

    var alreadyCalled = false;
    return (argument) => {
        if (alreadyCalled)
            return @undefined;
        alreadyCalled = true;

        var globalContext = @getPromiseAllContextInternalField(context, @promiseAllContextFieldGlobalContext);
        var index = @getPromiseAllContextInternalField(context, @promiseAllContextFieldIndex);
        var values = @getPromiseAllGlobalContextInternalField(globalContext, @promiseAllGlobalContextFieldValues);
        @putByValDirect(values, index, argument);

        var count = @getPromiseAllGlobalContextInternalField(globalContext, @promiseAllGlobalContextFieldRemainingElementsCount) - 1;
        @putPromiseAllGlobalContextInternalField(globalContext, @promiseAllGlobalContextFieldRemainingElementsCount, count);
        if (!count) {
            var promise = @getPromiseAllGlobalContextInternalField(globalContext, @promiseAllGlobalContextFieldPromise);
            return @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, values);
        }
    };
}

function all(iterable)
{
    "use strict";

    if (this !== @Promise)
        return @tailCallForwardArguments(@promiseAllSlow, this);

    var promise = @newPromise();
    var values = [];
    var globalContext = @promiseAllGlobalContextCreate(promise, values, 1);
    var index = 0;
    var onRejected;

    try {
        var promiseResolve = this.resolve;
        if (!@isCallable(promiseResolve))
            @throwTypeError("Promise resolve is not a function");

        for (var value of iterable) {
            @putByValDirect(values, index, @undefined);
            var nextPromise = promiseResolve.@call(this, value);
            @putPromiseAllGlobalContextInternalField(globalContext, @promiseAllGlobalContextFieldRemainingElementsCount, @getPromiseAllGlobalContextInternalField(globalContext, @promiseAllGlobalContextFieldRemainingElementsCount) + 1);
            var then = nextPromise.then;
            var context = @promiseAllContextCreate(globalContext, index);
            if (@isPromise(nextPromise) && then === @defaultPromiseThen) {
                var constructor = @speciesConstructor(nextPromise, @Promise);
                var promiseOrCapability;
                if (constructor !== @Promise)
                    promiseOrCapability = @newPromiseCapabilitySlow(constructor);
                @performPromiseThen(nextPromise, @promiseAllOnFulfilled, @promiseOnRejectedWithContext, promiseOrCapability, context);
            } else {
                if (!onRejected)
                    onRejected = @promiseNewOnRejected(promise);
                then.@call(nextPromise, @promiseAllNewResolveElement(context), onRejected);
            }
            ++index;
        }

        var count = @getPromiseAllGlobalContextInternalField(globalContext, @promiseAllGlobalContextFieldRemainingElementsCount) - 1;
        @putPromiseAllGlobalContextInternalField(globalContext, @promiseAllGlobalContextFieldRemainingElementsCount, count);
        if (!count)
            @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, values);
    } catch (error) {
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, error);
    }

    return promise;
}

function allSettled(iterable)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("|this| is not an object");

    var promiseCapability = @newPromiseCapability(this);
    var resolve = promiseCapability.resolve;
    var reject = promiseCapability.reject;
    var promise = promiseCapability.promise;

    var values = [];
    var remainingElementsCount = 1;
    var index = 0;

    try {
        var promiseResolve = this.resolve;
        if (!@isCallable(promiseResolve))
            @throwTypeError("Promise resolve is not a function");

        for (var value of iterable) {
            @putByValDirect(values, index, @undefined);
            var nextPromise = promiseResolve.@call(this, value);
            var then = nextPromise.then;
            ++remainingElementsCount;
            let currentIndex = index++;

            // Use comma expr for avoiding unnecessary Function.prototype.name
            var onResolved = (0, (value) => {
                if (currentIndex < 0)
                    return @undefined;

                @putByValDirect(values, currentIndex, {
                    status: "fulfilled",
                    value
                });
                currentIndex = -1;

                --remainingElementsCount;
                if (remainingElementsCount === 0)
                    return resolve.@call(@undefined, values);
                return @undefined;
            });
            var onRejected = (0, (reason) => {
                if (currentIndex < 0)
                    return @undefined;

                @putByValDirect(values, currentIndex, {
                    status: "rejected",
                    reason
                });
                currentIndex = -1;

                --remainingElementsCount;
                if (remainingElementsCount === 0)
                    return resolve.@call(@undefined, values);
                return @undefined;
            });

            if (@isPromise(nextPromise) && then === @defaultPromiseThen)
                @performPromiseThen(nextPromise, onResolved, onRejected, @undefined, /* context */ promise);
            else
                then.@call(nextPromise, onResolved, onRejected);
        }

        --remainingElementsCount;
        if (remainingElementsCount === 0)
            resolve.@call(@undefined, values);
    } catch (error) {
        reject.@call(@undefined, error);
    }

    return promise;
}

function any(iterable)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("|this| is not an object");

    var promiseCapability = @newPromiseCapability(this);
    var resolve = promiseCapability.resolve;
    var reject = promiseCapability.reject;
    var promise = promiseCapability.promise;

    var errors = [];
    var remainingElementsCount = 1;
    var index = 0;

    try {
        var promiseResolve = this.resolve;
        if (!@isCallable(promiseResolve))
            @throwTypeError("Promise resolve is not a function");

        for (var value of iterable) {
            @putByValDirect(errors, index, @undefined);
            var nextPromise = promiseResolve.@call(this, value);
            var then = nextPromise.then;
            let currentIndex = index++;
            ++remainingElementsCount;

            // Use comma expr for avoiding unnecessary Function.prototype.name
            var onRejected = (0, (reason) => {
              if (currentIndex < 0)
                return @undefined;

              @putByValDirect(errors, currentIndex, reason);
              currentIndex = -1;

              if (!--remainingElementsCount)
                reject.@call(@undefined, new @AggregateError(errors));

              return @undefined;
            });
            if (@isPromise(nextPromise) && then === @defaultPromiseThen)
                @performPromiseThen(nextPromise, resolve, onRejected, @undefined, /* context */ promise);
            else
                then.@call(nextPromise, resolve, onRejected);
        }

        --remainingElementsCount;
        if (remainingElementsCount === 0)
            throw new @AggregateError(errors);
    } catch (error) {
        reject.@call(@undefined, error);
    }

    return promise;
}

function race(iterable)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("|this| is not an object");

    var promiseCapability = @newPromiseCapability(this);
    var resolve = promiseCapability.resolve;
    var reject = promiseCapability.reject;
    var promise = promiseCapability.promise;

    try {
        var promiseResolve = this.resolve;
        if (!@isCallable(promiseResolve))
            @throwTypeError("Promise resolve is not a function");

        for (var value of iterable) {
            var nextPromise = promiseResolve.@call(this, value);
            var then = nextPromise.then;
            if (@isPromise(nextPromise) && then === @defaultPromiseThen)
                @performPromiseThen(nextPromise, resolve, reject, @undefined, /* context */ promise);
            else
                nextPromise.then(resolve, reject);
        }
    } catch (error) {
        reject.@call(@undefined, error);
    }

    return promise;
}

function reject(reason)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("|this| is not an object");

    if (this === @Promise) {
        var promise = @newPromise();
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, reason);
        return promise;
    }

    return @promiseRejectSlow(this, reason);
}

function resolve(value)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("|this| is not an object");

    return @promiseResolve(this, value);
}

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
