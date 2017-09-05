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

class Intrinsics {
    constructor(nameContext)
    {
        this.primitive = new ProtocolDecl(null, "primitive");
        this.primitive.isPrimitive = true;
        nameContext.add(this.primitive);
        
        this._map = new Map();

        // NOTE: Intrinsic resolution happens before type name resolution, so the strings we use here
        // to catch the intrinsics must be based on the type names that StandardLibrary.js uses. For
        // example, if a native function is declared using "int" rather than "int32", then we must use
        // "int" here, since we don't yet know that they are the same type.
        
        this._map.set(
            "native primitive type void<>",
            type => {
                this.void = type;
                type.size = 0;
                type.populateDefaultValue = () => { };
            });
        
        function isBitwiseEquivalent(left, right)
        {
            let doubleArray = new Float64Array(1);
            let intArray = new Int32Array(doubleArray.buffer);
            doubleArray[0] = left;
            let leftInts = Int32Array.from(intArray);
            doubleArray[0] = right;
            for (let i = 0; i < 2; ++i) {
                if (leftInts[i] != intArray[i])
                    return false;
            }
            return true;
        }

        this._map.set(
            "native primitive type int32<>",
            type => {
                this.int32 = type;
                type.isInt = true;
                type.isNumber = true;
                type.canRepresent = value => isBitwiseEquivalent(value | 0, value);
                type.size = 1;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
            });

        this._map.set(
            "native primitive type uint32<>",
            type => {
                this.uint32 = type;
                type.isInt = true;
                type.isNumber = true;
                type.canRepresent = value => isBitwiseEquivalent(value >>> 0, value);
                type.size = 1;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
            });

        this._map.set(
            "native primitive type double<>",
            type => {
                this.double = type;
                type.size = 1;
                type.isFloat = true;
                type.isNumber = true;
                type.canRepresent = value => true;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
            });

        this._map.set(
            "native primitive type bool<>",
            type => {
                this.bool = type;
                type.size = 1;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, false);
            });
        
        this._map.set(
            "native int operator+<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() + right.loadValue()) | 0);
            });
        
        this._map.set(
            "native uint operator+<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() + right.loadValue()) >>> 0);
            });
        
        this._map.set(
            "native int operator-<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() - right.loadValue()) | 0);
            });
        
        this._map.set(
            "native uint operator-<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() - right.loadValue()) >>> 0);
            });
        
        this._map.set(
            "native int operator*<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() * right.loadValue()) | 0);
            });
        
        this._map.set(
            "native uint operator*<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() * right.loadValue()) >>> 0);
            });
        
        this._map.set(
            "native int operator/<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() / right.loadValue()) | 0);
            });
        
        this._map.set(
            "native uint operator/<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() / right.loadValue()) >>> 0);
            });
        
        this._map.set(
            "native bool operator==<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() == right.loadValue());
            });
        
        this._map.set(
            "native bool operator==<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() == right.loadValue());
            });
        
        this._map.set(
            "native bool operator==<>(bool,bool)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() == right.loadValue());
            });
        
        this._map.set(
            "native operator int<>(int)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(value.loadValue());
            });
        
        this._map.set(
            "native operator uint<>(uint)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(value.loadValue());
            });
        
        this._map.set(
            "native operator bool<>(bool)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(value.loadValue());
            });
        
        let arrayElementPtr = func => {
            func.implementation = ([ref, index], node) => {
                ref = ref.loadValue();
                if (!ref)
                    throw new WTrapError(node.origin.originString, "Null dereference");
                index = index.loadValue();
                if (index > ref.length)
                    throw new WTrapError(node.origin.originString, "Array index " + index + " is out of bounds of " + ref);
                return EPtr.box(ref.ptr.plus(index * node.actualTypeArguments[0].size));
            };
        };
        
        this._map.set(
            "native thread T^ operator&[]<T>(thread T[],uint)",
            arrayElementPtr);
        this._map.set(
            "native threadgroup T^ operator&[]<T:primitive>(threadgroup T[],uint)",
            arrayElementPtr);
        this._map.set(
            "native device T^ operator&[]<T:primitive>(device T[],uint)",
            arrayElementPtr);
        this._map.set(
            "native constant T^ operator&[]<T:primitive>(constant T[],uint)",
            arrayElementPtr);
    }
    
    add(thing)
    {
        let intrinsic = this._map.get(thing.toString());
        if (!intrinsic)
            throw new WTypeError(thing.origin.originString, "Unrecognized intrinsic: " + thing);
        intrinsic(thing);
    }
}

