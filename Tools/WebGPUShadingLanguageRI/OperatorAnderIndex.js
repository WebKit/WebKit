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

class OperatorAnderIndexer {
    constructor(baseTypeName, addressSpace)
    {
        this._baseTypeName = baseTypeName;
        this._addressSpace = addressSpace;
    }

    get addressSpace() { return this._addressSpace; }
    get baseTypeName() { return this._baseTypeName; }

    toString()
    {
        return `native ${this.baseTypeName}* ${this.addressSpace} operator&[](${this.baseTypeName}[] ${this.addressSpace},uint)`;
    }

    static functions()
    {
        if (!this._functions) {
            this._functions = [];

            const typeNames = [ "uint", "int", "float", "bool" ].concat(allVectorTypeNames());
            const addressSpaces = [ "thread", "threadgroup", "device", "constant" ];

            for (let addressSpace of addressSpaces) {
                for (let typeName of typeNames)
                    this._functions.push(new OperatorAnderIndexer(typeName, addressSpace));
            }
        }
        return this._functions;
    }

    instantiateImplementation(func)
    {
        func.implementation = ([ref, index], node) => {
            ref = ref.loadValue();
            if (!ref)
                throw new WTrapError(node.origin.originString, "Null dereference");
            index = index.loadValue();
            if (index > ref.length)
                throw new WTrapError(node.origin.originString, "Array index " + index + " is out of bounds of " + ref);
            return EPtr.box(ref.ptr.plus(index * node.argumentTypes[0].elementType.size));
        };
        func.implementationData = this;
    }
}