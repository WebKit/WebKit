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

// https://tc39.es/proposal-explicit-resource-management/#sec-getdisposemethod
@linkTimeConstant
function getAsyncDisposableMethod(value)
{
    'use strict';

    var method = value.@@asyncDispose;

    if (!@isCallable(method))
        @throwTypeError("@@asyncDispose must be callable");

    if (@isUndefinedOrNull(method))
        return @undefined;

    return () => {
        var promise = @newPromise();
        var result;
        try {
            result = method.@call(value);
        } catch (e) {
            @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, e);
            return promise;
        }
        @resolvePromiseWithFirstResolvingFunctionCallCheck(promise, @undefined);
        return promise;
    };
}

// https://tc39.es/proposal-explicit-resource-management/#sec-createdisposableresource
@linkTimeConstant
function createDisposableResource(value, isAsync /* , method */)
{
    'use strict';

    var method;
    if (@argumentCount() < 3) {
        if (@isUndefinedOrNull(value))
            value = @undefined;
        else {
            if (!@isObject(value))
                @throwTypeError("Disposable value must be an object");
            if (isAsync) {
                method = @getAsyncDisposableMethod(value);
                if (method === @undefined)
                    @throwTypeError("@@asyncDispose must not be an undefined");
            } else {
                method = value.@@dispose;
                if (!@isCallable(method))
                    @throwTypeError("@@dispose must be callable");
                if (@isUndefinedOrNull(method))
                    @throwTypeError("@@dispose must not be an undefined");
            }
        }
    } else {
        method = @argument(2);
        if (!@isCallable(method))
            @throwTypeError("Callback that is called on dispose must be callable");
    }

    return { value, method };
}

// https://tc39.es/proposal-explicit-resource-management/#sec-adddisposableresource
@linkTimeConstant
function addDisposableResource(disposeCapability, value, isAsync /* , method */)
{
    'use strict';

    @assert(@isArray(disposeCapability));

    var resource;
    if (@argumentCount() < 4) {
        if (@isUndefinedOrNull(value))
            return;
        resource = @createDisposableResource(value, isAsync);
    } else {
        @assert(value === @undefined);
        resource = @createDisposableResource(@undefined, isAsync, @argument(3));
    }

    @arrayPush(disposeCapability, resource);
}

// https://tc39.es/proposal-explicit-resource-management/#sec-disposablestack.prototype.adopt
function adopt(value, onDispose)
{
    'use strict';

    if (!@isDisposableStack(this))
        @throwTypeError("DisposableStack.prototype.adopt requires that |this| be a DisposableStack object");

    if (@getDisposableStackInternalField(this, @disposableStackFieldState) === @DisposableStackStateDisposed)
        throw new @ReferenceError("DisposableStack.prototype.adopt requires that |this| be a pending DisposableStack object");

    if (!@isCallable(onDispose))
        @throwTypeError("DisposableStack.prototype.adopt requires that onDispose argument be a callable");

    var closure = () => { onDispose.@call(@undefined, value); }
    @addDisposableResource(@getDisposableStackInternalField(this, @disposableStackFieldCapability), @undefined, /* isAsync */ false, closure);

    return value;
}

// https://tc39.es/proposal-explicit-resource-management/#sec-disposablestack.prototype.defer
@overriddenName="defer"
function deferMethod(onDispose)
{
    'use strict';

    if (!@isDisposableStack(this))
        @throwTypeError("DisposableStack.prototype.defer requires that |this| be a DisposableStack object");

    if (@getDisposableStackInternalField(this, @disposableStackFieldState) === @DisposableStackStateDisposed)
        throw new @ReferenceError("DisposableStack.prototype.defer requires that |this| be a pending DisposableStack object");

    if (!@isCallable(onDispose))
        @throwTypeError("DisposableStack.prototype.defer requires that onDispose argument be a callable");

    @addDisposableResource(@getDisposableStackInternalField(this, @disposableStackFieldCapability), @undefined, /* isAsync */ false, onDispose);

    return @undefined;
}

// https://tc39.es/proposal-explicit-resource-management/#sec-disposablestack.prototype.dispose
function dispose()
{
    'use strict';

    if (!@isDisposableStack(this))
        @throwTypeError("DisposableStack.prototype.dispose requires that |this| be a DisposableStack object");

    if (@getDisposableStackInternalField(this, @disposableStackFieldState) === @DisposableStackStateDisposed)
        return @undefined;

    @putDisposableStackInternalField(this, @disposableStackFieldState, @DisposableStackStateDisposed);

    var stack = @getDisposableStackInternalField(this, @disposableStackFieldCapability);
    @assert(@isArray(stack));

    var i = stack.length;
    var thrown = false;
    var suppressed;

    while (i) {
        var resource = stack[--i];
        try {
            resource.method.@call(resource.value);
        } catch (e) {
            if (thrown)
                suppressed = new @SuppressedError(e, suppressed);
            else {
                thrown = true;
                suppressed = e;
            }
        }
    }

    @putDisposableStackInternalField(this, @disposableStackFieldCapability, []);
    if (thrown)
        throw suppressed;
}

// https://tc39.es/proposal-explicit-resource-management/#sec-disposablestack.prototype.move
function move()
{
    'use strict';

    if (!@isDisposableStack(this))
        @throwTypeError("DisposableStack.prototype.move requires that |this| be a DisposableStack object");

    if (@getDisposableStackInternalField(this, @disposableStackFieldState) === @DisposableStackStateDisposed)
        throw new @ReferenceError("DisposableStack.prototype.move requires that |this| be a pending DisposableStack object");

    var newDisposableStack = new @DisposableStack();
    @putDisposableStackInternalField(newDisposableStack, @disposableStackFieldCapability, @getDisposableStackInternalField(this, @disposableStackFieldCapability));

    @putDisposableStackInternalField(this, @disposableStackFieldCapability, []);
    @putDisposableStackInternalField(this, @disposableStackFieldState, @DisposableStackStateDisposed);

    return newDisposableStack;
}

// https://tc39.es/proposal-explicit-resource-management/#sec-disposablestack.prototype.use
function use(value)
{
    'use strict';

    if (!@isDisposableStack(this))
        @throwTypeError("DisposableStack.prototype.use requires that |this| be a DisposableStack object");

    if (@getDisposableStackInternalField(this, @disposableStackFieldState) === @DisposableStackStateDisposed)
        throw new @ReferenceError("DisposableStack.prototype.use requires that |this| be a pending DisposableStack object");

    @addDisposableResource(@getDisposableStackInternalField(this, @disposableStackFieldCapability), value, /* isAsync */ false);

    return value;
}
