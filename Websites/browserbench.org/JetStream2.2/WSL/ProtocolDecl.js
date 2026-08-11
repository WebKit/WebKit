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

class ProtocolDecl extends Protocol {
    constructor(origin, name)
    {
        super(origin, name);
        this.extends = [];
        this._signatures = [];
        this._signatureMap = new Map();
        this._typeVariable = new TypeVariable(origin, name, null);
    }
    
    addExtends(protocol)
    {
        this.extends.push(protocol);
    }
    
    add(signature)
    {
        if (!(signature instanceof ProtocolFuncDecl))
            throw new Error("Signature isn't a ProtocolFuncDecl but a " + signature.constructor.name);
        
        signature.protocolDecl = this;
        this._signatures.push(signature);
        let overloads = this._signatureMap.get(signature.name);
        if (!overloads)
            this._signatureMap.set(signature.name, overloads = []);
        overloads.push(signature);
    }
    
    get signatures() { return this._signatures; }
    signaturesByName(name) { return this._signatureMap.get(name); }
    get typeVariable() { return this._typeVariable; }
    
    signaturesByNameWithTypeVariable(name, typeVariable)
    {
        let substitution = new Substitution([this.typeVariable], [typeVariable]);
        let result = this.signaturesByName(name);
        if (!result)
            return null;
        return result.map(signature => signature.visit(substitution));
    }
    
    inherits(otherProtocol)
    {
        if (!otherProtocol)
            return {result: true};
        
        if (otherProtocol instanceof ProtocolRef)
            otherProtocol = otherProtocol.protocolDecl;
        
        for (let otherSignature of otherProtocol.signatures) {
            let signatures = this.signaturesByName(otherSignature.name);
            if (!signatures)
                return {result: false, reason: "Protocol " + this.name + " does not have a function named " + otherSignature.name + " (looking at signature " + otherSignature + ")"};
            let overload = resolveOverloadImpl(
                signatures, [],
                otherSignature.parameterTypes,
                otherSignature.returnTypeForOverloadResolution);
            if (!overload.func)
                return {result: false, reason: "Did not find matching signature for " + otherSignature + " in " + this.name + (overload.failures.length ? " (tried: " + overload.failures.join("; ") + ")" : "")};
            let substitutedReturnType =
                overload.func.returnType.substituteToUnification(
                    overload.func.typeParameters, overload.unificationContext);
            if (!substitutedReturnType.equals(otherSignature.returnType))
                return {result: false, reason: "Return type mismatch between " + otherSignature.returnType + " and " + substitutedReturnType};
        }
        return {result: true};
    }
    
    hasHeir(type)
    {
        let substitution = new Substitution([this._typeVariable], [type]);
        let signatures = this.signatures;
        for (let originalSignature of signatures) {
            let signature = originalSignature.visit(substitution);
            let overload = resolveOverloadImpl(signature.possibleOverloads, signature.typeParameters, signature.parameterTypes, signature.returnTypeForOverloadResolution);
            if (!overload.func)
                return {result: false, reason: "Did not find matching signature for " + originalSignature + " (at " + originalSignature.origin.originString + ") with type " + type + (overload.failures.length ? " (tried: " + overload.failures.join("; ") + ")" : "")};
            
            let substitutedReturnType = overload.func.returnType.substituteToUnification(
                overload.func.typeParameters, overload.unificationContext);
            if (!substitutedReturnType.equals(signature.returnType))
                return {result: false, reason: "At signature " + originalSignature + " (at " + originalSignature.origin.originString + "): return type mismatch between " + signature.returnType + " and " + substitutedReturnType + " in found function " + overload.func.toDeclString()};
        }
        return {result: true};
    }
    
    toString()
    {
        return "protocol " + this.name + " { " + this.signatures.join("; ") + "; }";
    }
}


