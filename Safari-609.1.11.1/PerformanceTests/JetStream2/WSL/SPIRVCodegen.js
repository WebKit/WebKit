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

function generateSPIRV(spirv, program)
{

    function findEntryPoints()
    {
        let entryPoints = [];
        for (let functionNames of program.functions.values()) {
            for (let func of functionNames) {
                switch (func.shaderType) {
                case "vertex":
                case "fragment":
                    entryPoints.push(func);
                    break;
                }
            }
        }
        return entryPoints;
    }

    let currentId = 3;
    let currentLocation = 0;
    let typeMap = new Map();
    let reverseTypeMap = new Map();
    let entryPoints = [];

    typeMap.set(program.intrinsics.void, currentId++);
    typeMap.set(program.intrinsics.uint32, currentId++);

    for (let entryPoint of findEntryPoints()) {
        let inlinedShader = program.funcInstantiator.getUnique(entryPoint, []);
        _inlineFunction(program, inlinedShader, new VisitingSet(entryPoint));

        let typeAnalyzer = new SPIRVTypeAnalyzer(program, typeMap, currentId);
        inlinedShader.visit(typeAnalyzer);
        currentId = typeAnalyzer.currentId;

        currentLocation = 0;
        let valueAnalyzer = new SPIRVPrimitiveVariableAnalyzer(program, typeMap, currentId, currentLocation);
        inlinedShader.returnType.visit(valueAnalyzer);
        currentId = valueAnalyzer.currentId;
        let outputValues = valueAnalyzer.result;

        let inputValues = [];
        for (let parameter of inlinedShader.parameters) {
            if (parameter.type.type instanceof StructType) {
                let valueAnalyzer = new SPIRVPrimitiveVariableAnalyzer(program, typeMap, currentId, currentLocation, parameter.name);
                parameter.visit(valueAnalyzer);
                currentId = valueAnalyzer.currentId;
                currentLocation = valueAnalyzer.currentLocation;
                for (let inputValue of valueAnalyzer.result)
                    inputValues.push(inputValue);
            } else if (parameter.type.type instanceof ArrayRefType) {
                // FIXME: Implement this.
            }
        }

        entryPoints.push({ id: currentId++, shader: inlinedShader, inputs: inputValues, outputs: outputValues });
    }
    
    for (let type of typeMap) {
        if (typeof type[1] == "object")
            reverseTypeMap.set(type[1].id, type[0]);
        else
            reverseTypeMap.set(type[1], type[0]);
    }














    function emitTypes(assembler) {
        let emittedTypes = new Set();
        function doEmitTypes(type)
        {
            if (emittedTypes.has(type[0]))
                return;
            emittedTypes.add(type[0]);
            if (typeof type[1] == "object") {
                if (type[1].fieldTypes) {
                    for (let fieldType of type[1].fieldTypes) {
                        let key = reverseTypeMap.get(fieldType);
                        let value = typeMap.get(key);
                        doEmitTypes([key, value]);
                    }
                    switch (type[0]) {
                    case "struct vec2<> { int32 x; int32 y }":
                    case "struct vec2<> { uint32 x; uint32 y; }":
                    case "struct vec2<> { float32 x; float32 y; }":
                    case "struct vec2<> { float64 x; float64 y; }":
                    case "struct vec3<> { int32 x; int32 y; int32 z; }":
                    case "struct vec3<> { uint32 x; uint32 y; uint32 z; }":
                    case "struct vec3<> { float32 x; float32 y; float32 z; }":
                    case "struct vec3<> { float64 x; float64 y; float64 z; }":
                    case "struct vec4<> { int32 x; int32 y; int32 z; int32 w; }":
                    case "struct vec4<> { uint32 x; uint32 y; uint32 z; uint32 w; }":
                    case "struct vec4<> { float32 x; float32 y; float32 z; float32 w; }":
                    case "struct vec4<> { float64 x; float64 y; float64 z; float64 w; }":
                        assembler.append(new spirv.ops.TypeVector(type[1].id, type[1].fieldTypes[0], type[1].fieldTypes.length));
                        break;
                    default:
                        assembler.append(new spirv.ops.TypeStruct(type[1].id, ...type[1].fieldTypes));
                        break;
                    }
                } else {
                    if (!type[1].elementType)
                        throw new Error("Unknown type!");
            
                    let elementType = type[1].elementType;
                    let key = reverseTypeMap.get(elementType);
                    let value = typeMap.get(key);
                    doEmitTypes([key, value]);

                    let id = currentId++;
                    assembler.append(new spirv.ops.Constant(typeMap.get(program.intrinsics.uint32), id, type[1].numElements));
                    assembler.append(new spirv.ops.TypeArray(type[1].id, elementType, id));
                }
            } else {
                switch (type[0].name) {
                case "void":
                    assembler.append(new spirv.ops.TypeVoid(type[1]));
                    break;
                case "bool":
                    assembler.append(new spirv.ops.TypeBool(type[1]));
                    break;
                case "int32":
                    assembler.append(new spirv.ops.TypeInt(type[1], 32, 1));
                    break;
                case "uint32":
                case "uint8":
                    assembler.append(new spirv.ops.TypeInt(type[1], 32, 0));
                    break;
                case "float32":
                    assembler.append(new spirv.ops.TypeFloat(type[1], 32));
                    break;
                case "float64":
                    assembler.append(new spirv.ops.TypeFloat(type[1], 64));
                    break;
                }
            }
        }
        doEmitTypes([program.intrinsics.uint32, typeMap.get(program.intrinsics.uint32)]);
        for (let type of typeMap)
            doEmitTypes(type)
    }












    let constants = new Map();
    class ConstantFinder extends Visitor {
        visitGenericLiteralType(node)
        {
            let type = node.type;
            while (type instanceof TypeRef)
                type = type.type;
            let values;
            switch (type) {
            case program.intrinsics.bool:
                values = [node.value];
                break;
            case program.intrinsics.int32:
            case program.intrinsics.uint32:
            case program.intrinsics.uint8:
                values = [node.value]
                break;
            case program.intrinsics.float: {
                let arrayBuffer = new ArrayBuffer(Math.max(Uint32Array.BYTES_PER_ELEMENT, Float32Array.BYTES_PER_ELEMENT));
                let floatView = new Float32Array(arrayBuffer);
                let uintView = new Uint32Array(arrayBuffer);
                floatView[0] = node.value;
                values = uintView;
                break;
            }
            case program.intrinsics.double: {
                let arrayBuffer = new ArrayBuffer(Math.max(Uint32Array.BYTES_PER_ELEMENT, Float64Array.BYTES_PER_ELEMENT));
                let doubleView = new Float64Array(arrayBuffer);
                let uintView = new Uint32Array(arrayBuffer);
                doubleView[0] = node.value;
                values = uintView;
                break;
            }
            default:
                throw new Error("Unrecognized literal.");
            }
            constants.set(node, { id: currentId++, typeId: typeMap.get(type), type: type, values: values });
        }
    }
    for (let entryPoint of entryPoints)
        entryPoint.shader.visit(new ConstantFinder());












    let assembler = new SPIRVAssembler();
    // 1. All OpCapability instructions
    assembler.append(new spirv.ops.Capability(spirv.kinds.Capability.Shader));
    assembler.append(new spirv.ops.Capability(spirv.kinds.Capability.Float64));
    // 2. Optional OpExtension instructions
    // 3. Optional OpExtInstImport instructions
    // 4. The single required OpMemoryModel instruction
    // FIXME: Figure out if we can use the Simple memory model instead of the GLSL memory model.
    // The spec says nothing about what the difference between them is. 💯
    assembler.append(new spirv.ops.MemoryModel(spirv.kinds.AddressingModel.Logical, spirv.kinds.MemoryModel.GLSL450));

    // 5. All entry point declarations
    for (let entryPoint of entryPoints) {
        let executionModel;
        switch (entryPoint.shader.shaderType) {
        case "vertex":
            executionModel = spirv.kinds.ExecutionModel.Vertex;
            break;
        case "fragment":
            executionModel = spirv.kinds.ExecutionModel.Fragment;
            break;
        }
        let id = entryPoint.id;
        let name = entryPoint.shader.name;
        let interfaceIds = []
        for (let value of entryPoint.inputs)
            interfaceIds.push(value.id);
        for (let value of entryPoint.outputs)
            interfaceIds.push(value.id);
        assembler.append(new spirv.ops.EntryPoint(executionModel, id, name, ...interfaceIds));
    }

    // 6. All execution mode declarations
    for (let entryPoint of entryPoints) {
        let id = entryPoint.id;
        assembler.append(new spirv.ops.ExecutionMode(id, spirv.kinds.ExecutionMode.OriginLowerLeft));
    }

    // 7. These debug instructions
    // 8. All annotation instructions
    // FIXME: There are probably more annotations that are required than just location.
    let locations = [];
    for (let entryPoint of entryPoints) {
        switch (entryPoint.shader.shaderType) {
        case "vertex":
            for (let input of entryPoint.inputs) {
                assembler.append(new spirv.ops.Decorate(input.id, spirv.kinds.Decoration.Location, input.location));
                locations.push({ name: entryPoint.shader.name + "." + input.name, location: input.location });
            }
            break;
        case "fragment":
            for (let output of entryPoint.outputs) {
                assembler.append(new spirv.ops.Decorate(output.id, spirv.kinds.Decoration.Location, output.location));
                locations.push({ name: entryPoint.shader.name + "." + output.name, location: output.location });
            }
            break;
        }
    }

    // 9. All type declarations, all constant instructions, and all global variable declarations
    emitTypes(assembler);
    let functionType = currentId++;
    assembler.append(new spirv.ops.TypeFunction(functionType, typeMap.get(program.intrinsics.void)));
    for (let constant of constants) {
        if (constant[1].type == program.intrinsics.bool) {
            if (constant[1].value[0])
                assembler.append(new spirv.ops.ConstantTrue(constant[1].id));
            else
                assembler.append(new spirv.ops.ConstantFalse(constant[1].id));
        } else
            assembler.append(new spirv.ops.Constant(constant[1].typeId, constant[1].id, ...constant[1].values));
    }
    for (let entryPoint of entryPoints) {
        for (let input of entryPoint.inputs)
            assembler.append(new spirv.ops.Variable(input.type, input.id, spirv.kinds.StorageClass.Input));
        for (let output of entryPoint.outputs)
            assembler.append(new spirv.ops.Variable(output.type, output.id, spirv.kinds.StorageClass.Output));
    }

    // 10. All function declarations
    // 11. All function definitions
    for (let entryPoint of entryPoints) {
        assembler.append(new spirv.ops.Function(typeMap.get(program.intrinsics.void), entryPoint.id, [spirv.kinds.FunctionControl.None], functionType));
        assembler.append(new spirv.ops.FunctionEnd());
    }

    return { file: assembler.result, locations: locations };
}

