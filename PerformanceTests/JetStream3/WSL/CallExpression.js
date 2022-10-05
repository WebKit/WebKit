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
    
    static resolve(origin, possibleOverloads, typeParametersInScope, name, typeArguments, argumentList, argumentTypes, returnType)
    {
        let call = new CallExpression(origin, name, typeArguments, argumentList);
        call.argumentTypes = argumentTypes.map(argument => argument.visit(new AutoWrapper()));
        call.possibleOverloads = possibleOverloads;
        if (returnType)
            call.setCastData(returnType);
        return {call, resultType: call.resolve(possibleOverloads, typeParametersInScope, typeArguments)};
    }
    
    resolve(possibleOverloads, typeParametersInScope, typeArguments)
    {
        if (!possibleOverloads)
            throw new WTypeError(this.origin.originString, "Did not find any functions named " + this.name);
        
        let overload = null;
        let failures = [];
        for (let typeParameter of typeParametersInScope) {
            if (!(typeParameter instanceof TypeVariable))
                continue;
            if (!typeParameter.protocol)
                continue;
            let signatures =
                typeParameter.protocol.protocolDecl.signaturesByNameWithTypeVariable(this.name, typeParameter);
            if (!signatures)
                continue;
            overload = resolveOverloadImpl(signatures, this.typeArguments, this.argumentTypes, this.returnType);
            if (overload.func)
                break;
            failures.push(...overload.failures);
            overload = null;
        }
        if (!overload) {
            overload = resolveOverloadImpl(
                possibleOverloads, this.typeArguments, this.argumentTypes, this.returnType);
            if (!overload.func) {
                failures.push(...overload.failures);
                let message = "Did not find function named " + this.name + " for call with ";
                if (this.typeArguments.length)
                    message += "type arguments <" + this.typeArguments + "> and ";
                message += "argument types (" + this.argumentTypes + ")";
                if (this.returnType)
                    message +=" and return type " + this.returnType;
                if (failures.length)
                    message += ", but considered:\n" + failures.join("\n")
                throw new WTypeError(this.origin.originString, message);
            }
        }
        for (let i = 0; i < typeArguments.length; ++i) {
            let typeArgumentType = typeArguments[i];
            let typeParameter = overload.func.typeParameters[i];
            if (!(typeParameter instanceof ConstexprTypeParameter))
                continue;
            if (!typeParameter.type.equalsWithCommit(typeArgumentType))
                throw new Error("At " + this.origin.originString + " constexpr type argument and parameter types not equal: argument = " + typeArgumentType + ", parameter = " + typeParameter.type);
        }
        for (let i = 0; i < this.argumentTypes.length; ++i) {
            let argumentType = this.argumentTypes[i];
            let parameterType = overload.func.parameters[i].type.substituteToUnification(
                overload.func.typeParameters, overload.unificationContext);
            let result = argumentType.equalsWithCommit(parameterType);
            if (!result)
                throw new Error("At " + this.origin.originString + " argument and parameter types not equal after type argument substitution: argument = " + argumentType + ", parameter = " + parameterType);
        }
        return this.resolveToOverload(overload);
    }
    
    resolveToOverload(overload)
    {
        this.func = overload.func;
        this.actualTypeArguments = overload.typeArguments.map(typeArgument => typeArgument instanceof Type ? typeArgument.visit(new AutoWrapper()) : typeArgument);
        this.instantiatedActualTypeArguments = this.actualTypeArguments;
        let result = overload.func.returnType.substituteToUnification(
            overload.func.typeParameters, overload.unificationContext);
        if (!result)
            throw new Error("Null return type");
        result = result.visit(new AutoWrapper());
        this.resultType = result;
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

