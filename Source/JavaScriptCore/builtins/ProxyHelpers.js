/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
function performProxyObjectHas(propertyName)
{
    "use strict";

    var target = @getProxyInternalField(this, @proxyFieldTarget);
    var handler = @getProxyInternalField(this, @proxyFieldHandler);

    if (handler === null)
        @throwTypeError("Proxy has already been revoked. No more operations are allowed to be performed on it");

    var trap = handler.has;
    if (@isUndefinedOrNull(trap))
        return propertyName in target;

    if (!@isCallable(trap))
        @throwTypeError("'has' property of a Proxy's handler should be callable");

    if (trap.@call(handler, target, propertyName))
        return true;

    if (@mustValidateResultOfProxyTrapsExceptGetAndSet(target))
        @handleNegativeProxyHasTrapResult(target, propertyName);

    return false;
}

@linkTimeConstant
function performProxyObjectHasByVal(propertyName)
{
    "use strict";

    var target = @getProxyInternalField(this, @proxyFieldTarget);
    var handler = @getProxyInternalField(this, @proxyFieldHandler);
    var propertyName = @toPropertyKey(propertyName);

    if (handler === null)
        @throwTypeError("Proxy has already been revoked. No more operations are allowed to be performed on it");

    var trap = handler.has;
    if (@isUndefinedOrNull(trap))
        return propertyName in target;

    if (!@isCallable(trap))
        @throwTypeError("'has' property of a Proxy's handler should be callable");

    if (trap.@call(handler, target, propertyName))
        return true;

    if (@mustValidateResultOfProxyTrapsExceptGetAndSet(target))
        @handleNegativeProxyHasTrapResult(target, propertyName);

    return false;
}

@linkTimeConstant
function performProxyObjectGet(propertyName, receiver)
{
    "use strict";

    var target = @getProxyInternalField(this, @proxyFieldTarget);
    var handler = @getProxyInternalField(this, @proxyFieldHandler);

    if (handler === null)
        @throwTypeError("Proxy has already been revoked. No more operations are allowed to be performed on it");

    var trap = handler.get;
    if (@isUndefinedOrNull(trap))
        return @getByValWithThis(target, receiver, propertyName);

    if (!@isCallable(trap))
        @throwTypeError("'get' property of a Proxy's handler should be callable");

    var trapResult = trap.@call(handler, target, propertyName, @toThis(receiver));

    if (@mustValidateResultOfProxyGetAndSetTraps(target))
        @handleProxyGetTrapResult(trapResult, target, propertyName);

    return trapResult;
}

@linkTimeConstant
function performProxyObjectGetByVal(propertyName, receiver)
{
    "use strict";

    var target = @getProxyInternalField(this, @proxyFieldTarget);
    var handler = @getProxyInternalField(this, @proxyFieldHandler);
    var propertyName = @toPropertyKey(propertyName);

    if (handler === null)
        @throwTypeError("Proxy has already been revoked. No more operations are allowed to be performed on it");

    var trap = handler.get;
    if (@isUndefinedOrNull(trap))
        return @getByValWithThis(target, receiver, propertyName);

    if (!@isCallable(trap))
        @throwTypeError("'get' property of a Proxy's handler should be callable");

    var trapResult = trap.@call(handler, target, propertyName, @toThis(receiver));

    if (@mustValidateResultOfProxyGetAndSetTraps(target))
        @handleProxyGetTrapResult(trapResult, target, propertyName);

    return trapResult;
}

@linkTimeConstant
function performProxyObjectSetSloppy(propertyName, receiver, value)
{
    "use strict";

    var target = @getProxyInternalField(this, @proxyFieldTarget);
    var handler = @getProxyInternalField(this, @proxyFieldHandler);

    if (handler === null)
        @throwTypeError("Proxy has already been revoked. No more operations are allowed to be performed on it");

    var trap = handler.set;
    if (@isUndefinedOrNull(trap)) {
        @putByValWithThisSloppy(target, receiver, propertyName, value);
        return;
    }

    if (!@isCallable(trap))
        @throwTypeError("'set' property of a Proxy's handler should be callable");

    if (!trap.@call(handler, target, propertyName, value, @toThis(receiver)))
        return;

    if (@mustValidateResultOfProxyGetAndSetTraps(target))
        @handlePositiveProxySetTrapResult(target, propertyName, value);
}

@linkTimeConstant
function performProxyObjectSetStrict(propertyName, receiver, value)
{
    "use strict";

    var target = @getProxyInternalField(this, @proxyFieldTarget);
    var handler = @getProxyInternalField(this, @proxyFieldHandler);

    if (handler === null)
        @throwTypeError("Proxy has already been revoked. No more operations are allowed to be performed on it");

    var trap = handler.set;
    if (@isUndefinedOrNull(trap)) {
        @putByValWithThisStrict(target, receiver, propertyName, value);
        return;
    }

    if (!@isCallable(trap))
        @throwTypeError("'set' property of a Proxy's handler should be callable");

    if (!trap.@call(handler, target, propertyName, value, @toThis(receiver)))
        @throwTypeError("Proxy object's 'set' trap returned falsy value for property '" + @String(propertyName) + "'");

    if (@mustValidateResultOfProxyGetAndSetTraps(target))
        @handlePositiveProxySetTrapResult(target, propertyName, value);
}

@linkTimeConstant
function performProxyObjectSetByValSloppy(propertyName, receiver, value)
{
    "use strict";

    var target = @getProxyInternalField(this, @proxyFieldTarget);
    var handler = @getProxyInternalField(this, @proxyFieldHandler);
    var propertyName = @toPropertyKey(propertyName);

    if (handler === null)
        @throwTypeError("Proxy has already been revoked. No more operations are allowed to be performed on it");

    var trap = handler.set;
    if (@isUndefinedOrNull(trap)) {
        @putByValWithThisSloppy(target, receiver, propertyName, value);
        return;
    }

    if (!@isCallable(trap))
        @throwTypeError("'set' property of a Proxy's handler should be callable");

    if (!trap.@call(handler, target, propertyName, value, @toThis(receiver)))
        return;

    if (@mustValidateResultOfProxyGetAndSetTraps(target))
        @handlePositiveProxySetTrapResult(target, propertyName, value);
}

@linkTimeConstant
function performProxyObjectSetByValStrict(propertyName, receiver, value)
{
    "use strict";

    var target = @getProxyInternalField(this, @proxyFieldTarget);
    var handler = @getProxyInternalField(this, @proxyFieldHandler);
    var propertyName = @toPropertyKey(propertyName);

    if (handler === null)
        @throwTypeError("Proxy has already been revoked. No more operations are allowed to be performed on it");

    var trap = handler.set;
    if (@isUndefinedOrNull(trap)) {
        @putByValWithThisStrict(target, receiver, propertyName, value);
        return;
    }

    if (!@isCallable(trap))
        @throwTypeError("'set' property of a Proxy's handler should be callable");

    if (!trap.@call(handler, target, propertyName, value, @toThis(receiver)))
        @throwTypeError("Proxy object's 'set' trap returned falsy value for property '" + @String(propertyName) + "'");

    if (@mustValidateResultOfProxyGetAndSetTraps(target))
        @handlePositiveProxySetTrapResult(target, propertyName, value);
}
