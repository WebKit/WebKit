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

const DefaultMetalSource = `#include <metal_stdlib>
using namespace metal;

`;

// Handles the compilation of Program AST instances to MSLCompileResult instances, which
// include the raw MSL. In general clients should call |whlslToMsl|, which parses,
// typechecks, and inlines the WHLSL before passing it to this compiler.
class MSLBackend {

    constructor(program)
    {
        this._program = program;
        this._declarations = [];
        this._funcNameMangler = new MSLNameMangler("F");
        this._typeUnifier = new MSLTypeUnifier();
        this._forwardTypeDecls = [];
        this._typeDefinitions = [];
        this._forwardFunctionDecls = [];
        this._functionDefintions = [];
    }

    get program()
    {
        return this._program;
    }

    get declarations()
    {
        return this._declarations;
    }

    get functionSources()
    {
        return this._functionSources;
    }

    compile()
    {
        try {
            const src = this._msl();
            const mangledMap = {};
            for (let func of this._functionDefintions) {
                const key = func.func.name;
                const value = this._funcNameMangler.mangle(func.func);
                mangledMap[key] = value;
            }

            return new MSLCompileResult(src, null, mangledMap, this._functionSources);
        } catch (e) {
            return new MSLCompileResult(null, e, null, null);
        }
    }

    _msl()
    {
        insertTrapParameter(this.program);

        const functionsToCompile = this._findFunctionsToCompile();
        for (let func of functionsToCompile)
            func.visit(this._typeUnifier);

        this._allTypeAttributes = new MSLTypeAttributesMap(functionsToCompile, this._typeUnifier);
        this._createTypeDecls();
        this._createFunctionDecls(functionsToCompile);

        let outputStr = DefaultMetalSource;

        const addSection = (title, decls) => {
            outputStr += `#pragma mark - ${title}\n\n${decls.join("\n\n")}\n\n`;
        };

        addSection("Forward type declarations", this._forwardTypeDecls);
        addSection("Type definitions", this._typeDefinitions);
        addSection("Forward function declarations", this._forwardFunctionDecls);
        addSection("Function definitions", this._functionDefintions);

        if (!this._allowComments)
            outputStr = this._removeCommentsAndEmptyLines(outputStr);

        return outputStr;
    }

    _findFunctionsToCompile()
    {
        const entryPointFunctions = [];
        for (let [name, instances] of this._program.functions) {
            for (let instance of instances) {
                if (instance.isEntryPoint)
                    entryPointFunctions.push(instance);
            }
        }
        const functions = new Set(entryPointFunctions);

        class FindFunctionsThatGetCalled extends Visitor {
            visitCallExpression(node)
            {
                super.visitCallExpression(node);
                if (node.func instanceof FuncDef) {
                    functions.add(node.func);
                    node.func.visit(this);
                }
            }
        }
        const findFunctionsThatGetCalledVisitor = new FindFunctionsThatGetCalled();
        for (let entryPoint of entryPointFunctions)
            entryPoint.visit(findFunctionsThatGetCalledVisitor);
        return Array.from(functions);
    }

    _createTypeDecls()
    {
        const typesThatNeedDeclaration = this._typeUnifier.typesThatNeedDeclaration();
        const typeDeclsInOrderInTypeDefOrder = this._sortTypeDefsAndForwardDeclarationsInTopologicalOrder(typesThatNeedDeclaration);
        for (let type of typeDeclsInOrderInTypeDefOrder) {
            if (type instanceof StructType)
                this._forwardTypeDecls.push(this._metalSourceForStructForwardDeclaration(type));
            else if (type instanceof ArrayRefType)
                this._forwardTypeDecls.push(this._metalSourceForArrayRefForwardDeclaration(type));
            else
                this._forwardTypeDecls.push(this._metalSourceForTypeDef(type));
        }

        const typeDeclsThatNeedDeclarationInOrder = this._sortTypeDeclarationsInTopologicalOrder(typesThatNeedDeclaration);
        for (let type of typeDeclsThatNeedDeclarationInOrder) {
            if (type instanceof StructType)
                this._typeDefinitions.push(this._metalSourceForStructDefinition(type));
            else if (type instanceof ArrayRefType)
                this._typeDefinitions.push(this._metalSourceForArrayRefDefinition(type));
        }
    }

