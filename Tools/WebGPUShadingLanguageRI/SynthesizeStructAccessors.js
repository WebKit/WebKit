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

function synthesizeStructAccessors(program)
{
    for (let type of program.types.values()) {
        if (!(type instanceof StructType))
            continue;
        
        for (let field of type.fields) {
            function setupImplementationData(nativeFunc, implementation)
            {
                nativeFunc.visitImplementationData = (implementationData, visitor) => {
                    // Visiting the type first ensures that the struct layout builder figures out the field's
                    // offset.
                    implementationData.type.visit(visitor);
                };

                nativeFunc.implementation = (argumentList, node) => {
                    return implementation(argumentList, field.offset, type.size, field.type.size);
                };
            }
            
            function createFieldType()
            {
                return field.type.visit(new Rewriter());
            }
            
            function createTypeRef()
            {
                return TypeRef.wrap(type);
            }
            
            let isCast = false;
            let shaderType;
            let nativeFunc;

            // The getter: operator.field
            nativeFunc = new NativeFunc(
                field.origin, "operator." + field.name, createFieldType(),
                [new FuncParameter(field.origin, null, createTypeRef())], isCast, shaderType);
            setupImplementationData(nativeFunc, ([base], offset, structSize, fieldSize) => {
                let result = new EPtr(new EBuffer(fieldSize), 0);
                result.copyFrom(base.plus(offset), fieldSize);
                return result;
            });
            program.add(nativeFunc);
            
            // The setter: operator.field=
            nativeFunc = new NativeFunc(
                field.origin, "operator." + field.name + "=", createTypeRef(),
                [
                    new FuncParameter(field.origin, null, createTypeRef()),
                    new FuncParameter(field.origin, null, createFieldType())
                ],
                isCast, shaderType);
            setupImplementationData(nativeFunc, ([base, value], offset, structSize, fieldSize) => {
                let result = new EPtr(new EBuffer(structSize), 0);
                result.copyFrom(base, structSize);
                result.plus(offset).copyFrom(value, fieldSize);
                return result;
            });
            program.add(nativeFunc);
            
            // The ander: operator&.field
            function setupAnder(addressSpace)
            {
                nativeFunc = new NativeFunc(
                    field.origin, "operator&." + field.name, new PtrType(field.origin, addressSpace, createFieldType()),
                    [
                        new FuncParameter(
                            field.origin, null,
                            new PtrType(field.origin, addressSpace, createTypeRef()))
                    ],
                    isCast, shaderType);
                setupImplementationData(nativeFunc, ([base], offset, structSize, fieldSize) => {
                    base = base.loadValue();
                    if (!base)
                        throw new WTrapError(node.origin.originString, "Null dereference");
                    return EPtr.box(base.plus(offset));
                });
                program.add(nativeFunc);
            }
            
            setupAnder("thread");
            setupAnder("threadgroup");
            setupAnder("device");
            setupAnder("constant");
        }
    }
}
