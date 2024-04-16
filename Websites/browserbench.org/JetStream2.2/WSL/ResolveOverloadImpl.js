/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
"use strict";

function resolveOverloadImpl(functions, typeArguments, argumentTypes, returnType, allowEntryPoint = false)
{
    if (!functions)
        throw new Error("Null functions; that should have been caught by the caller.");
    
    let failures = [];
    let successes = [];
    for (let func of functions) {
        if (!allowEntryPoint && func.shaderType) {
            failures.push(new OverloadResolutionFailure(func, "Function is a " + func.shaderType + " shader, so it cannot be called from within an existing shader."))
            continue;
        }
        let overload = inferTypesForCall(func, typeArguments, argumentTypes, returnType);
        if (overload.failure)
            failures.push(overload.failure);
        else
            successes.push(overload);
    }
    
    if (!successes.length)
        return {failures: failures};
    
    let minimumConversionCost = successes.reduce(
        (result, overload) => Math.min(result, overload.unificationContext.conversionCost),
        Infinity);
    successes = successes.filter(
        overload => overload.unificationContext.conversionCost == minimumConversionCost);
    
    // If any of the signatures are restricted then we consider those first. This is an escape mechanism for
    // built-in things.
    // FIXME: It should be an error to declare a function that is at least as specific as a restricted function.
    // https://bugs.webkit.org/show_bug.cgi?id=176580
    let hasRestricted = successes.reduce(
        (result, overload) => result || overload.func.isRestricted,
        false);
    if (hasRestricted)
        successes = successes.filter(overload => overload.func.isRestricted);
    
    // We are only interested in functions that are at least as specific as all of the others. This means
    // that they can be "turned around" and applied onto all of the other functions in the list.
    let prunedSuccesses = [];
    for (let i = 0; i < successes.length; ++i) {
        let ok = true;
        let argumentFunc = successes[i].func;
        for (let j = 0; j < successes.length; ++j) {
            if (i == j)
                continue;
            let parameterFunc = successes[j].func;
            let overload = inferTypesForCall(
                parameterFunc,
                typeArguments.length ? argumentFunc.typeParameters : [],
                argumentFunc.parameterTypes,
                argumentFunc.returnTypeForOverloadResolution);
            if (!overload.func) {
                ok = false;
                break;
            }
        }
        if (ok)
            prunedSuccesses.push(successes[i]);
    }
    
    if (prunedSuccesses.length == 1)
        return prunedSuccesses[0];
    
    let ambiguityList;
    let message;
    if (prunedSuccesses.length == 0) {
        ambiguityList = successes;
        message = "Ambiguous overload - no function can be applied to all others";
    } else {
        ambiguityList = prunedSuccesses;
        message = "Ambiguous overload - functions mutually applicable";
    }
    
    return {failures: ambiguityList.map(overload => new OverloadResolutionFailure(overload.func, message))};
}