    _createFunctionDecls(unifiedFunctionDefs)
    {
        for (let func of unifiedFunctionDefs) {
            this._forwardFunctionDecls.push(new MSLFunctionForwardDeclaration(this.program, this._funcNameMangler, func, this._typeUnifier, this._allTypeAttributes));
            this._functionDefintions.push(new MSLFunctionDefinition(this.program, this._funcNameMangler, func, this._typeUnifier, this._allTypeAttributes));
        }
    }

    _sortTypeDefsAndForwardDeclarationsInTopologicalOrder(typesToDeclare)
    {
        // All types are sorted in topological order in this method; every type needs a typedef.
        // We do not recurse into structs becasue this is *just* for forward declarations.
        const declarations = new Array();
        for (let type of typesToDeclare)
            declarations.push(type);

        let typeOrder = [];
        let visitedSet = new Set();
        const typeUnifier = this._typeUnifier;
        class TypeOrderer extends Visitor {
            _visitType(node, visitCallback)
            {
                const id = typeUnifier.uniqueTypeId(node);
                if (!visitedSet.has(id)) {
                    visitedSet.add(id);
                    visitCallback();
                    typeOrder.push(node);
                }
            }

            visitNativeType(node)
            {
                this._visitType(node, () => {});
            }

            visitEnumType(node)
            {
                node.baseType.visit(this);
            }

            visitArrayType(node)
            {
                this._visitType(node, () => super.visitArrayType(node));
            }

            visitVectorType(node)
            {
                this._visitType(node, () => {});
            }

            visitMatrixType(node)
            {
                this._visitType(node, () => {});
            }

            visitStructType(node)
            {
                // Don't need to recurse into elements for the forward declarations.
                this._visitType(node, () => {});
            }

            visitArrayRefType(node)
            {
                this._visitType(node, () => super.visitArrayRefType(node));
            }

            visitReferenceType(node)
            {
                this._visitType(node, () => super.visitReferenceType(node));
            }

            visitTypeRef(node)
            {
                super.visitTypeRef(node);
                node.type.visit(this);
            }
        }
        const typeOrderer = new TypeOrderer();
        for (let type of typesToDeclare)
            type.visit(typeOrderer);
        return typeOrder;
    }

    _sortTypeDeclarationsInTopologicalOrder(typesToDeclare)
    {
        const declarations = new Array();
        for (let type of typesToDeclare)
            declarations.push(type);

        let typeOrder = [];
        let visitedSet = new Set();
        const typeUnifier = this._typeUnifier;
        class TypeOrderer extends Visitor {
            _visitType(node, visitCallback)
            {
                const id = typeUnifier.uniqueTypeId(node);
                if (!visitedSet.has(id)) {
                    visitedSet.add(id);
                    visitCallback();
                    typeOrder.push(node);
                }
            }

            visitStructType(node)
            {
                this._visitType(node, () => super.visitStructType(node));
            }

            visitTypeRef(node)
            {
                super.visitTypeRef(node);
                node.type.visit(this);
            }

            visitArrayRefType(node)
            {
                this._visitType(node, () => super.visitArrayRefType(node));
            }

            visitReferenceType(node)
            {
                // We need an empty implementation to avoid recursion.
            }
        }
        const typeOrderer = new TypeOrderer();
        for (let type of typesToDeclare)
            type.visit(typeOrderer);

        const typeOrderMap = new Map();
        for (let i = 0; i < typeOrder.length; i++)
            typeOrderMap.set(typeOrder[i], i);

        return typeOrder;
    }

