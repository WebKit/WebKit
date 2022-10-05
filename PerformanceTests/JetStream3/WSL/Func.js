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

class Func extends Node {
    constructor(origin, name, returnType, typeParameters, parameters, isCast, shaderType)
    {
        if (!(origin instanceof LexerToken))
            throw new Error("Bad origin: " + origin);
        for (let parameter of parameters) {
            if (!parameter)
                throw new Error("Null parameter");
            if (!parameter.type)
                throw new Error("Null parameter type");
        }
        super();
        this._origin = origin;
        this._name = name;
        this._returnType = returnType;
        this._typeParameters = typeParameters;
        this._parameters = parameters;
        this._isCast = isCast;
        this._shaderType = shaderType;
    }
    
    get origin() { return this._origin; }
    get name() { return this._name; }
    get returnType() { return this._returnType; }
    get typeParameters() { return this._typeParameters; }
    get typeParametersForCallResolution() { return this.typeParameters; }
    get parameters() { return this._parameters; }
    get parameterTypes() { return this.parameters.map(parameter => parameter.type); }
    get isCast() { return this._isCast; }
    get shaderType() { return this._shaderType; }
    get returnTypeForOverloadResolution() { return this.isCast ? this.returnType : null; }
    
    get kind() { return Func; }
    
    toDeclString()
    {
        let result = "";
        if (this.shaderType)
            result += this.shaderType + " ";
        if (this.isCast)
            result += "operator<" + this.typeParameters + "> " + this.returnType;
        else
            result += this.returnType + " " + this.name + "<" + this.typeParameters + ">";
        return result + "(" + this.parameters + ")";
    }
    
    toString()
    {
        return this.toDeclString();
    }
}

