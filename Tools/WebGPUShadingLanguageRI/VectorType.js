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

class VectorType extends NativeType {
    constructor(origin, name)
    {
        super(origin, name);
        const match = /^([A-z]+)([0-9])$/.exec(name);
        if (!match)
            throw new WTypeError(origin.originString, `${name} doesn't match the format for vector type names.'`);

        this._elementType = new TypeRef(origin, match[1]);
        this._numElementsValue = parseInt(match[2]);
    }

    get elementType() { return this._elementType; }
    get numElementsValue() { return this._numElementsValue; }
    get size() { return this.elementType.size * this.numElementsValue; }

    unifyImpl(unificationContext, other)
    {
        if (!(other instanceof VectorType))
            return false;

        if (this.numElementsValue !== other.numElementsValue)
            return false;

        return this.elementType.unify(unificationContext, other.elementType);
    }

    populateDefaultValue(buffer, offset)
    {
        for (let i = 0; i < this.numElementsValue; ++i)
            this.elementType.populateDefaultValue(buffer, offset + i * this.elementType.size);
    }

    toString()
    {
        return `native typedef ${this.elementType}${this.numElementsValue}`;
    }
}

