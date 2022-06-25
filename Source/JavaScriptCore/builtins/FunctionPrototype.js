/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

function call(thisArgument)
{
    "use strict";

    var argumentValues = [];
    // Start from 1 to ignore thisArgument
    for (var i = 1; i < arguments.length; i++)
        @putByValDirect(argumentValues, i-1, arguments[i]);

    return this.@apply(thisArgument, argumentValues);
}

function apply(thisValue, argumentValues)
{
    "use strict";

    return this.@apply(thisValue, argumentValues);
}

@overriddenName="[Symbol.hasInstance]"
function symbolHasInstance(value)
{
    "use strict";

    if (!@isCallable(this))
        return false;

    if (@isBoundFunction(this))
        return @hasInstanceBoundFunction(this, value);

    var target = this.prototype;
    return @instanceOf(value, target);
}

function bind(thisValue)
{
    "use strict";

    var target = this;
    if (!@isCallable(target))
        @throwTypeError("|this| is not a function inside Function.prototype.bind");

    var argumentCount = @argumentCount();
    var boundArgs = null;
    var numBoundArgs = 0;
    if (argumentCount > 1) {
        numBoundArgs = argumentCount - 1;
        boundArgs = @createArgumentsButterfly();
    }

    var length = 0;
    if (@hasOwnLengthProperty(target)) {
        var lengthValue = target.length;
        if (typeof lengthValue === "number") {
            lengthValue = @toIntegerOrInfinity(lengthValue);
            // Note that we only care about positive lengthValues, however, this comparision
            // against numBoundArgs suffices to prove we're not a negative number.
            if (lengthValue > numBoundArgs)
                length = lengthValue - numBoundArgs;
        }
    }

    var name = target.name;
    if (typeof name !== "string")
        name = "";

    return @makeBoundFunction(target, thisValue, boundArgs, length, name);
}
