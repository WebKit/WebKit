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

class ArrayType extends Type {
    constructor(origin, elementType, numElements)
    {
        if (!numElements)
            throw new Error("null numElements");
        super();
        this._origin = origin;
        this._elementType = elementType;
        this._numElements = numElements;
    }
    
    get origin() { return this._origin; }
    get elementType() { return this._elementType; }
    get numElements() { return this._numElements; }
    get isPrimitive() { return this.elementType.isPrimitive; }
    get isArray() { return true; }
    
    get numElementsValue()
    {
        if (!(this.numElements.isLiteral))
            throw new Error("numElements is not a literal: " + this.numElements);
        return this.numElements.value;
    }

    toString()
    {
        return this.elementType + "[" + this.numElements + "]";
    }
    
    get size()
    {
        return this.elementType.size * this.numElementsValue;
    }
    
    populateDefaultValue(buffer, offset)
    {
        for (let i = 0; i < this.numElementsValue; ++i)
            this.elementType.populateDefaultValue(buffer, offset + i * this.elementType.size);
    }
    
    unifyImpl(unificationContext, other)
    {
        if (!(other instanceof ArrayType))
            return false;
        
        if (!this.numElements.unify(unificationContext, other.numElements))
            return false;
        
        return this.elementType.unify(unificationContext, other.elementType);
    }

    argumentForAndOverload(origin, value)
    {
        let result = new MakeArrayRefExpression(origin, value);
        result.numElements = this.numElements;
        return result;
    }
    argumentTypeForAndOverload(origin)
    {
        return new ArrayRefType(origin, "thread", this.elementType);
    }
}

