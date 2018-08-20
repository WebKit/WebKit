/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

function synthesizeArrayOperatorLength(program)
{
    const arrayTypes = new Set();

    class FindArrayTypes extends Visitor {
        visitArrayType(node)
        {
            arrayTypes.add(node);
        }
    }

    program.visit(new FindArrayTypes());

    const uint = TypeRef.wrap(program.globalNameContext.get(Type, "uint"));
    const nameResolver = new NameResolver(program.globalNameContext);
    uint.visit(nameResolver);

    for (let arrayType of arrayTypes) {
        const paramType = TypeRef.wrap(arrayType);
        paramType.visit(nameResolver);
        paramType.type.elementType.visit(nameResolver);

        let possibleExistingFunctions = program.functions.get("operator.length");
        if (possibleExistingFunctions) {
            // Ignore user-defined functions, only include those introduced below.
            possibleExistingFunctions = possibleExistingFunctions.filter(t => t instanceof NativeFunc);
            const overloads = resolveOverloadImpl(possibleExistingFunctions, [ paramType ], uint);
            if (overloads.func)
                continue;
        }

        let nativeFunc = new NativeFunc(
            arrayType.origin, "operator.length", uint,
            [ new FuncParameter(arrayType.origin, null, paramType) ],
            false, null);
        nativeFunc.implementation = ([array]) => EPtr.box(arrayType.numElementsValue);
        program.add(nativeFunc);

    }
}
