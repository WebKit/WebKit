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

class CallExpression extends Expression {
    constructor(origin, name, typeArguments, argumentList)
    {
        super(origin);
        this._name = name;
        this._typeArguments = typeArguments;
        this._argumentList = argumentList;
        this.func = null;
        this._isCast = false;
        this._returnType = null;
    }
    
    get name() { return this._name; }
    get typeArguments() { return this._typeArguments; }
    get argumentList() { return this._argumentList; }
    get isCast() { return this._isCast; }
    get returnType() { return this._returnType; }
    
    resolve(overload)
    {
        this.func = overload.func;
        this.actualTypeArguments = overload.typeArguments.map(typeArgument => typeArgument instanceof Type ? typeArgument.visit(new AutoWrapper()) : typeArgument);
        let result = overload.func.returnType.substituteToUnification(
            overload.func.typeParameters, overload.unificationContext);
        this.resultType = result.visit(new AutoWrapper());
        if (!result)
            throw new Error("Null return type");
        return result;
    }
    
    becomeCast(returnType)
    {
        this._returnType = new TypeRef(this.origin, this.name, this._typeArguments);
        this._returnType.type = returnType;
        this._name = "operator cast";
        this._isCast = true;
        this._typeArguments = [];
    }
    
    setCastData(returnType)
    {
        this._returnType = returnType;
        this._isCast = true;
    }
    
    toString()
    {
        return (this.isCast ? "operator " + this.returnType : this.name) +
            "<" + this.typeArguments + ">" +
            (this.actualTypeArguments ? "<<" + this.actualTypeArguments + ">>" : "") +
            "(" + this.argumentList + ")";
    }
}

