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

class StructLayoutBuilder extends Visitor {
    constructor()
    {
        super();
        this._offset = 0;
    }
    
    visitReferenceType(node)
    {
    }
    
    visitStructType(node)
    {
        if (node.size != null)
            return;
        if (node.typeParameters.length)
            throw new Error("Cannot do layout for generic type: " + node);
        let oldOffset = this._offset;
        this._offset = 0;
        super.visitStructType(node);
        node.size = this._offset;
        this._offset += oldOffset;
    }
    
    get offset() { return this._offset; }
    
    visitField(node)
    {
        super.visitField(node);
        node.offset = this._offset;
        let size = node.type.size;
        if (size == null)
            throw new Error("Type does not have size: " + node.type);
        this._offset += size;
    }
    
    visitNativeFuncInstance(node)
    {
        super.visitNativeFuncInstance(node);
        node.func.didLayoutStructsInImplementationData(node.implementationData);
    }
    
    visitTypeRef(node)
    {
        super.visitTypeRef(node);
        node.type.visit(this);
    }
    
    visitCallExpression(node)
    {
        for (let argument of node.argumentList)
            Node.visit(argument, this);
        let handleTypeArguments = actualTypeArguments => {
            if (actualTypeArguments) {
                for (let argument of actualTypeArguments)
                    argument.visit(this);
            }
        };
        handleTypeArguments(node.instantiatedActualTypeArguments);
        Node.visit(node.nativeFuncInstance, this);
        Node.visit(node.resultType, this);
    }
}

