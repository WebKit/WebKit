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

// This is a combined LHS/RHS evaluator that passes around EPtr's to everything.
class Evaluator extends Visitor {
    constructor(program)
    {
        super();
        this._program = program;
    }
    
    runBody(block)
    {
        try {
            block.visit(this);
            // FIXME: We should have a check that there is no way to drop out of a function without
            // returning unless the function returns void.
            return null;
        } catch (e) {
            if (e == BreakException || e == ContinueException)
                throw new Error("Should not see break/continue at function scope");
            if (e instanceof ReturnException)
                return e.value;
            throw e;
        }
    }
    
    visitFunctionLikeBlock(node)
    {
        for (let i = 0; i < node.argumentList.length; ++i) {
            node.parameters[i].ePtr.copyFrom(
                node.argumentList[i].visit(this),
                node.parameters[i].type.size);
        }
        return this.runBody(node.body);
    }
    
    visitReturn(node)
    {
        throw new ReturnException(node.value ? node.value.visit(this) : null);
    }
    
    visitVariableDecl(node)
    {
        if (!node.ePtr.buffer)
            throw new Error("eptr without buffer in " + node);
        node.type.populateDefaultValue(node.ePtr.buffer, node.ePtr.offset);
        if (node.initializer)
            node.ePtr.copyFrom(node.initializer.visit(this), node.type.size);
    }
    
    visitAssignment(node)
    {
        let result = node.lhs.visit(this);
        result.copyFrom(node.rhs.visit(this), node.type.size);
        return result;
    }
    
    _dereference(ptr, type)
    {
        let size = type.size;
        let result = new EPtr(new EBuffer(size), 0);
        result.copyFrom(ptr.loadValue(), size);
        return result;
    }
    
    visitDereferenceExpression(node)
    {
        return this._dereference(node.ptr.visit(this), node.type);
    }
    
    visitCommaExpression(node)
    {
        let result;
        for (let expression of node.list)
            result = expression.visit(this);
        return result;
    }
    
    visitVariableRef(node)
    {
        return node.variable.ePtr;
    }
    
    visitIntLiteral(node)
    {
        return EPtr.box(node.value);
    }
    
    visitCallExpression(node)
    {
        // We evaluate inlined ASTs, so this can only be a native call.
        let callArguments = node.argumentList.map(argument => {
            let result = argument.visit(this);
            if (!result)
                throw new Error("Null result from " + argument);
            return result;
        });
        return node.func.implementation(callArguments, node);
    }
}

