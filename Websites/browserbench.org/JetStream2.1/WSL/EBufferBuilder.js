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

class EBufferBuilder extends Visitor {
    constructor(program)
    {
        super();
        this._program = program;
    }
    
    _createEPtr(type)
    {
        if (type.size == null)
            throw new Error("Type does not have size: " + type);
        let buffer = new EBuffer(type.size);
        if (!type.populateDefaultValue)
            throw new Error("Cannot populateDefaultValue with: " + type);
        type.populateDefaultValue(buffer, 0);
        return new EPtr(buffer, 0);
    }
    
    _createEPtrForNode(node)
    {
        if (!node.type)
            throw new Error("node has no type: " + node);
        node.ePtr = this._createEPtr(node.type);
    }
    
    visitFuncParameter(node)
    {
        this._createEPtrForNode(node);
    }
    
    visitVariableDecl(node)
    {
        this._createEPtrForNode(node);
        if (node.initializer)
            node.initializer.visit(this);
    }
    
    visitFuncDef(node)
    {
        node.returnEPtr = this._createEPtr(node.returnType);
        super.visitFuncDef(node);
    }
    
    visitFunctionLikeBlock(node)
    {
        node.returnEPtr = this._createEPtr(node.returnType);
        super.visitFunctionLikeBlock(node);
    }
    
    visitCallExpression(node)
    {
        node.resultEPtr = this._createEPtr(node.resultType);
        super.visitCallExpression(node);
    }
    
    visitMakePtrExpression(node)
    {
        node.ePtr = EPtr.box();
        super.visitMakePtrExpression(node);
    }
    
    visitGenericLiteral(node)
    {
        node.ePtr = EPtr.box();
    }
    
    visitNullLiteral(node)
    {
        node.ePtr = EPtr.box();
    }
    
    visitBoolLiteral(node)
    {
        node.ePtr = EPtr.box();
    }
    
    visitEnumLiteral(node)
    {
        node.ePtr = EPtr.box();
    }
    
    visitLogicalNot(node)
    {
        node.ePtr = EPtr.box();
        super.visitLogicalNot(node);
    }
    
    visitLogicalExpression(node)
    {
        node.ePtr = EPtr.box();
        super.visitLogicalExpression(node);
    }
    
    visitAnonymousVariable(node)
    {
        this._createEPtrForNode(node);
    }
    
    visitMakeArrayRefExpression(node)
    {
        node.ePtr = EPtr.box();
        super.visitMakeArrayRefExpression(node);
    }
    
    visitConvertPtrToArrayRefExpression(node)
    {
        node.ePtr = EPtr.box();
        super.visitConvertPtrToArrayRefExpression(node);
    }
}

