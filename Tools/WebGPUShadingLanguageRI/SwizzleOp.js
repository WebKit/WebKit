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

class SwizzleOp {
    constructor(outSize, components, inSize)
    {
        this._outSize = outSize;
        this._components = components.slice(); // Shallow clone
        this._inSize = inSize;
    }

    get outSize() { return this._outSize; }
    get components() { return this._components; }
    get inSize() { return this._inSize; }

    toString()
    {
        return `native vec${this.outSize}<T> operator.${this.components.join("")}<T>(vec${this.inSize}<T> v)`;
    }

    static allSwizzleOperators()
    {
        if (!SwizzleOp._allSwizzleOperators) {
            SwizzleOp._allSwizzleOperators = [];
            
            function _generateSwizzle(maxDepth, maxItems, array) {
                if (!array)
                    array = [];
                if (array.length == maxDepth) {
                    SwizzleOp._allSwizzleOperators.push(new SwizzleOp(array.length, array, maxItems));
                    return;
                }
                for (let i = 0; i < maxItems; ++i) {
                    array.push(intToString(i));
                    _generateSwizzle(maxDepth, maxItems, array);
                    array.pop();
                }
            };
        
            for (let maxDepth = 2; maxDepth <= 4; maxDepth++) {
                for (let maxItems = 2; maxItems <= 4; maxItems++)
                    _generateSwizzle(maxDepth, maxItems);
            }
        }
        return SwizzleOp._allSwizzleOperators;
    }
}

// Initialise the static member (JS doesn't allow static fields declared in the class)
SwizzleOp._allSwizzleOperators = null;