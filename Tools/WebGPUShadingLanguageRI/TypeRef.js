/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
    constructor(origin, name, typeArguments = [])
    {
        super();
        this._origin = origin;
        this._name = name;
        this._type = null;
        this._typeArguments = typeArguments;
    }
    
    static wrap(type)
    {
        if (type instanceof TypeRef)
            return type;
        let result;
        if (type instanceof NativeType)
            result = new TypeRef(type.origin, type.name, type.typeArguments);
        else
            result = new TypeRef(type.origin, type.name);
        result.type = type;
        return result;
    }
 
    get origin() { return this._origin; }
    get name() { return this._name; }
    get typeArguments() { return this._typeArguments; }
    
    get type()
    {
        return this._type;
    }

    set type(newType)
    {
        this._type = newType;
    }

    resolve(possibleOverloads)
    {
        if (!possibleOverloads)
            throw new WTypeError(this.origin.originString, "Did not find any types named " + this.name);

        let failures = [];
        let overload = resolveTypeOverloadImpl(possibleOverloads, this.typeArguments);

        if (!overload.type) {
            failures.push(...overload.failures);
            let message = "Did not find type named " + this.name + " for type arguments ";
            message += "(" + this.typeArguments + ")";
            if (failures.length)
                message += ", but considered:\n" + failures.join("\n")
            throw new WTypeError(this.origin.originString, message);
        }

        for (let i = 0; i < this.typeArguments.length; ++i) {
            let typeArgument = this.typeArguments[i];
            let resolvedTypeArgument = overload.type.typeArguments[i];
            let result = typeArgument.equalsWithCommit(resolvedTypeArgument);
            if (!result)
                throw new Error("At " + this.origin.originString + " argument types for Type and TypeRef not equal: argument type = " + typeArgument + ", resolved type argument = " + resolvedTypeArgument);
            if (resolvedTypeArgument.constructor.name == "GenericLiteral") {
                result = typeArgument.type.equalsWithCommit(resolvedTypeArgument.type);
                if (!result)
                    throw new Error("At " + this.origin.originString + " argument types for Type and TypeRef not equal: argument type = " + typeArgument + ", resolved type argument = " + resolvedTypeArgument);
            }
                
        }
        this.type = overload.type;
    }

    get unifyNode()
    {
        if (!this.type)
            throw new Error(`No type when evaluating ${this} unifyNode`);
        return this.type.unifyNode;
    }
    
    populateDefaultValue(buffer, offset)
    {
        return this.type.populateDefaultValue(buffer, offset);
    }
    
    get size()
    {
        return this.type.size;
    }
    
    get isPrimitive()
    {
        return this.type.isPrimitive;
    }
    
    unifyImpl(unificationContext, other)
    {
        if (!(other instanceof TypeRef))
            return false;
        if (!this.type.unify(unificationContext, other.type))
            return false;
        return true;
    }
    
    toString()
    {
        if (!this.name)
            return this.type.toString();
        let result = this.name;
        if (this.typeArguments.length > 0)
            result += "<" + this.typeArguments.map(argument => argument.toString()).join(",") + ">";
        return result;
    }
}

