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

class SwizzleOp {
    constructor(baseTypeName, outSize, components, inSize)
    {
        this._baseTypeName = baseTypeName;
        this._outSize = outSize;
        this._components = components.slice(); // Shallow clone
        this._inSize = inSize;
    }

    get baseTypeName() { return this._baseTypeName; }
    get outSize() { return this._outSize; }
    get components() { return this._components; }
    get inSize() { return this._inSize; }

    toString()
    {
        return `native ${this.baseTypeName}${this.outSize} operator.${this.components.join("")}(${this.baseTypeName}${this.inSize} v)`;
    }

    static intToString(x)
    {
        switch (x) {
        case 0:
            return "x";
        case 1:
            return "y";
        case 2:
            return "z";
        case 3:
            return "w";
        default:
            throw new Error("Could not generate standard library.");
        }
    }

    static functions()
    {
        if (!this._functions) {
            // We can't directly use this._functions in _generateSwizzles.
            const functions = [];

            function _generateSwizzle(baseTypeName, maxDepth, maxItems, array) {
                if (!array)
                    array = [];
                if (array.length == maxDepth) {
                    functions.push(new SwizzleOp(baseTypeName, array.length, array, maxItems));
                    return;
                }
                for (let i = 0; i < maxItems; ++i) {
                    array.push(SwizzleOp.intToString(i));
                    _generateSwizzle(baseTypeName, maxDepth, maxItems, array);
                    array.pop();
                }
            };

            for (let typeName of VectorElementTypes) {
                for (let maxDepth of VectorElementSizes) {
                    for (let maxItems = 2; maxItems <= 4; maxItems++)
                        _generateSwizzle(typeName, maxDepth, maxItems);
                }
            }

            this._functions = functions;
        }
        return this._functions;
    }

    instantiateImplementation(func)
    {
        func.implementation = ([vec], node) => {
            const outputBuffer = new EBuffer(this.outSize);
            const readIndices = { 'x': 0, 'y': 1, 'z': 2, 'w': 3 };
            for (let i = 0; i < this.outSize; i++)
                outputBuffer.set(i, vec.get(readIndices[this.components[i]]));


            return new EPtr(outputBuffer, 0);
        };
        func.implementationData = this;
    }
}