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

class TypeRef extends Type {
    constructor(origin, name, typeArguments)
    {
        super();
        this._origin = origin;
        this._name = name;
        this.type = null;
        this._typeArguments = typeArguments;
    }
    
    static wrap(type)
    {
        if (type instanceof TypeRef && !type.typeArguments)
            return type;
        let name = type.name;
        let result = new TypeRef(type.origin, name, []);
        result.type = type;
        return result;
    }
    
    static instantiate(type, typeArguments)
    {
        let result = new TypeRef(type.origin, type.name, typeArguments);
        result.type = type;
        return result;
    }
 
    get origin() { return this._origin; }
    get name() { return this._name; }
    get typeArguments() { return this._typeArguments; }
    
    get unifyNode()
    {
        if (!this.typeArguments.length)
            return this.type.unifyNode;
        return this;
    }
    
    populateDefaultValue(buffer, offset)
    {
        if (!this.typeArguments.length)
            return this.type.populateDefaultValue(buffer, offset);
        throw new Error("Cannot get default value of a type instantiation");
    }
    
    get size()
    {
        if (!this.typeArguments.length)
            return this.type.size;
        throw new Error("Cannot get size of a type instantiation");
    }
    
    get isPrimitive()
    {
        if (!this.typeArguments.length)
            return this.type.isPrimitive;
        throw new Error("Cannot determine if an uninstantiated type is primitive: " + this);
    }
    
    setTypeAndArguments(type, typeArguments)
    {
        this._name = null;
        this.type = type;
        this._typeArguments = typeArguments;
    }
    
    unifyImpl(unificationContext, other)
    {
        if (!(other instanceof TypeRef))
            return false;
        if (!this.type.unify(unificationContext, other.type))
            return false;
        if (this.typeArguments.length != other.typeArguments.length)
            return false;
        for (let i = 0; i < this.typeArguments.length; ++i) {
            if (!this.typeArguments[i].unify(unificationContext, other.typeArguments[i]))
                return false;
        }
        return true;
    }
    
    toString()
    {
        if (!this.name)
            return this.type.toString();
        if (!this.typeArguments.length)
            return this.name;
        return this.name + "<" + this.typeArguments + ">";
    }
}

