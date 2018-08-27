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

class Intrinsics {
    constructor(nameContext)
    {
        this._map = new Map();

        // NOTE: Intrinsic resolution happens before type name resolution, so the strings we use here
        // to catch the intrinsics must be based on the type names that StandardLibrary.js uses.
        // For example, if a native function is declared using "int" rather than "int", then we must
        // use "int" here, since we don't yet know that they are the same type.

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

        function cast(typedArrayConstructor, number)
        {
            var array = new typedArrayConstructor(1);
            array[0] = number;
            return array[0];
        }

        function bitwiseCast(typedArrayConstructor1, typedArrayConstructor2, value)
        {
            let typedArray1 = new typedArrayConstructor1(1);
            let typedArray2 = new typedArrayConstructor2(typedArray1.buffer);
            typedArray1[0] = value;
            return typedArray2[0];
        }

        function castToHalf(number)
        {
            // FIXME: Make this math obey IEEE 754.
            if (Number.isNaN(number))
               return number
            if (number > 65504)
                return Number.POSITIVE_INFINITY;
            if (number < -65504)
                return Number.NEGATIVE_INFINITY;
            if (number > 0 && number < Math.pow(2, -24))
                return 0;
            if (number < 0 && number > -Math.pow(2, -24))
                return -0;
            let doubleArray = new Float64Array(1);
            let uintArray = new Uint8Array(doubleArray.buffer);
            doubleArray[0] = number;
            let sign = uintArray[7] & 0x80;
            let exponent = ((uintArray[7] & 0x7f) << 4) | ((uintArray[6] & 0xf0) >>> 4);
            let significand = ((uintArray[6] & 0x0f) << 6) | ((uintArray[5] & 0xfc) >>> 2);

            if ((exponent - 1023) < -14) {
                exponent = 0;
                significand = (Math.abs(number) * Math.pow(2, 24)) >>> 0;
                let value = Math.pow(2, -14) * significand / 1024;
                if (sign != 0)
                    value *= -1;
                return value;
            }

            doubleArray[0] = 0;

            uintArray[7] |= sign;
            uintArray[7] |= (exponent >>> 4);
            uintArray[6] |= ((exponent << 4) & 0xf0);
            uintArray[6] |= (significand >>> 6);
            uintArray[5] |= ((significand << 2) & 0xfc);

            return doubleArray[0];
        }

        this._map.set(
            "native typedef void",
            type => {
                this.void = type;
                type.size = 0;
                type.populateDefaultValue = () => { };
            });

        this._map.set(
            "native typedef bool",
            type => {
                this.bool = type;
                type.isPrimitive = true;
                type.size = 1;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, false);
            });

