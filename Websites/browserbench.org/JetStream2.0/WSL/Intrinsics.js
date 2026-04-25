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
        this._map = new Map();

        // NOTE: Intrinsic resolution happens before type name resolution, so the strings we use here
        // to catch the intrinsics must be based on the type names that StandardLibraryPrologue.js uses.
        // For example, if a native function is declared using "int" rather than "int32", then we must
        // use "int" here, since we don't yet know that they are the same type.
        
        this._map.set(
            "native typedef void<>",
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
            "native typedef int32<>",
            type => {
                this.int32 = type;
                type.isPrimitive = true;
                type.isInt = true;
                type.isNumber = true;
                type.isSigned = true;
                type.canRepresent = value => isBitwiseEquivalent(value | 0, value);
                type.size = 1;
                type.defaultValue = 0;
                type.createLiteral = (origin, value) => IntLiteral.withType(origin, value | 0, type);
                type.successorValue = value => (value + 1) | 0;
                type.valuesEqual = (a, b) => a === b;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                type.formatValueFromIntLiteral = value => value | 0;
                type.formatValueFromUintLiteral = value => value | 0;
                type.allValues = function*() { 
                    for (let i = 0; i <= 0xffffffff; ++i) {
                        let value = i | 0;
                        yield {value: value, name: value};
                    }
                };
            });

        this._map.set(
            "native typedef uint32<>",
            type => {
                this.uint32 = type;
                type.isPrimitive = true;
                type.isInt = true;
                type.isNumber = true;
                type.isSigned = false;
                type.canRepresent = value => isBitwiseEquivalent(value >>> 0, value);
                type.size = 1;
                type.defaultValue = 0;
                type.createLiteral = (origin, value) => IntLiteral.withType(origin, value >>> 0, type);
                type.successorValue = value => (value + 1) >>> 0;
                type.valuesEqual = (a, b) => a === b;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                type.formatValueFromIntLiteral = value => value >>> 0;
                type.formatValueFromUintLiteral = value => value >>> 0;
                type.allValues = function*() { 
                    for (let i = 0; i <= 0xffffffff; ++i)
                        yield {value: i, name: i};
                };
            });

        this._map.set(
            "native typedef uint8<>",
            type => {
                this.uint8 = type;
                type.isInt = true;
                type.isNumber = true;
                type.isSigned = false;
                type.canRepresent = value => isBitwiseEquivalent(value & 0xff, value);
                type.size = 1;
                type.defaultValue = 0;
                type.createLiteral = (origin, value) => IntLiteral.withType(origin, value & 0xff, type);
                type.successorValue = value => (value + 1) & 0xff;
                type.valuesEqual = (a, b) => a === b;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                type.formatValueFromIntLiteral = value => value & 0xff;
                type.formatValueFromUintLiteral = value => value & 0xff;
                type.allValues = function*() {
                    for (let i = 0; i <= 0xff; ++i)
                        yield {value: i, name: i};
                };
            });

        this._map.set(
            "native typedef float32<>",
            type => {
                this.float = type;
                type.isPrimitive = true;
                type.size = 1;
                type.isFloating = true;
                type.isNumber = true;
                type.canRepresent = value => isBitwiseEquivalent(Math.fround(value), value);
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                type.formatValueFromIntLiteral = value => value;
                type.formatValueFromUintLiteral = value => value;
                type.formatValueFromFloatLiteral = value => Math.fround(value);
                type.formatValueFromDoubleLiteral = value => Math.fround(value);
            });

        this._map.set(
            "native typedef float64<>",
            type => {
                this.double = type;
                type.isPrimitive = true;
                type.size = 1;
                type.isFloating = true;
                type.isNumber = true;
                type.canRepresent = value => true;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                type.formatValueFromIntLiteral = value => value;
                type.formatValueFromUintLiteral = value => value;
                type.formatValueFromFloatLiteral = value => value;
                type.formatValueFromDoubleLiteral = value => value;
            });

        this._map.set(
            "native typedef bool<>",
            type => {
                this.bool = type;
                type.isPrimitive = true;
                type.size = 1;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, false);
            });
        
        this._map.set(
            "native operator<> int32(uint32)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() | 0);
            });
        
        this._map.set(
            "native operator<> int32(uint8)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() | 0);
            });
        
        this._map.set(
            "native operator<> int32(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() | 0);
            });
        
        this._map.set(
            "native operator<> int32(double)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() | 0);
            });
        
        this._map.set(
            "native operator<> uint32(int32)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() >>> 0);
            });
        
        this._map.set(
            "native operator<> uint32(uint8)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() >>> 0);
            });
        
        this._map.set(
            "native operator<> uint32(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() >>> 0);
            });
        
        this._map.set(
            "native operator<> uint32(double)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() >>> 0);
            });
        
        this._map.set(
            "native operator<> uint8(int32)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() & 0xff);
            });
        
        this._map.set(
            "native operator<> uint8(uint32)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() & 0xff);
            });
        
        this._map.set(
            "native operator<> uint8(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() & 0xff);
            });
        
        this._map.set(
            "native operator<> uint8(double)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() & 0xff);
            });
        
        this._map.set(
            "native operator<> float(double)",
            func => {
                func.implementation = ([value]) => EPtr.box(Math.fround(value.loadValue()));
            });
        
        this._map.set(
            "native operator<> float(int32)",
            func => {
                func.implementation = ([value]) => EPtr.box(Math.fround(value.loadValue()));
            });
        
        this._map.set(
            "native operator<> float(uint32)",
            func => {
                func.implementation = ([value]) => EPtr.box(Math.fround(value.loadValue()));
            });
        
        this._map.set(
            "native operator<> float(uint8)",
            func => {
                func.implementation = ([value]) => EPtr.box(Math.fround(value.loadValue()));
            });
        
        this._map.set(
            "native operator<> double(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue());
            });
        
        this._map.set(
            "native operator<> double(int32)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue());
            });
        
        this._map.set(
            "native operator<> double(uint32)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue());
            });
        
        this._map.set(
            "native operator<> double(uint8)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue());
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
            "native float operator+<>(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() + right.loadValue()));
            });
        
        this._map.set(
            "native double operator+<>(double,double)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() + right.loadValue());
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
            "native float operator-<>(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() - right.loadValue()));
            });
        
        this._map.set(
            "native double operator-<>(double,double)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() - right.loadValue());
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
            "native float operator*<>(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() * right.loadValue()));
            });
        
        this._map.set(
            "native double operator*<>(double,double)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() * right.loadValue());
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
            "native int operator&<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() & right.loadValue());
            });
        
        this._map.set(
            "native uint operator&<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() & right.loadValue()) >>> 0);
            });
        
        this._map.set(
            "native int operator|<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() | right.loadValue());
            });
        
        this._map.set(
            "native uint operator|<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() | right.loadValue()) >>> 0);
            });
        
        this._map.set(
            "native int operator^<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() ^ right.loadValue());
            });
        
        this._map.set(
            "native uint operator^<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() ^ right.loadValue()) >>> 0);
            });
        
        this._map.set(
            "native int operator<<<>(int,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() << right.loadValue());
            });
        
        this._map.set(
            "native uint operator<<<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() << right.loadValue()) >>> 0);
            });
        
        this._map.set(
            "native int operator>><>(int,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >> right.loadValue());
            });
        
        this._map.set(
            "native uint operator>><>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >>> right.loadValue());
            });
        
        this._map.set(
            "native int operator~<>(int)",
            func => {
                func.implementation = ([value]) => EPtr.box(~value.loadValue());
            });
        
        this._map.set(
            "native uint operator~<>(uint)",
            func => {
                func.implementation = ([value]) => EPtr.box((~value.loadValue()) >>> 0);
            });
        
        this._map.set(
            "native float operator/<>(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() / right.loadValue()));
            });
        
        this._map.set(
            "native double operator/<>(double,double)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() / right.loadValue());
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
            "native bool operator==<>(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() == right.loadValue());
            });
        
        this._map.set(
            "native bool operator==<>(double,double)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() == right.loadValue());
            });
        
        this._map.set(
            "native bool operator<<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() < right.loadValue());
            });
        
        this._map.set(
            "native bool operator<<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() < right.loadValue());
            });
        
        this._map.set(
            "native bool operator<<>(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() < right.loadValue());
            });
        
        this._map.set(
            "native bool operator<<>(double,double)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() < right.loadValue());
            });
        
        this._map.set(
            "native bool operator<=<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() <= right.loadValue());
            });
        
        this._map.set(
            "native bool operator<=<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() <= right.loadValue());
            });
        
        this._map.set(
            "native bool operator<=<>(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() <= right.loadValue());
            });
        
        this._map.set(
            "native bool operator<=<>(double,double)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() <= right.loadValue());
            });
        
        this._map.set(
            "native bool operator><>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() > right.loadValue());
            });
        
        this._map.set(
            "native bool operator><>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() > right.loadValue());
            });
        
        this._map.set(
            "native bool operator><>(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() > right.loadValue());
            });
        
        this._map.set(
            "native bool operator><>(double,double)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() > right.loadValue());
            });
        
        this._map.set(
            "native bool operator>=<>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >= right.loadValue());
            });
        
        this._map.set(
            "native bool operator>=<>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >= right.loadValue());
            });
        
        this._map.set(
            "native bool operator>=<>(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >= right.loadValue());
            });
        
        this._map.set(
            "native bool operator>=<>(double,double)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >= right.loadValue());
            });
        
        for (let addressSpace of addressSpaces) {
            this._map.set(
                `native T* ${addressSpace} operator&[]<T>(T[] ${addressSpace},uint)`,
                func => {
                    func.implementation = ([ref, index], node) => {
                        ref = ref.loadValue();
                        if (!ref)
                            throw new WTrapError(node.origin.originString, "Null dereference");
                        index = index.loadValue();
                        if (index > ref.length)
                            throw new WTrapError(node.origin.originString, "Array index " + index + " is out of bounds of " + ref);
                        return EPtr.box(ref.ptr.plus(index * node.instantiatedActualTypeArguments[0].size));
                    };
                });

            this._map.set(
                `native uint operator.length<T>(T[] ${addressSpace})`,
                func => {
                    func.implementation = ([ref], node) => {
                        ref = ref.loadValue();
                        if (!ref)
                            return EPtr.box(0);
                        return EPtr.box(ref.length);
                    };
                });
        }
    }
    
    add(thing)
    {
        let intrinsic = this._map.get(thing.toString());
        if (!intrinsic)
            throw new WTypeError(thing.origin.originString, "Unrecognized intrinsic: " + thing);
        intrinsic(thing);
    }
}

