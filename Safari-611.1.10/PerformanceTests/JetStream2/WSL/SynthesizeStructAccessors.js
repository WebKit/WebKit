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

function synthesizeStructAccessors(program)
{
    for (let type of program.types.values()) {
        if (!(type instanceof StructType))
            continue;
        
        for (let field of type.fields) {
            function createTypeParameters()
            {
                return type.typeParameters.map(
                    typeParameter => typeParameter.visit(new TypeParameterRewriter()));
            }
            
            function createTypeArguments()
            {
                return typeParameters.map(typeParameter => typeParameter.visit(new AutoWrapper()));
            }
            
            function setupImplementationData(nativeFunc, implementation)
            {
                nativeFunc.instantiateImplementation = substitution => {
                    let newType = type.instantiate(nativeFunc.typeParameters.map(typeParameter => {
                        let substitute = substitution.map.get(typeParameter);
                        if (!substitute)
                            throw new Error("Null substitute for type parameter " + typeParameter);
                        return substitute;
                    }));
                    return {type: newType, fieldName: field.name};
                };
                nativeFunc.visitImplementationData = (implementationData, visitor) => {
                    // Visiting the type first ensures that the struct layout builder figures out the field's
                    // offset.
                    implementationData.type.visit(visitor);
                };
                nativeFunc.didLayoutStructsInImplementationData = implementationData => {
                    let structSize = implementationData.type.size;
                    if (structSize == null)
                        throw new Error("No struct size for " + nativeFunc);
                    let field = implementationData.type.fieldByName(implementationData.fieldName);
                    if (!field)
                        throw new Error("Could not find field");
                    let offset = field.offset;
                    let fieldSize = field.type.size;
                    if (fieldSize == null)
                        throw new Error("No field size for " + nativeFunc);
                    if (offset == null)
                        throw new Error("No offset for " + nativeFunc);
                    
                    implementationData.offset = offset;
                    implementationData.structSize = structSize;
                    implementationData.fieldSize = fieldSize;
                };
                nativeFunc.implementation = (argumentList, node) => {
                    let nativeFuncInstance = node.nativeFuncInstance;
                    let implementationData = nativeFuncInstance.implementationData;
                    return implementation(
                        argumentList,
                        implementationData.offset,
                        implementationData.structSize,
                        implementationData.fieldSize);
                };
            }
            
            function createFieldType()
            {
                return field.type.visit(new Substitution(type.typeParameters, typeParameters));
            }
            
            function createTypeRef()
            {
                return TypeRef.instantiate(type, createTypeArguments());
            }
            
            let isCast = false;
            let shaderType;
            let typeParameters;
            let nativeFunc;

            // The getter: operator.field
            typeParameters = createTypeParameters();
            nativeFunc = new NativeFunc(
                field.origin, "operator." + field.name, createFieldType(), typeParameters,
                [new FuncParameter(field.origin, null, createTypeRef())], isCast, shaderType);
            setupImplementationData(nativeFunc, ([base], offset, structSize, fieldSize) => {
                let result = new EPtr(new EBuffer(fieldSize), 0);
                result.copyFrom(base.plus(offset), fieldSize);
                return result;
            });
            program.add(nativeFunc);
            
            // The setter: operator.field=
            typeParameters = createTypeParameters();
            nativeFunc = new NativeFunc(
                field.origin, "operator." + field.name + "=", createTypeRef(), typeParameters,
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
                typeParameters = createTypeParameters();
                nativeFunc = new NativeFunc(
                    field.origin, "operator&." + field.name, new PtrType(field.origin, addressSpace, createFieldType()),
                    typeParameters,
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