        this._map.set(
            "native typedef uchar",
            type => {
                this.uchar = type;
                type.isPrimitive = true;
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
             "native typedef ushort",
             type => {
                 this.ushort = type;
                 type.isPrimitive = true;
                 type.isInt = true;
                 type.isNumber = true;
                 type.isSigned = false;
                 type.canRepresent = value => isBitwiseEquivalent(value & 0xffff, value);
                 type.size = 1;
                 type.defaultValue = 0;
                 type.createLiteral = (origin, value) => IntLiteral.withType(origin, value & 0xffff, type);
                 type.successorValue = value => (value + 1) & 0xffff;
                 type.valuesEqual = (a, b) => a === b;
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                 type.formatValueFromIntLiteral = value => value & 0xffff;
                 type.formatValueFromUintLiteral = value => value & 0xffff;
                 type.allValues = function*() {
                     for (let i = 0; i <= 0xffff; ++i)
                         yield {value: i, name: i};
                 };
             });

        this._map.set(
             "native typedef uint",
             type => {
                 this.uint = type;
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
             "native typedef char",
             type => {
                 this.char = type;
                 type.isPrimitive = true;
                 type.isInt = true;
                 type.isNumber = true;
                 type.isSigned = true;
                 type.canRepresent = value => isBitwiseEquivalent(cast(Int8Array, value), value);
                 type.size = 1;
                 type.defaultValue = 0;
                 type.createLiteral = (origin, value) => IntLiteral.withType(origin, cast(Int8Array, value), type);
                 type.successorValue = value => cast(Int8Array, value + 1);
                 type.valuesEqual = (a, b) => a === b;
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                 type.formatValueFromIntLiteral = value => cast(Int8Array, value);
                 type.formatValueFromUintLiteral = value => cast(Int8Array, value);
                 type.allValues = function*() {
                     for (let i = 0; i <= 0xff; ++i) {
                         let value = cast(Int8Array, i);
                         yield {value: value, name: value};
                     }
                 };
             });

        this._map.set(
             "native typedef short",
             type => {
                 this.short = type;
                 type.isPrimitive = true;
                 type.isInt = true;
                 type.isNumber = true;
                 type.isSigned = true;
                 type.canRepresent = value => isBitwiseEquivalent(cast(Int16Array, value), value);
                 type.size = 1;
                 type.defaultValue = 0;
                 type.createLiteral = (origin, value) => IntLiteral.withType(origin, cast(Int16Array, value), type);
                 type.successorValue = value => cast(Int16Array, value + 1);
                 type.valuesEqual = (a, b) => a === b;
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                 type.formatValueFromIntLiteral = value => cast(Int16Array, value);
                 type.formatValueFromUintLiteral = value => cast(Int16Array, value);
                 type.allValues = function*() {
                     for (let i = 0; i <= 0xffff; ++i) {
                         let value = cast(Int16Array, i);
                         yield {value: value, name: value};
                     }
                 };
             });

        this._map.set(
             "native typedef int",
             type => {
                 this.int = type;
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
             "native typedef half",
             type => {
                 this.half = type;
                 type.isPrimitive = true;
                 type.size = 1;
                 type.isFloating = true;
                 type.isNumber = true;
                 type.canRepresent = value => isBitwiseEquivalent(castToHalf(value), value);
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                 type.formatValueFromIntLiteral = value => value;
                 type.formatValueFromUintLiteral = value => value;
                 type.formatValueFromFloatLiteral = value => castToHalf(value);
             });

         this._map.set(
             "native typedef float",
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
             });

        this._map.set(
             "native typedef atomic_int",
             type => {
                 this.atomic_int = type;
             });

        this._map.set(
             "native typedef atomic_uint",
             type => {
                 this.atomic_uint = type;
             });

        for (let vectorType of VectorElementTypes) {
            for (let vectorSize of VectorElementSizes) {
                this._map.set(`native typedef vector<${vectorType}, ${vectorSize}>`, type => {
                    this[`vector<${vectorType}, ${vectorSize}>`] = type;
                    type.isPrimitive = true;
                });
            }
        }

        this._map.set(
            "native operator int(uint)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() | 0);
            });

        this._map.set(
            "native operator int(uchar)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() | 0);
            });

