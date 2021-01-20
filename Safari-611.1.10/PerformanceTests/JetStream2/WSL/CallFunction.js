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

// This allows you to pass structs and arrays in-place, but it's a more annoying API.
function callFunction(program, name, typeArguments, argumentList)
{
    let argumentTypes = argumentList.map(argument => argument.type);
    let funcOrFailures = resolveInlinedFunction(program, name, typeArguments, argumentTypes, true);
    if (!(funcOrFailures instanceof Func)) {
        let failures = funcOrFailures;
        throw new WTypeError("<callFunction>", "Cannot resolve function call " + name + "<" + typeArguments + ">(" + argumentList + ")" + (failures.length ? "; tried:\n" + failures.join("\n") : ""));
    }
    let func = funcOrFailures;
    for (let i = 0; i < func.parameters.length; ++i) {
        let type = argumentTypes[i].instantiatedType;
        type.visit(new StructLayoutBuilder());
        func.parameters[i].ePtr.copyFrom(argumentList[i].ePtr, type.size);
    }
    let result = new Evaluator(program).runFunc(func);
    return new TypedValue(func.uninstantiatedReturnType, result);
}

