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

class BuiltinMatrixSetter {
    constructor(baseTypeName, height, width)
    {
        this._baseTypeName = baseTypeName;
        this._height = height;
        this._width = width;
    }

    get baseTypeName() { return this._baseTypeName; }
    get height() { return this._height; }
    get width() { return this._width; }

    toString()
    {
        return `native ${this.baseTypeName}${this.height}x${this.width} operator[]=(${this.baseTypeName}${this.height}x${this.width},uint,${this.baseTypeName}${this.width})`;
    }

    static functions()
    {
        if (!this._functions) {
            this._functions = [];

            for (let typeName of ["half", "float"]) {
                for (let height of [2, 3, 4]) {
                    for (let width of [2, 3, 4])
                        this._functions.push(new BuiltinMatrixSetter(typeName, height, width));
                }
            }
        }
        return this._functions;
    }

    instantiateImplementation(func)
    {
        func.implementation = ([base, index, value]) => {
            const indexValue = index.loadValue();
            if (indexValue >= 0 && indexValue < this.height) {
                let result = new EPtr(new EBuffer(this.width * this.height), 0);
                result.copyFrom(base, this.width * this.height);
                result.plus(indexValue * this.width).copyFrom(value, this.width);
                return result;
            } else
                throw new WTrapError("[Builtin matrix setter]", "Out-of-bounds index when indexing into a matrix");
        };
        func.implementationData = this;
    }
}
