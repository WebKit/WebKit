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

class NullType extends Type {
    constructor(origin)
    {
        super();
        this._origin = origin;
    }
    
    get origin() { return this._origin; }
    
    // For now we answer false for isPtr and isArrayRef because that happens to make all of the logic
    // work. But, it's strange, since as a bottom type it really is the case that this is both isPtr and
    // isArrayRef.
    
    get isPrimitive() { return true; }
    get isUnifiable() { return true; }
    get isLiteral() { return true; }
    
    typeVariableUnify(unificationContext, other)
    {
        if (!(other instanceof Type))
            return false;
        
        return this._typeVariableUnifyImpl(unificationContext, other);
    }
    
    unifyImpl(unificationContext, other)
    {
        return this.typeVariableUnify(unificationContext, other);
    }
    
    verifyAsArgument(unificationContext)
    {
        let realThis = unificationContext.find(this);
        if (realThis.isPtr || realThis.isArrayRef)
            return {result: true};
        return {result: false, reason: "Null cannot be used with non-pointer type " + realThis};
    }
    
    verifyAsParameter(unificationContext)
    {
        throw new Error("NullType should never be used as a type parameter");
    }
    
    commitUnification(unificationContext)
    {
        this.type = unificationContext.find(this);
    }
    
    toString()
    {
        return "nullType";
    }
}