    // Also removes #pragma marks.
    _removeCommentsAndEmptyLines(src)
    {
        const singleLineCommentRegex = /(\/\/|\#pragma)(.*?)($|\n)/;
        const lines = src.split('\n')
            .map(line => line.replace(singleLineCommentRegex, '').trimEnd())
            .filter(line => line.length > 0);
        return lines.join('\n');
    }

    _metalSourceForArrayRefDefinition(arrayRefType)
    {
        let src = `struct ${this._typeUnifier.uniqueTypeId(arrayRefType)} {\n`;
        const fakePtrType = new PtrType(arrayRefType.origin, arrayRefType.addressSpace, arrayRefType.elementType);
        src += `    ${this._typeUnifier.uniqueTypeId(fakePtrType)} ptr;\n`
        src += "    uint32_t length;\n";
        src += "};";
        return src;
    }

    _metalSourceForArrayRefForwardDeclaration(arrayRefType)
    {
        return `struct ${this._typeUnifier.uniqueTypeId(arrayRefType)};`;
    }

    _metalSourceForStructForwardDeclaration(structType)
    {
        return `struct ${this._typeUnifier.uniqueTypeId(structType)};`;
    }

    _metalSourceForStructDefinition(structType)
    {
        const structTypeAttributes = this._allTypeAttributes.attributesForType(structType);
        let src = `struct ${this._typeUnifier.uniqueTypeId(structType)} {\n`;

        for (let [fieldName, field] of structType.fieldMap) {
            const mangledFieldName = structTypeAttributes.mangledFieldName(fieldName);
            src += `    ${this._typeUnifier.uniqueTypeId(field.type)} ${mangledFieldName}`;

            const annotations = [];
            if (structTypeAttributes.isVertexAttribute)
                annotations.push(`attribute(${field._semantic._index})`);
            if (structTypeAttributes.isVertexOutputOrFragmentInput && field._semantic._name === "SV_Position")
                annotations.push("position");
            if (structTypeAttributes.isFragmentOutput && field._semantic._name == "SV_Target")
                annotations.push(`color(${field._semantic._extraArguments[0]})`);
            if (annotations.length)
                src += ` [[${annotations.join(", ")}]]`;
            src += `; // ${fieldName} (${field.type}) \n`;
        }

        src += "};";

        return src;
    }

    _metalSourceForTypeDef(node)
    {
        const name = this._typeUnifier.uniqueTypeId(node);
        if (node.isArray)
            return `typedef ${this._typeUnifier.uniqueTypeId(node.elementType)} ${name}[${node.numElementsValue}];`;
        else if (node.isPtr)
            return `typedef ${node.addressSpace} ${this._typeUnifier.uniqueTypeId(node.elementType)} (*${name});`;
        else {
            class NativeTypeNameVisitor extends Visitor {
                visitNativeType(node)
                {
                    // FIXME: Also add samplers and textures here.
                    const nativeTypeNameMap = {
                        "void": "void",
                        "bool": "bool",
                        "uchar": "uint8_t",
                        "ushort": "uint16_t",
                        "uint": "uint32_t",
                        "char": "int8_t",
                        "short": "int16_t",
                        "int": "int32_t",
                        "half": "half",
                        "float": "float",
                        "atomic_int": "atomic_int",
                        "atomic_uint": "atomic_uint"
                    };

                    return nativeTypeNameMap[node.name];
                }

                visitVectorType(node)
                {
                    // Vector type names work slightly differently to native type names.
                    const elementTypeNameMap = {
                        "bool": "bool",
                        "char": "char",
                        "uchar": "uchar",
                        "short": "short",
                        "ushort": "ushort",
                        "int": "int",
                        "uint": "uint",
                        "half": "half",
                        "float": "float"
                    };

                    const elementTypeName = elementTypeNameMap[node.elementType.name];
                    if (!elementTypeName)
                        throw new Error(`${node.elementType.name} is not a supported vector element type`);

                    return `${elementTypeName}${node.numElementsValue}`;
                }

                visitMatrixType(node)
                {
                    const elementTypeNameMap = {
                        "half": "half",
                        "float": "float"
                    };

                    const elementTypeName = elementTypeNameMap[node.elementType.name];
                    if (!elementTypeName)
                        throw new Error(`${node.elementType.name} is not a supported matrix element type`);

                    return `${elementTypeName}${node.numRowsValue}x${node.numColumnsValue}`;
                }
            }
            const nativeName = node.visit(new NativeTypeNameVisitor());
            if (!nativeName)
                throw new Error(`Native type name not found for ${node}`);
            return `typedef ${nativeName} ${name};`;
        }
    }
}
