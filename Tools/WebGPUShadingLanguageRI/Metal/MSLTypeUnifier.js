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


class MSLTypeUnifier extends Visitor {

    constructor()
    {
        super();

        this._typeNameMangler = new MSLNameMangler('T');
        this._allTypes = new Set();
    }

    get allTypes()
    {
        return this._allTypes;
    }

    uniqueTypeId(type)
    {
        const result = type.visit(this);
        if (!result)
            throw new Error(`${type} has no unique type ID.`);
        return result;
    }

    visitTypeRef(node)
    {
        if ((node.typeArguments && node.typeArguments.length) || !node.name) {
            return node.type.visit(this);
        }

        if (!this._allTypes.has(node.type))
            node.type.visit(this);

        const baseType = typeOf(node);

        if (baseType instanceof NativeType || baseType instanceof VectorType || baseType instanceof MatrixType || baseType instanceof EnumType || baseType instanceof ArrayType)
            return baseType.visit(this);

        return this._typeNameMangler.mangle(node.name);
    }

    visitStructType(node)
    {
        this._allTypes.add(node);
        for (let field of node.fields)
            field.visit(this);
        node._typeID = this._typeNameMangler.mangle(node.name);
        return node._typeID;
    }

    visitEnumType(node)
    {
        this._allTypes.add(node);
        node._typeID = node.baseType.visit(this);
        return node._typeID;
    }

    visitNativeType(node)
    {
        this._allTypes.add(node);
        node._typeID = this._typeNameMangler.mangle(node.name);
        return node._typeID;
    }

    visitPtrType(node)
    {
        this._allTypes.add(node);
        node.elementType.visit(this);
        node._typeID = this._typeNameMangler.mangle(`${node.elementType.visit(this)}* ${node.addressSpace}`);
        return node._typeID;
    }

    visitArrayType(node)
    {
        this._allTypes.add(node);
        node.elementType.visit(this);
        node._typeID = this._typeNameMangler.mangle(`${node.elementType.visit(this)}[${node.numElements}]`);
        return node._typeID;
    }

    visitArrayRefType(node)
    {
        this._allTypes.add(node);
        // The name mangler is used here because array refs are "user-defined" types in the sense
        // that they will need to be defined as structs in the Metal output.
        node._typeID = this._typeNameMangler.mangle(`${node.elementType.visit(this)}[] ${node.addressSpace}`);
        return node._typeID;
    }

    visitVectorType(node)
    {
        this._allTypes.add(node);
        node._typeID = this._typeNameMangler.mangle(node.toString());
        return node._typeID;
    }

    visitMatrixType(node)
    {
        this._allTypes.add(node);
        node._typeID = this._typeNameMangler.mangle(node.toString());
        return node._typeID;
    }

    visitMakeArrayRefExpression(node)
    {
        super.visitMakeArrayRefExpression(node);
        node._typeID = node.type.visit(this);
        // When the array ref is declared in MSL it will contained a pointer to the element type. However, the original
        // WHLSL doesn't need to contain the type "pointer to the element type", so we ensure that the type is present
        // here. The logic isn't needed for ConvertPtrToArrayRefExpression because the type is necesssarily present there.
        new PtrType(node.origin, node.addressSpace ? node.addressSpace : "thread", typeOf(node).elementType).visit(this);
        return node._typeID;
    }

    visitGenericLiteralType(node)
    {
        node._typeID = node.type.visit(this);
        return node._typeID;
    }

    typesThatNeedDeclaration()
    {
        const declSet = new Set();
        const nameSet = new Set();
        for (let type of this._allTypes) {
            const name = type.visit(this);
            if (!nameSet.has(name)) {
                declSet.add(type);
                nameSet.add(name);
            }
        }
        return declSet;
    }
}