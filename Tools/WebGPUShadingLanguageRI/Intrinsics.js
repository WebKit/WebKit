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
                 type.canRepresent = value => true;
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
                 type.canRepresent = value => true;
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

        for (let type of ["half", "float"]) {
            for (let height = 2; height <= 4; ++height) {
                for (let width = 2; width <= 4; ++width) {
                    this._map.set(`native typedef matrix<${type}, ${height}, ${width}>`, type => {
                        this[`matrix<${type}, ${height}, ${width}>`] = type;
                    });
                }
            }
        }

        this._map.set(
            "native typedef sampler",
            type => {
                this.sampler = type;
                // FIXME: Figure out what to put here.
            });

        for (let textureType of ["Texture1D", "RWTexture1D", "Texture1DArray", "RWTexture1DArray", "Texture2D", "RWTexture2D", "Texture2DArray", "RWTexture2DArray", "Texture3D", "RWTexture3D", "TextureCube"]) {
            for (let typeArgument of ["bool", "uchar", "ushort", "uint", "char", "short", "int", "half", "float"]) {
                this._map.set(
                    `native typedef ${textureType}<${typeArgument}>`,
                    type => {
                        this[`${textureType}<${typeArgument}>`] = type;
                    });
                for (let i = 2; i <= 4; ++i) {
                    this._map.set(
                        `native typedef ${textureType}<${typeArgument}${i}>`,
                        type => {
                            this[`${textureType}<${typeArgument}${i}>`] = type;
                        });
                }
            }
        }

        for (let textureType of ["TextureDepth2D", "RWTextureDepth2D", "TextureDepth2DArray", "RWTextureDepth2DArray", "TextureDepthCube"]) {
            for (let typeArgument of ["float", "half"]) {
                this._map.set(
                    `native typedef ${textureType}<${typeArgument}>`,
                    type => {
                        this[`${textureType}<${typeArgument}>`] = type;
                    });
            }
        }

        for (let primitiveType of ["ushort", "uint", "char", "short", "int", "half", "float"]) {
            this._map.set(
                `native operator uchar(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(value.loadValue() & 0xff);
                });
        }

        for (let primitiveType of ["uchar", "uint", "char", "short", "int", "half", "float"]) {
            this._map.set(
                `native operator ushort(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(value.loadValue() & 0xffff);
                });
        }

        for (let primitiveType of ["uchar", "ushort", "char", "short", "int", "half", "float"]) {
            this._map.set(
                `native operator uint(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(value.loadValue() >>> 0);
                });
        }

        for (let primitiveType of ["uchar", "ushort", "uint", "short", "int", "half", "float"]) {
            this._map.set(
                `native operator char(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(cast(Int8Array, value.loadValue()));
                });
        }

        for (let primitiveType of ["uchar", "ushort", "uint", "char", "int", "half", "float"]) {
            this._map.set(
                `native operator short(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(cast(Int16Array, value.loadValue()));
                });
        }

        for (let primitiveType of ["uchar", "ushort", "uint", "char", "short", "half", "float"]) {
            this._map.set(
                `native operator int(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(cast(Int32Array, value.loadValue()));
                });
        }

        for (let primitiveType of ["uchar", "ushort", "uint", "char", "short", "int", "float"]) {
            this._map.set(
                `native operator half(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(castToHalf(value.loadValue()));
                });
        }

        for (let primitiveType of ["uchar", "ushort", "uint", "char", "short", "int", "half"]) {
            this._map.set(
                `native operator float(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(Math.fround(value.loadValue()));
                });
        }

        this._map.set(
            `native operator int(atomic_int)`,
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue());
            });

        this._map.set(
            `native operator uint(atomic_uint)`,
            func => {
                func.implementation = ([value]) => EPtr.box(value.loadValue());
            });

        this._map.set(
            "native bool operator==(bool,bool)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(left.loadValue() == right.loadValue());
            });

        for (let primitiveType of ["uint", "int", "float"]) {
            this._map.set(
                `native bool operator==(${primitiveType},${primitiveType})`,
                func => {
                    func.implementation = ([left, right]) =>
                        EPtr.box(left.loadValue() == right.loadValue());
                });

            this._map.set(
                `native bool operator<(${primitiveType},${primitiveType})`,
                func => {
                    func.implementation = ([left, right]) =>
                        EPtr.box(left.loadValue() < right.loadValue());
                });

            this._map.set(
                `native bool operator<=(${primitiveType},${primitiveType})`,
                func => {
                    func.implementation = ([left, right]) =>
                        EPtr.box(left.loadValue() <= right.loadValue());
                });

            this._map.set(
                `native bool operator>(${primitiveType},${primitiveType})`,
                func => {
                    func.implementation = ([left, right]) =>
                        EPtr.box(left.loadValue() > right.loadValue());
                });

            this._map.set(
                `native bool operator>=(${primitiveType},${primitiveType})`,
                func => {
                    func.implementation = ([left, right]) =>
                        EPtr.box(left.loadValue() >= right.loadValue());
                });
        }

        this._map.set(
            "native int operator-(int)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box((-value.loadValue()) | 0);
            });

        this._map.set(
            "native float operator-(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(-value.loadValue()));
            });

        this._map.set(
            "native int operator+(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() + right.loadValue()) | 0);
            });

        this._map.set(
            "native int operator-(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() - right.loadValue()) | 0);
            });

        this._map.set(
            "native int operator*(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() * right.loadValue()) | 0);
            });

        this._map.set(
            "native int operator/(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() / right.loadValue()) | 0);
            });

        this._map.set(
            "native uint operator+(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() + right.loadValue()) >>> 0);
            });

        this._map.set(
            "native uint operator-(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() - right.loadValue()) >>> 0);
            });

        this._map.set(
            "native uint operator*(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() * right.loadValue()) >>> 0);
            });

        this._map.set(
            "native uint operator/(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() / right.loadValue()) >>> 0);
            });

        this._map.set(
            "native float operator+(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() + right.loadValue()));
            });

        this._map.set(
            "native float operator-(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() - right.loadValue()));
            });

        this._map.set(
            "native float operator*(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() * right.loadValue()));
            });

        this._map.set(
            "native float operator/(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(left.loadValue() / right.loadValue()));
            });

        this._map.set(
            "native int operator&(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() & right.loadValue()) | 0);
            });

        this._map.set(
            "native int operator|(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() | right.loadValue()) | 0);
            });

        this._map.set(
            "native int operator^(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() ^ right.loadValue()) | 0);
            });

        this._map.set(
            "native int operator~(int)",
            func => {
                func.implementation = ([value]) => EPtr.box((~value.loadValue()) | 0);
            });

        this._map.set(
            "native int operator<<(int,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() << right.loadValue()) | 0);
            });

        this._map.set(
            "native int operator>>(int,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() >> right.loadValue()) | 0);
            });

        this._map.set(
            "native uint operator&(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() & right.loadValue()) >>> 0);
            });

        this._map.set(
            "native uint operator|(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() | right.loadValue()) >>> 0);
            });

        this._map.set(
            "native uint operator^(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() ^ right.loadValue()) >>> 0);
            });

        this._map.set(
            "native uint operator~(uint)",
            func => {
                func.implementation = ([value]) => EPtr.box((~value.loadValue()) >>> 0);
            });

        this._map.set(
            "native uint operator<<(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() << right.loadValue()) >>> 0);
            });

        this._map.set(
            "native uint operator>>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box((left.loadValue() >>> right.loadValue()) >>> 0);
            });

        this._map.set(
            "native float cos(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.cos(value.loadValue())));
            });

        this._map.set(
            "native float sin(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.sin(value.loadValue())));
            });

        this._map.set(
            "native float tan(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.tan(value.loadValue())));
            });

        this._map.set(
            "native float acos(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.acos(value.loadValue())));
            });

        this._map.set(
            "native float asin(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.asin(value.loadValue())));
            });

        this._map.set(
            "native float atan(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.atan(value.loadValue())));
            });

        this._map.set(
            "native float cosh(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.cosh(value.loadValue())));
            });

        this._map.set(
            "native float sinh(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.sinh(value.loadValue())));
            });

        this._map.set(
            "native float tanh(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.tanh(value.loadValue())));
            });

        this._map.set(
            "native float ceil(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.ceil(value.loadValue())));
            });

        this._map.set(
            "native float exp(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.exp(value.loadValue())));
            });

        this._map.set(
            "native float floor(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.floor(value.loadValue())));
            });

        this._map.set(
            "native float log(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.log(value.loadValue())));
            });

        this._map.set(
            "native float round(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.round(value.loadValue())));
            });

        this._map.set(
            "native float sqrt(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.sqrt(value.loadValue())));
            });

        this._map.set(
            "native float trunc(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Math.fround(Math.trunc(value.loadValue())));
            });

        this._map.set(
            "native float ddx(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(0);
            });

        this._map.set(
            "native float ddy(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(0);
            });

        this._map.set(
            "native float pow(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(Math.pow(left.loadValue(), right.loadValue())));
            });

        this._map.set(
            "native bool isfinite(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Number.isFinite(value.loadValue()));
            });

        this._map.set(
            "native bool isinf(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box((value.loadValue() == Number.POSITIVE_INFINITY) || (value.loadValue() == Number.NEGATIVE_INFINITY));
            });

        this._map.set(
            "native bool isnan(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(Number.isNaN(value.loadValue()));
            });

        // FIXME: Implement this.
        this._map.set(
            "native bool isnormal(half)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(isNaN(value.loadValue()));
            });

        // FIXME: Implement this.
        this._map.set(
            "native bool isnormal(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(isNaN(value.loadValue()));
            });

        this._map.set(
            "native float atan2(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(Math.fround(Math.atan2(left.loadValue(), right.loadValue())));
            });

        this._map.set(
            "native int asint(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(bitwiseCast(Float32Array, Int32Array, value.loadValue()));
            });

        this._map.set(
            "native uint asuint(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(bitwiseCast(Float32Array, Uint32Array, value.loadValue()));
            });

        this._map.set(
            "native float asfloat(int)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(bitwiseCast(Int32Array, Float32Array, value.loadValue()));
            });

        this._map.set(
            "native float asfloat(uint)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(bitwiseCast(Uint32Array, Float32Array, value.loadValue()));
            });

        // FIXME: Implement this.
        this._map.set(
            "native float f16tof32(uint)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(0);
            });

        // FIXME: Implement this.
        this._map.set(
            "native uint f32tof16(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(0);
            });

        this._map.set(
            "native void AllMemoryBarrierWithGroupSync()",
            func => {
                func.implementation = function() {};
            });

        this._map.set(
            "native void DeviceMemoryBarrierWithGroupSync()",
            func => {
                func.implementation = function() {};
            });

        this._map.set(
            "native void GroupMemoryBarrierWithGroupSync()",
            func => {
                func.implementation = function() {};
            });

        for (let nativeVectorTypeName of allVectorTypeNames()) {
            this._map.set(`native typedef ${nativeVectorTypeName}`, type => {
                type.isPrimitive = true;
            });
        }

        for (let getter of BuiltinVectorGetter.functions())
            this._map.set(getter.toString(), func => getter.instantiateImplementation(func));

        for (let setter of BuiltinVectorSetter.functions())
            this._map.set(setter.toString(), func => setter.instantiateImplementation(func));

        for (let getter of BuiltinMatrixGetter.functions())
            this._map.set(getter.toString(), func => getter.instantiateImplementation(func));

        for (let setter of BuiltinMatrixSetter.functions())
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

