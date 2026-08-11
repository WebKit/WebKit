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

function createLiteralType(config)
{
    class GenericLiteralType extends Type {
        constructor(origin, value)
        {
            super();
            this._origin = origin;
            this._value = value;
            this.preferredType = new TypeRef(origin, config.preferredTypeName, []);
        }
        
        get origin() { return this._origin; }
        get value() { return this._value; }
        
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
        
        prepareToVerify(unificationContext)
        {
            let realThis = unificationContext.find(this);
            if (realThis instanceof TypeVariable || realThis.isLiteral) {
                return () => {
                    if (realThis.unify(unificationContext, this.preferredType))
                        return {result: true};
                    return {result: false, reason: "Type mismatch between " + unificationContext.find(realThis) + " and " + this.preferredType};
                };
            }
        }
        
        verifyAsArgument(unificationContext)
        {
            return config.verifyAsArgument.call(this, unificationContext);
        }
        
        verifyAsParameter(unificationContext)
        {
            throw new Error("GenericLiteralType should never be used as a type parameter");
        }
        
        conversionCost(unificationContext)
        {
            let realThis = unificationContext.find(this);
            if (realThis.equals(this.preferredType))
                return 0;
            return 1;
        }
        
        commitUnification(unificationContext)
        {
            this.type = unificationContext.find(this).visit(new AutoWrapper());
        }
        
        toString()
        {
            return config.preferredTypeName + "LiteralType<" + this.value + ">";
        }
    }
    
    return GenericLiteralType;
}

