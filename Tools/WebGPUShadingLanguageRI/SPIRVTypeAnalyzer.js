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

class SPIRVTypeAnalyzer extends Visitor {
    constructor(program, typeMap, currentId)
    {
        super();
        this._program = program;
        this._typeMap = typeMap;
        this._currentId = currentId;
        this._stack = [];
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

    get stack()
    {
        return this._stack;
    }

    visitTypeRef(node)
    {
        node.type.visit(this);
    }

    _encounterType(id)
    {
        if (this.stack.length > 0)
            this.stack[this.stack.length - 1].push(id);
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
        if (!this.typeMap.has(node))
            this.typeMap.set(node, this._currentId++);
        let id = this.typeMap.get(node);
        this._encounterType(id);
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

    // FIXME: Using toString() in these functions is a hack. Instead, we should implement a proper type deduper.
    visitArrayType(node)
    {
        let id;
        if (this.typeMap.has(node.toString())) {
            id = this.typeMap.get(node);
            if (typeof id == "object")
                id = id.id;
        } else {
            let fieldType = [];
            this.stack.push(fieldType);
            node.elementType.visit(this);
            this.stack.pop();
            if (fieldType.length != 1)
                throw new Error("Arrays can only have one element type!");
            id = this._currentId++;
            node.numElements.visit(this);
            this.typeMap.set(node.toString(), {id: id, elementType: fieldType[0], numElements: node.numElements.value});
        }
        this._encounterType(id);
    }

    visitStructType(node)
    {
        let id;
        if (this.typeMap.has(node.toString())) {
            id = this.typeMap.get(node.toString());
            if (typeof id == "object")
                id = id.id;
        } else {
            let fieldTypes = [];
            this.stack.push(fieldTypes);
            for (let field of node.fields)
                field.visit(this);
            this.stack.pop();
            id = this._currentId++;
            this.typeMap.set(node.toString(), {id: id, fieldTypes: fieldTypes});
        }
        this._encounterType(id);
    }
}
