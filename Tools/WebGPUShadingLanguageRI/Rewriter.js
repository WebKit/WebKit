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

class Rewriter {
    constructor()
    {
        this._mapping = new Map();
    }
    
    _mapNode(oldItem, newItem)
    {
        this._mapping.set(oldItem, newItem);
        return newItem;
    }
    
    _getMapping(oldItem)
    {
        let result = this._mapping.get(oldItem);
        if (result)
            return result;
        return oldItem;
    }
    
    visitFunc(node)
    {
        return new Func(
            node.name, node.returnType.visit(this),
            node.typeParameters.map(typeParameter => typeParameter.visit(this)),
            node.parameters.map(parameter => parameter.visit(this)));
    }
    
    visitFuncParameter(node)
    {
        let result = new FuncParameter(node.origin, node.name, node.type.visit(this));
        this._mapNode(node, result);
        result.lValue = node.lValue;
        return result;
    }
    
    visitBlock(node)
    {
        let result = new Block(node.origin);
        for (let statement of node.statements)
            result.add(statement.visit(this));
        return result;
    }
    
    visitCommaExpression(node)
    {
        return new CommaExpression(node.origin, node.list.map(expression => {
            let result = expression.visit(this);
            if (!result)
                throw new Error("Null result from " + expression);
            return result;
        }));
    }
    
    visitProtocolRef(node)
    {
        return node;
    }
    
    visitTypeRef(node)
    {
        let result = new TypeRef(node.origin, node.name, node.typeArguments.map(typeArgument => typeArgument.visit(this)));
        result.type = node.type;
        return result;
    }
    
    visitTypeVariable(node)
    {
        return node;
    }
    
    visitConstexprTypeParameter(node)
    {
        return new ConstexprTypeParameter(node.origin, node.name, node.type.visit(this));
    }
    
    visitField(node)
    {
        return new Field(node.origin, node.name, node.type.visit(this));
    }
    
    visitReferenceType(node)
    {
        return new node.constructor(node.rogiin, node.addressSpace, node.elementType.visit(this));
    }
    
    visitPtrType(node)
    {
        return this.visitReferenceType(node);
    }
    
    visitArrayRefType(node)
    {
        return this.visitReferenceType(node);
    }
    
    visitArrayType(node)
    {
        return new ArrayType(node.origin, node.elementType.visit(this));
    }
    
    visitAssignment(node)
    {
        return new Assignment(node.origin, node.lhs.visit(this), node.rhs.visit(this));
    }
    
    visitVariableRef(node)
    {
        node.variable = this._getMapping(node.variable);
        return node;
    }
    
    visitReturn(node)
    {
        return new Return(node.origin, node.value ? node.value.visit(this) : null);
    }
    
    visitIntLiteral(node)
    {
        return node;
    }

    visitCallExpression(node)
    {
        let result = new CallExpression(
            node.origin, node.name,
            node.typeArguments.map(typeArgument => typeArgument.visit(this)),
            node.argumentList.map(argument => argument.visit(this)));
        let actualTypeArguments = node.actualTypeArguments;
        if (actualTypeArguments) {
            result.actualTypeArguments =
                actualTypeArguments.map(actualTypeArgument => actualTypeArgument.visit(this));
        }
        result.func = node.func;
        return result;
    }
    
    visitFunctionLikeBlock(node)
    {
        return new FunctionLikeBlock(
            node.origin,
            node.argumentList.map(argument => argument.visit(this)),
            node.parameters.map(parameter => parameter.visit(this)),
            node.body.visit(this));
    }
}

