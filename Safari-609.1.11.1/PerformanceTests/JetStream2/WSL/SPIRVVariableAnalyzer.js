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

class SPIRVPrimitiveVariableAnalyzer extends Visitor {
    constructor(program, typeMap, currentId, currentLocation, startName)
    {
        super();
        this._program = program;
        this._typeMap = typeMap;
        this._currentId = currentId;
        this._currentLocation = currentLocation;
        this._nameComponents = [];
        if (startName)
            this._nameComponents.push(startName);
        this._result = [];
    }

    get program()
    {
        this._program;
    }

    get typeMap()
    {
        return this._typeMap;
    }

    get currentId()
    {
        return this._currentId;
    }

    get currentLocation()
    {
        return this._currentLocation;
    }

    get nameComponents()
    {
        return this._nameComponents;
    }

    get result()
    {
        return this._result;
    }

    visitTypeRef(node)
    {
        node.type.visit(this);
    }

    visitNullType(node)
    {
        super.visit(this);
    }

    visitGenericLiteralType(node)
    {
        node.type.visit(this);
    }

    visitNativeType(node)
    {
        this.result.push({ name: this.nameComponents.join(""), id: this._currentId++, type: this.typeMap.get(node), location: this._currentLocation++ });
    }

    visitEnumType(node)
    {
        super.visit(this);
    }

    visitPtrType(node)
    {
        // Intentionally blank
    }

    visitArrayRefType(node)
    {
        this.visitNativeType(program.intrinsics.uint32);
        this.visitNativeType(program.intrinsics.uint32);
    }

    visitArrayType(node)
    {
        for (let i = 0; i < node.numElements.value; ++i) {
            this.nameComponents.push("[");
            this.nameComponents.push(i.toString());
            this.nameComponents.push("]");
            node.elementType.visit(this);
            this.nameComponents.pop();
            this.nameComponents.pop();
            this.nameComponents.pop();
        }
    }

    visitStructType(node)
    {
        let builtInStructs = [
            "struct vec2<> { int32 x; int32 y }",
            "struct vec2<> { uint32 x; uint32 y; }",
            "struct vec2<> { float32 x; float32 y; }",
            "struct vec2<> { float64 x; float64 y; }",
            "struct vec3<> { int32 x; int32 y; int32 z; }",
            "struct vec3<> { uint32 x; uint32 y; uint32 z; }",
            "struct vec3<> { float32 x; float32 y; float32 z; }",
            "struct vec3<> { float64 x; float64 y; float64 z; }",
            "struct vec4<> { int32 x; int32 y; int32 z; int32 w; }",
            "struct vec4<> { uint32 x; uint32 y; uint32 z; uint32 w; }",
            "struct vec4<> { float32 x; float32 y; float32 z; float32 w; }",
            "struct vec4<> { float64 x; float64 y; float64 z; float64 w; }"
        ];
        for (let builtInStruct of builtInStructs) {
            if (node.toString() == builtInStruct) {
                this.result.push({ name: this.nameComponents.join(""), id: this._currentId++, type: this.typeMap.get(node.toString()).id, location: this._currentLocation++  });
                return;
            }
        }
        for (let field of node.fields) {
            let dotPrefix = this.nameComponents.length > 0;
            if (dotPrefix)
                this.nameComponents.push(".");
            this.nameComponents.push(field.name);
            field.visit(this);
            this.nameComponents.pop();
            if (dotPrefix)
                this.nameComponents.pop();
        }
    }
}
