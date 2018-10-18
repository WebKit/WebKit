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

// This finds type and value expressions, but skips type and value declarations.
class ExpressionFinder extends Visitor {
    constructor(callback)
    {
        super();
        this._callback = callback;
    }
    
    visitFunc(node)
    {
        this._callback(node.returnType);
        for (let typeParameter of node.typeParameters)
            typeParameter.visit(this);
        for (let parameter of node.parameters)
            parameter.visit(this);
    }
    
    visitFuncParameter(node)
    {
        this._callback(node.type);
    }
    
    visitConstexprTypeParameter(node)
    {
        this._callback(node.type);
    }
    
    visitAssignment(node)
    {
        this._callback(node);
    }
    
    visitReadModifyWriteExpression(node)
    {
        this._callback(node);
    }
    
    visitIdentityExpression(node)
    {
        this._callback(node);
    }
    
    visitCallExpression(node)
    {
        this._callback(node);
    }
    
    visitReturn(node)
    {
        if (node.value)
            this._callback(node.value);
    }
    
    visitWhileLoop(node)
    {
        this._callback(node.conditional);
        node.body.visit(this);
    }
    
    visitDoWhileLoop(node)
    {
        node.body.visit(this);
        this._callback(node.conditional);
    }
    
    visitIfStatement(node)
    {
        this._callback(node.conditional);
        node.body.visit(this);
        if (node.elseBody)
            node.elseBody.visit(this);
    }
    
    visitForLoop(node)
    {
        // Initialization is a statement, not an expression. If it's an assignment or variableDecl, we'll
        // catch it and redirect to the callback.
        if (node.initialization)
            node.initialization.visit(this);
        
        if (node.condition)
            this._callback(node.condition);
        if (node.increment)
            this._callback(node.increment);
        
        node.body.visit(this);
    }
    
    visitVariableDecl(node)
    {
        this._callback(node.type);
        if (node.initializer)
            this._callback(node.initializer);
    }
}

