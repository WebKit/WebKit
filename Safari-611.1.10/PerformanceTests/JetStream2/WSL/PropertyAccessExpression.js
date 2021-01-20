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

class PropertyAccessExpression extends Expression {
    constructor(origin, base)
    {
        super(origin);
        this.base = base;
        this._isLValue = null; // We use null to indicate that we don't know yet.
        this.addressSpace = null;
        this._notLValueReason = null;
    }
    
    get resultType() { return this.resultTypeForGet ? this.resultTypeForGet : this.resultTypeForAnd; }
    get isLValue() { return this._isLValue; }
    set isLValue(value) { this._isLValue = value; }
    get notLValueReason() { return this._notLValueReason; }
    set notLValueReason(value) { this._notLValueReason = value; }
    
    rewriteAfterCloning()
    {
        // At this point, node.base.isLValue will only return true if it's possible to get a pointer to it,
        // since we will no longer see any DotExpressions or IndexExpressions. Also, node is a newly created
        // node and everything beneath it is newly created, so we can edit it in-place.
        
        if ((this.base.isLValue || this.baseType.isRef) && this.callForAnd)
            return new DereferenceExpression(this.origin, this.callForAnd, this.resultType, this.callForAnd.resultType.addressSpace);
        
        if (this.callForGet)
            return this.callForGet;
        
        if (!this.callForAnd)
            throw new Error(this.origin.originString + ": Property access without getter and ander: " + this);
        
        let anonVar = new AnonymousVariable(this.origin, this.baseType);
        this.callForAnd.argumentList[0] = this.baseType.unifyNode.argumentForAndOverload(this.origin, VariableRef.wrap(anonVar));
        return new CommaExpression(this.origin, [
            anonVar,
            new Assignment(this.origin, VariableRef.wrap(anonVar), this.base, this.baseType),
            new DereferenceExpression(this.origin, this.callForAnd, this.resultType, this.callForAnd.resultType.addressSpace)
        ]);
    }
    
    updateCalls()
    {
        if (this.callForGet)
            this.callForGet.argumentList[0] = this.base;
        if (this.callForAnd)
            this.callForAnd.argumentList[0] = this.baseType.argumentForAndOverload(this.origin, this.base);
        if (this.callForSet)
            this.callForSet.argumentList[0] = this.base;
    }
    
    emitGet(base)
    {
        let result = this.visit(new Rewriter());
        result.base = base;
        result.updateCalls();
        return result.rewriteAfterCloning();
    }
    
    emitSet(base, value)
    {
        let result = this.visit(new Rewriter());
        if (!result.callForSet)
            throw new WTypeError(this.origin.originString, "Cannot emit set because: " + this.errorForSet.typeErrorMessage);
        result.base = base;
        result.updateCalls();
        result.callForSet.argumentList[result.callForSet.argumentList.length - 1] = value;
        return result.callForSet;
    }
}

