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

class BuiltinVectorEqualityOperator {
    constructor(baseTypeName, size)
    {
        this._baseTypeName = baseTypeName;
        this._size = size;
    }

    get baseTypeName() { return this._baseTypeName; }
    get size() { return this._size; }

    toString()
    {
        return `native bool operator==(${this.baseTypeName}${this.size},${this.baseTypeName}${this.size})`;
    }

    static functions()
    {
        if (!this._functions) {
            this._functions = [];

            for (let typeName of VectorElementTypes) {
                for (let size of VectorElementSizes)
                    this._functions.push(new BuiltinVectorEqualityOperator(typeName, size));
            }
        }
        return this._functions;
    }

    instantiateImplementation(func)
    {
        func.implementation = ([ref1, ref2], node) => {
            for (let i = 0; i < this.size; i++) {
                if (ref1.get(i) != ref2.get(i))
                    return EPtr.box(false);
            }
            return EPtr.box(true);
        };
        func.implementationData = this;
    }
}