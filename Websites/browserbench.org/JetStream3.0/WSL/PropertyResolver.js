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

class PropertyResolver extends Visitor {
    _visitRValuesWithinLValue(node)
    {
        let visit = node => node.visit(this);
        
        class RValueFinder {
            visitDotExpression(node)
            {
                node.struct.visit(this);
            }
            
            visitIndexExpression(node)
            {
                node.array.visit(this);
                visit(node.index);
            }
            
            visitVariableRef(node)
            {
            }
            
            visitDereferenceExpression(node)
            {
                visit(node.ptr);
            }
            
            visitIdentityExpression(node)
            {
                node.target.visit(this);
            }
            
            visitMakeArrayRefExpression(node)
            {
                visit(node.lValue);
            }
        }
        
        node.visit(new RValueFinder());
    }
    
    _visitPropertyAccess(node)
    {
        let newNode = node.visit(new NormalUsePropertyResolver());
        newNode.visit(this);
        node.become(newNode);
    }
    
    visitDotExpression(node)
    {
        this._visitPropertyAccess(node);
    }
    
    visitIndexExpression(node)
    {
        this._visitPropertyAccess(node);
    }
    
    _handleReadModifyWrite(node)
    {
        let type = node.oldValueVar.type;
        if (!type)
            throw new Error("Null type");
        let simpleLHS = node.lValue.visit(new NormalUsePropertyResolver());
        if (simpleLHS.isLValue) {
            if (!simpleLHS.addressSpace)
                throw new Error(node.origin.originString + ": LHS without address space: " + simpleLHS);
            let ptrType = new PtrType(node.origin, simpleLHS.addressSpace, type);
            let ptrVar = new AnonymousVariable(node.origin, ptrType);
            node.become(new CommaExpression(node.origin, [
                node.oldValueVar, node.newValueVar, ptrVar,
                new Assignment(
                    node.origin, VariableRef.wrap(ptrVar),
                    new MakePtrExpression(node.origin, simpleLHS), ptrType),
                new Assignment(
                    node.origin, node.oldValueRef(),
                    new DereferenceExpression(
                        node.origin, VariableRef.wrap(ptrVar), type, simpleLHS.addressSpace),
                    type),
                new Assignment(node.origin, node.newValueRef(), node.newValueExp, type),
                new Assignment(
                    node.origin,
                    new DereferenceExpression(
                        node.origin, VariableRef.wrap(ptrVar), type, simpleLHS.addressSpace),
                    node.newValueRef(), type),
                node.resultExp
            ]));
            return;
        }
        
        let result = new ReadModifyWriteExpression(node.origin, node.lValue.base, node.lValue.baseType);
        result.newValueExp = new CommaExpression(node.origin, [
            node.oldValueVar, node.newValueVar,
            new Assignment(node.origin, node.oldValueRef(), node.lValue.emitGet(result.oldValueRef()), type),
            new Assignment(node.origin, node.newValueRef(), node.newValueExp, type),
            node.lValue.emitSet(result.oldValueRef(), node.newValueRef())
        ]);
        result.resultExp = node.newValueRef();
        this._handleReadModifyWrite(result);
        node.become(result);
    }
    
    visitReadModifyWriteExpression(node)
    {
        node.newValueExp.visit(this);
        node.resultExp.visit(this);
        this._visitRValuesWithinLValue(node.lValue);
        this._handleReadModifyWrite(node);
    }
    
    visitAssignment(node)
    {
        this._visitRValuesWithinLValue(node.lhs);
        node.rhs.visit(this);
        
        let simpleLHS = node.lhs.visit(new NormalUsePropertyResolver());
        if (simpleLHS.isLValue) {
            node.lhs.become(simpleLHS);
            return;
        }
        
        if (!(node.lhs instanceof PropertyAccessExpression))
            throw new Error("Unexpected lhs type: " + node.lhs.constructor.name);
        
        let result = new ReadModifyWriteExpression(node.origin, node.lhs.base, node.lhs.baseType);
        let resultVar = new AnonymousVariable(node.origin, node.type);
        result.newValueExp = new CommaExpression(node.origin, [
            resultVar,
            new Assignment(node.origin, VariableRef.wrap(resultVar), node.rhs, node.type),
            node.lhs.emitSet(result.oldValueRef(), VariableRef.wrap(resultVar))
        ]);
        result.resultExp = VariableRef.wrap(resultVar);
        this._handleReadModifyWrite(result);
        node.become(result);
    }
    
    visitMakePtrExpression(node)
    {
        super.visitMakePtrExpression(node);
        if (!node.lValue.isLValue)
            throw new WTypeError(node.origin.originString, "Not an lvalue: " + node.lValue);
    }
    
    visitMakeArrayRefExpression(node)
    {
        super.visitMakeArrayRefExpression(node);
        if (!node.lValue.isLValue)
            throw new WTypeError(node.origin.originString, "Not an lvalue: " + node.lValue);
    }
}

