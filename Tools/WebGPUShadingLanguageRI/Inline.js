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

function inline(program)
{
    for (let funcList of program.functions.values()) {
        for (let func of funcList) {
            if (!func.typeParameters.length) {
                func = program.funcInstantiator.getUnique(func, [])
                _inlineFunction(program, func, new VisitingSet(func));
            }
        }
    }
}

function _inlineFunction(program, func, visiting)
{
    if (func.typeParameters.length)
        throw new Error("Cannot inline function that has type parameters");
    if (func.inlined || func.isNative)
        return;
    
    func.visit(new LateChecker());
    
    // This is the precise time when we can build EBuffers in order to get them to be uniqued by
    // type instantiation but nothing else.
    func.visit(new StructLayoutBuilder());
    func.visit(new EBufferBuilder(program));
    
    func.rewrite(new Inliner(program, func, visiting));

    func.inlined = true;
}

function resolveInlinedFunction(program, name, typeArguments, argumentTypes, allowEntryPoint = false)
{
    let overload = program.globalNameContext.resolveFuncOverload(name, typeArguments, argumentTypes, undefined, allowEntryPoint);
    if (!overload.func)
        return overload.failures;
    if (!overload.func.typeParameters)
        return overload.func;
    let func = program.funcInstantiator.getUnique(overload.func, overload.typeArguments);
    _inlineFunction(program, func, new VisitingSet(overload.func));
    return func;
}
