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

class BuiltinVectorConstructors {
    constructor(baseTypeName, parameterSizes)
    {
        this._baseTypeName = baseTypeName;
        this._parameterSizes = parameterSizes;
    }

    get baseTypeName() { return this._baseTypeName; }
    get parameterSizes() { return this._parameterSizes; }
    get outputSize()
    {
        return this.parameterSizes.reduce((a, b) => a + b, 0);
    }

    toString()
    {
        return `native operator ${this.baseTypeName}${this.outputSize}(${this.parameterSizes.map(x => x == 1 ? this.baseTypeName : this.baseTypeName + x).join(",")})`;
    }

    static functions()
    {
        if (!this._functions) {
            this._functions = [];

            for (let typeName of VectorElementTypes) {
                for (let size of VectorElementSizes) {
                    for (let paramSizes of this._vectorParameterSizesForMaximumSize(size))
                        this._functions.push(new BuiltinVectorConstructors(typeName, paramSizes));
                }
            }
        }
        return this._functions;
    }

    static _vectorParameterSizesForMaximumSize(maxSize)
    {
        let variants = [ [ maxSize ] ];
        for (let splitPoint = 1; splitPoint < maxSize; splitPoint++) {
            for (let v of BuiltinVectorConstructors._vectorParameterSizesForMaximumSize(maxSize - splitPoint))
                variants.push([ splitPoint ].concat(v));
        }
        return variants;
    }

    instantiateImplementation(func)
    {
        func.implementation = (args) => {
            const result = new EPtr(new EBuffer(this.outputSize), 0);
            let offset = 0;
            for (let i = 0; i < args.length; i++) {
                for (let j = 0; j < this.parameterSizes[i]; j++)
                    result.set(offset++, args[i].get(j));
            }
            return result;
        };
        func.implementationData = this;
    }
}