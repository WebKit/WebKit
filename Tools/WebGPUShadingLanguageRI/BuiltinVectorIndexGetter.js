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

class BuiltinVectorIndexGetter {
    constructor(baseTypeName, size)
    {
        this._baseTypeName = baseTypeName;
        this._size = size;
    }

    get baseTypeName() { return this._baseTypeName; }
    get size() { return this._size; }

    toString()
    {
        return `native ${this.baseTypeName} operator[](${this.baseTypeName}${this.size},uint)`;
    }

    static functions()
    {
        if (!this._allIndexGetters) {
            this._allIndexGetters = [];

            for (let typeName of VectorElementTypes) {
                for (let size of VectorElementSizes)
                    this._allIndexGetters.push(new BuiltinVectorIndexGetter(typeName, size));
            }
        }
        return this._allIndexGetters;
    }

    instantiateImplementation(func)
    {
        func.implementation = ([vec, index], node) => {
            const indexValue = index.loadValue();
            if (indexValue >= 0 && indexValue < this.size)
                return EPtr.box(vec.get(index.loadValue()));
            else
                throw new Error(`${indexValue} out of bounds for vector of size ${this.size}`);
        };
        func.implementationData = this;
    }
}