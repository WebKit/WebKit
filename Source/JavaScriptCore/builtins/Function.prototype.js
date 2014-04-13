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

function call(thisArgument) {
    "use strict";
    return this.@call(...arguments);
}

function apply(thisValue, argumentValues) {
    "use strict";
    return this.@apply(thisValue, argumentValues);
}

function bind(thisValue /*, optional arguments */)
{
    "use strict";
    var boundFunction = this;
    if (typeof boundFunction !== "function")
        throw new @TypeError("Cannot bind non-function object.");
    var bindingFunction;
    var oversizedCall = undefined;
    if (arguments.length <= 1) {
        bindingFunction = function () {
            if (@IsConstructor)
                return new boundFunction(...arguments);
            return boundFunction.@apply(thisValue, arguments);
        }
    } else {
        var length = arguments.length;
        switch (length - 1 /* skip thisValue */) {
        case 1: {
            var boundParameter = arguments[1];
            bindingFunction = function () {
                var argumentLength = arguments.length;
                if (!argumentLength) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter);
                    return boundFunction.@call(thisValue, boundParameter);
                }
                if (argumentLength == 1) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter, arguments[0]);
                    return boundFunction.@call(thisValue, boundParameter, arguments[0]);
                }
                if (argumentLength == 2) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter, arguments[0], arguments[1]);
                    return boundFunction.@call(thisValue, boundParameter, arguments[0], arguments[1]);
                }
                if (argumentLength == 3) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter, arguments[0], arguments[1], arguments[2]);
                    return boundFunction.@call(thisValue, boundParameter, arguments[0], arguments[1], arguments[2]);
                }
                var completeArguments = [boundParameter, ...arguments];
                if (!oversizedCall) {
                    oversizedCall = function (isConstruct, boundFunction, thisValue, completeArguments) {
                        if (isConstruct)
                            return new boundFunction(...completeArguments);
                        return boundFunction.@apply(thisValue, completeArguments);
                    }
                }
                return oversizedCall(@IsConstructor, boundFunction, thisValue, completeArguments);
            }
            break;
        }
        case 2: {
            var boundParameter1 = arguments[1];
            var boundParameter2 = arguments[2];
            bindingFunction = function () {
                if (!arguments.length) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter1, boundParameter2);
                    return boundFunction.@call(thisValue, boundParameter1, boundParameter2);
                }
                if (arguments.length == 1) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter1, boundParameter2, arguments[0]);
                    return boundFunction.@call(thisValue, boundParameter1, boundParameter2, arguments[0]);
                }
                if (arguments.length == 2) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter1, boundParameter2, arguments[0], arguments[1]);
                    return boundFunction.@call(thisValue, boundParameter1, boundParameter2, arguments[0], arguments[1]);
                }
                if (arguments.length == 3) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter1, boundParameter2, arguments[0], arguments[1], arguments[2]);
                    return boundFunction.@call(thisValue, boundParameter1, boundParameter2, arguments[0], arguments[1], arguments[2]);
                }
                var completeArguments = [boundParameter1, boundParameter2, ...arguments];
                if (!oversizedCall) {
                    oversizedCall = function (isConstruct, boundFunction, thisValue, completeArguments) {
                        if (isConstruct)
                            return new boundFunction(...completeArguments);
                        return boundFunction.@apply(thisValue, completeArguments);
                    }
                }
                return oversizedCall(@IsConstructor, boundFunction, thisValue, completeArguments);
            }
            break;
        }
        case 3: {
            var boundParameter1 = arguments[1];
            var boundParameter2 = arguments[2];
            var boundParameter3 = arguments[3];
            bindingFunction = function () {
                if (!arguments.length) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter1, boundParameter2, boundParameter3);
                    return boundFunction.@call(thisValue, boundParameter1, boundParameter2, boundParameter3);
                }
                if (arguments.length == 1) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter1, boundParameter2, boundParameter3, arguments[0]);
                    return boundFunction.@call(thisValue, boundParameter1, boundParameter2, boundParameter3, arguments[0]);
                }
                if (arguments.length == 2) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter1, boundParameter2, boundParameter3, arguments[0], arguments[1]);
                    return boundFunction.@call(thisValue, boundParameter1, boundParameter2, boundParameter3, arguments[0], arguments[1]);
                }
                if (arguments.length == 3) {
                    if (@IsConstructor)
                        return new boundFunction(boundParameter1, boundParameter2, boundParameter3, arguments[0], arguments[1], arguments[2]);
                    return boundFunction.@call(thisValue, boundParameter1, boundParameter2, boundParameter3, arguments[0], arguments[1], arguments[2]);
                }
                var completeArguments = [boundParameter1, boundParameter2, boundParameter3, ...arguments];
                if (!oversizedCall) {
                    oversizedCall = function (isConstruct, boundFunction, thisValue, completeArguments) {
                        if (isConstruct)
                            return new boundFunction(...completeArguments);
                        return boundFunction.@apply(thisValue, completeArguments);
                    }
                }
                return oversizedCall(@IsConstructor, boundFunction, thisValue, completeArguments);
            }
            break;
        }
        default:
            var boundParameters = [];
            for (var i = 1; i < length; i++)
                boundParameters[i - 1] = arguments[i];
            
            bindingFunction = function () {
                if (!arguments.length) {
                    if (@IsConstructor)
                        return new boundFunction(...boundParameters);
                    return boundFunction.@apply(thisValue, boundParameters);
                }
                
                var completeArguments = [];
                var localBoundParameters = boundParameters;
                var boundLength = localBoundParameters.length;
                for (var i = 0; i < boundLength; i++)
                    completeArguments[i] = localBoundParameters[i];
                for (var i = 0; i < arguments.length; i++)
                    completeArguments[i + boundLength] = arguments[i];
                if (@IsConstructor)
                    return new boundFunction(...completeArguments);
                else
                    return boundFunction.@apply(thisValue, completeArguments);
            }
        }
    }
    bindingFunction.@boundFunctionName = this.name;
    bindingFunction.@boundFunction = boundFunction.@boundFunction || boundFunction;
    var boundLength = boundFunction.length - (arguments.length - 1);
    if (boundLength < 0)
        boundLength = 0;
    bindingFunction.@boundFunctionLength = boundLength;
    @SetTypeErrorAccessor(bindingFunction, "arguments");
    @SetTypeErrorAccessor(bindingFunction, "caller");
    return bindingFunction;
}
