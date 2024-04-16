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

class StructType extends Type {
    constructor(origin, name, typeParameters)
    {
        super();
        this._origin = origin;
        this._name = name;
        this._typeParameters = typeParameters;
        this._fields = new Map();
    }
    
    add(field)
    {
        field.struct = this;
        if (this._fields.has(field.name))
            throw new WTypeError(field.origin.originString, "Duplicate field name: " + field.name);
        this._fields.set(field.name, field);
    }
    
    get name() { return this._name; }
    get origin() { return this._origin; }
    get typeParameters() { return this._typeParameters; }
    
    get fieldNames() { return this._fields.keys(); }
    fieldByName(name) { return this._fields.get(name); }
    get fields() { return this._fields.values(); }
    get fieldMap() { return this._fields; }
    get isPrimitive()
    {
        let result = true;
        for (let field of this.fields)
            result &= field.type.isPrimitive;
        return result;
    }
    
    instantiate(typeArguments = null)
    {
        let substitution = null;
        let typeParameters = this.typeParameters;
        
        if (typeArguments) {
            if (typeArguments.length != this.typeParameters.length)
                throw new WTypeError(this.origin.originString, "Wrong number of type arguments to instantiation");
            
            substitution = new Substitution(this.typeParameters, typeArguments);
            typeParameters = [];
        }
        
        let instantiateImmediates = new InstantiateImmediates();
        let result = new StructType(this.origin, this.name, typeParameters);
        for (let field of this.fields) {
            let newField = field;
            if (substitution)
                newField = newField.visit(substitution);
            newField = newField.visit(instantiateImmediates);
            result.add(newField);
        }
        
        return result;
    }
    
    populateDefaultValue(buffer, offset)
    {
        if (this.size == null)
            throw new Error("Struct does not have layout: " + this + " " + describe(this));
        for (let field of this.fields)
            field.type.populateDefaultValue(buffer, offset + field.offset);
    }
    
    toString()
    {
        return "struct " + this.name + "<" + this.typeParameters + "> { " + Array.from(this.fields).join("; ") + "; }";
    }
}