        this._map.set(
            "native operator int(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() | 0);
            });

        this._map.set(
            "native operator uint(int)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() >>> 0);
            });

        this._map.set(
            "native operator uint(uchar)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() >>> 0);
            });

        this._map.set(
            "native operator uint(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() >>> 0);
            });

        this._map.set(
            "native operator uchar(int)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() & 0xff);
            });

        this._map.set(
            "native operator uchar(uint)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() & 0xff);
            });

        this._map.set(
            "native operator uchar(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue() & 0xff);
            });

        this._map.set(
            "native operator float(int)",
            func => {
                func.implementation = ([value]) => EPtr.box(Math.fround(value.loadValue()));
            });

        this._map.set(
            "native operator float(uint)",
            func => {
                func.implementation = ([value]) => EPtr.box(Math.fround(value.loadValue()));
            });

        this._map.set(
            "native operator float(uchar)",
            func => {
                func.implementation = ([value]) => EPtr.box(Math.fround(value.loadValue()));
            });

        this._map.set(
            "native int operator+(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() + right.loadValue()) | 0);
            });

        this._map.set(
            "native uint operator+(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() + right.loadValue()) >>> 0);
            });

        this._map.set(
            "native float operator+(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() + right.loadValue()));
            });

        this._map.set(
            "native int operator-(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() - right.loadValue()) | 0);
            });

        this._map.set(
            "native uint operator-(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() - right.loadValue()) >>> 0);
            });

        this._map.set(
            "native float operator-(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() - right.loadValue()));
            });

        this._map.set(
            "native int operator*(int,int)",
            func => {
                func.implementation = ([left, right]) => {
                    return EPtr.box((left.loadValue() * right.loadValue()) | 0);
                };
            });

        this._map.set(
            "native uint operator*(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() * right.loadValue()) >>> 0);
            });

        this._map.set(
            "native float operator*(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() * right.loadValue()));
            });

        this._map.set(
            "native int operator/(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() / right.loadValue()) | 0);
            });

        this._map.set(
            "native uint operator/(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() / right.loadValue()) >>> 0);
            });

        this._map.set(
            "native int operator&(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() & right.loadValue());
            });

        this._map.set(
            "native uint operator&(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() & right.loadValue()) >>> 0);
            });

        this._map.set(
            "native int operator|(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() | right.loadValue());
            });

        this._map.set(
            "native uint operator|(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() | right.loadValue()) >>> 0);
            });

        this._map.set(
            "native int operator^(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() ^ right.loadValue());
            });

        this._map.set(
            "native uint operator^(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() ^ right.loadValue()) >>> 0);
            });

        this._map.set(
            "native int operator<<(int,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() << right.loadValue());
            });

        this._map.set(
            "native uint operator<<(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() << right.loadValue()) >>> 0);
            });

        this._map.set(
            "native int operator>>(int,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >> right.loadValue());
            });

        this._map.set(
            "native uint operator>>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >>> right.loadValue());
            });

        this._map.set(
            "native int operator~(int)",
            func => {
                func.implementation = ([value]) => EPtr.box(~value.loadValue());
            });

        this._map.set(
            "native uint operator~(uint)",
            func => {
                func.implementation = ([value]) => EPtr.box((~value.loadValue()) >>> 0);
            });

        this._map.set(
            "native float operator/(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() / right.loadValue()));
            });

        this._map.set(
            "native bool operator==(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() == right.loadValue());
            });

        this._map.set(
            "native bool operator==(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() == right.loadValue());
            });

        this._map.set(
            "native bool operator==(bool,bool)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() == right.loadValue());
            });

        this._map.set(
            "native bool operator==(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() == right.loadValue());
            });

        this._map.set(
            "native bool operator<(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() < right.loadValue());
            });

        this._map.set(
            "native bool operator<(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() < right.loadValue());
            });

        this._map.set(
            "native bool operator<(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() < right.loadValue());
            });

        this._map.set(
            "native bool operator<=(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() <= right.loadValue());
            });

        this._map.set(
            "native bool operator<=(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() <= right.loadValue());
            });

        this._map.set(
            "native bool operator<=(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() <= right.loadValue());
            });

        this._map.set(
            "native bool operator>(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() > right.loadValue());
            });

        this._map.set(
            "native bool operator>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() > right.loadValue());
            });

        this._map.set(
            "native bool operator>(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() > right.loadValue());
            });

        this._map.set(
            "native bool operator>=(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >= right.loadValue());
            });

        this._map.set(
            "native bool operator>=(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >= right.loadValue());
            });

        this._map.set(
            "native bool operator>=(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() >= right.loadValue());
            });

        for (let nativeVectorTypeName of allVectorTypeNames())
            this._map.set(`native typedef ${nativeVectorTypeName}`, type => {
                type.isPrimitive = true;
            });

        for (let swizzle of SwizzleOp.functions())
            this._map.set(swizzle.toString(), func => swizzle.instantiateImplementation(func));

        for (let boolOp of OperatorBool.functions())
            this._map.set(boolOp.toString(), func => boolOp.instantiateImplementation(func));

        for (let anderIndex of OperatorAnderIndexer.functions())
            this._map.set(anderIndex.toString(), func => anderIndex.instantiateImplementation(func));

        for (let cast of BuiltinVectorConstructors.functions())
            this._map.set(cast.toString(), func => cast.instantiateImplementation(func));

        for (let getter of BuiltinVectorIndexGetter.functions())
            this._map.set(getter.toString(), func => getter.instantiateImplementation(func));

        for (let setter of BuiltinVectorIndexSetter.functions())
            this._map.set(setter.toString(), func => setter.instantiateImplementation(func));

        for (let equalityOperator of BuiltinVectorEqualityOperator.functions())
            this._map.set(equalityOperator.toString(), func => equalityOperator.instantiateImplementation(func));

        for (let getter of BuiltinVectorGetter.functions())
            this._map.set(getter.toString(), func => getter.instantiateImplementation(func));

        for (let setter of BuiltinVectorSetter.functions())
            this._map.set(setter.toString(), func => setter.instantiateImplementation(func));
    }

    add(thing)
    {
        let intrinsic = this._map.get(thing.toString());
        if (!intrinsic)
            throw new WTypeError(thing.origin.originString, "Unrecognized intrinsic: " + thing);
        intrinsic(thing);
    }
}

