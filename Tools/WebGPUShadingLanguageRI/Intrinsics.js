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
                type.canRepresent = value => isBitwiseEquivalent(castToUchar(value), value);
                type.size = 1;
                type.defaultValue = 0;
                type.createLiteral = (origin, value) => IntLiteral.withType(origin, castToUchar(value), type);
                type.successorValue = value => castToUchar(value + 1);
                type.valuesEqual = (a, b) => a === b;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                type.formatValueFromIntLiteral = value => castToUchar(value);
                type.formatValueFromUintLiteral = value => castToUchar(value);
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
                 type.canRepresent = value => isBitwiseEquivalent(castToUshort(value), value);
                 type.size = 1;
                 type.defaultValue = 0;
                 type.createLiteral = (origin, value) => IntLiteral.withType(origin, castToUshort(value), type);
                 type.successorValue = value => castToUshort(value + 1);
                 type.valuesEqual = (a, b) => a === b;
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                 type.formatValueFromIntLiteral = value => castToUshort(value);
                 type.formatValueFromUintLiteral = value => castToUshort(value);
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
                 type.canRepresent = value => isBitwiseEquivalent(castToUint(value), value);
                 type.size = 1;
                 type.defaultValue = 0;
                 type.createLiteral = (origin, value) => IntLiteral.withType(origin, castToUint(value), type);
                 type.successorValue = value => castToUint(value + 1);
                 type.valuesEqual = (a, b) => a === b;
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                 type.formatValueFromIntLiteral = value => castToUint(value);
                 type.formatValueFromUintLiteral = value => castToUint(value);
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
                 type.canRepresent = value => isBitwiseEquivalent(castToChar(value), value);
                 type.size = 1;
                 type.defaultValue = 0;
                 type.createLiteral = (origin, value) => IntLiteral.withType(origin, castToChar(value), type);
                 type.successorValue = value => castToChar(value + 1);
                 type.valuesEqual = (a, b) => a === b;
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                 type.formatValueFromIntLiteral = value => castToChar(value);
                 type.formatValueFromUintLiteral = value => castToChar(value);
                 type.allValues = function*() {
                     for (let i = 0; i <= 0xff; ++i) {
                         let value = castToChar(i);
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
                 type.canRepresent = value => isBitwiseEquivalent(castToShort(value), value);
                 type.size = 1;
                 type.defaultValue = 0;
                 type.createLiteral = (origin, value) => IntLiteral.withType(origin, castToShort(value), type);
                 type.successorValue = value => castToShort(value + 1);
                 type.valuesEqual = (a, b) => a === b;
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                 type.formatValueFromIntLiteral = value => castToShort(value);
                 type.formatValueFromUintLiteral = value => castToShort(value);
                 type.allValues = function*() {
                     for (let i = 0; i <= 0xffff; ++i) {
                         let value = castToShort(i);
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
                 type.canRepresent = value => isBitwiseEquivalent(castToInt(value), value);
                 type.size = 1;
                 type.defaultValue = 0;
                 type.createLiteral = (origin, value) => IntLiteral.withType(origin, castToInt(value), type);
                 type.successorValue = value => castToInt(value + 1);
                 type.valuesEqual = (a, b) => a === b;
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
                 type.formatValueFromIntLiteral = value => castToInt(value);
                 type.formatValueFromUintLiteral = value => castToInt(value);
                 type.allValues = function*() {
                     for (let i = 0; i <= 0xffffffff; ++i) {
                         let value = castToInt(i);
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
                 type.formatValueFromFloatLiteral = value => castToFloat(value);
             });

        this._map.set(
             "native typedef atomic_int",
             type => {
                 this.atomic_int = type;
                 type.size = 1;
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
             });

        this._map.set(
             "native typedef atomic_uint",
             type => {
                 this.atomic_uint = type;
                 type.size = 1;
                 type.populateDefaultValue = (buffer, offset) => buffer.set(offset, 0);
             });

        for (let vectorType of VectorElementTypes) {
            for (let vectorSize of VectorElementSizes) {
                this._map.set(`native typedef vector<${vectorType}, ${vectorSize}>`, type => {
                    this[`vector<${vectorType}, ${vectorSize}>`] = type;
                    type.isPrimitive = true;
                });
            }
        }

        for (let typeName of ["half", "float"]) {
            for (let height = 2; height <= 4; ++height) {
                for (let width = 2; width <= 4; ++width) {
                    this._map.set(`native typedef matrix<${typeName}, ${height}, ${width}>`, type => {
                        this[`matrix<${typeName}, ${height}, ${width}>`] = type;
                    });
                }
            }
        }

        this._map.set(
            "native typedef sampler",
            type => {
                this.sampler = type;
                type.size = 1;
                type.populateDefaultValue = (buffer, offset) => buffer.set(offset, {});
            });

        for (let textureType of ["Texture1D", "RWTexture1D", "Texture1DArray", "RWTexture1DArray", "Texture2D", "RWTexture2D", "Texture2DArray", "RWTexture2DArray", "Texture3D", "RWTexture3D", "TextureCube"]) {
            for (let typeArgument of ["bool", "uchar", "ushort", "uint", "char", "short", "int", "half", "float"]) {
                this._map.set(
                    `native typedef ${textureType}<${typeArgument}>`,
                    type => {
                        this[`${textureType}<${typeArgument}>`] = type;
                        type.size = 1;
                        type.isTexture = true;
                        type.populateDefaultValue = (buffer, offset) => buffer.set(offset, {});
                    });
                for (let i = 2; i <= 4; ++i) {
                    this._map.set(
                        `native typedef ${textureType}<${typeArgument}${i}>`,
                        type => {
                            this[`${textureType}<${typeArgument}${i}>`] = type;
                            type.size = 1;
                            type.isTexture = true;
                            type.populateDefaultValue = (buffer, offset) => buffer.set(offset, {});
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
                        type.size = 1;
                        type.populateDefaultValue = (buffer, offset) => buffer.set(offset, {});
                    });
            }
        }

        for (let primitiveType of ["ushort", "uint", "char", "short", "int", "half", "float"]) {
            this._map.set(
                `native operator uchar(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(castToUchar(value.loadValue()));
                });
        }

        for (let primitiveType of ["uchar", "uint", "char", "short", "int", "half", "float"]) {
            this._map.set(
                `native operator ushort(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(castToUshort(value.loadValue()));
                });
        }

        for (let primitiveType of ["uchar", "ushort", "char", "short", "int", "half", "float"]) {
            this._map.set(
                `native operator uint(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(castToUint(value.loadValue()));
                });
        }

        for (let primitiveType of ["uchar", "ushort", "uint", "short", "int", "half", "float"]) {
            this._map.set(
                `native operator char(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(castToChar(value.loadValue()));
                });
        }

        for (let primitiveType of ["uchar", "ushort", "uint", "char", "int", "half", "float"]) {
            this._map.set(
                `native operator short(${primitiveType})`,
                func => {
                    func.implementation = ([value]) => EPtr.box(castToShort(value.loadValue()));
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
                    func.implementation = ([value]) => EPtr.box(castToFloat(value.loadValue()));
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
                    EPtr.box(castToInt(-value.loadValue()));
            });

        this._map.set(
            "native float operator-(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(-value.loadValue()));
            });

        this._map.set(
            "native int operator+(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToInt(left.loadValue() + right.loadValue()));
            });

        this._map.set(
            "native int operator-(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToInt(left.loadValue() - right.loadValue()));
            });

        this._map.set(
            "native int operator*(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToInt(left.loadValue() * right.loadValue()));
            });

        this._map.set(
            "native int operator/(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToInt(left.loadValue() / right.loadValue()));
            });

        this._map.set(
            "native uint operator+(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToUint(left.loadValue() + right.loadValue()));
            });

        this._map.set(
            "native uint operator-(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToUint(left.loadValue() - right.loadValue()));
            });

        this._map.set(
            "native uint operator*(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToUint(left.loadValue() * right.loadValue()));
            });

        this._map.set(
            "native uint operator/(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToUint(left.loadValue() / right.loadValue()));
            });

        this._map.set(
            "native float operator+(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToFloat(left.loadValue() + right.loadValue()));
            });

        this._map.set(
            "native float operator-(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToFloat(left.loadValue() - right.loadValue()));
            });

        this._map.set(
            "native float operator*(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToFloat(left.loadValue() * right.loadValue()));
            });

        this._map.set(
            "native float operator/(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToFloat(left.loadValue() / right.loadValue()));
            });

        this._map.set(
            "native int operator&(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToInt(left.loadValue() & right.loadValue()));
            });

        this._map.set(
            "native int operator|(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToInt(left.loadValue() | right.loadValue()));
            });

        this._map.set(
            "native int operator^(int,int)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToInt(left.loadValue() ^ right.loadValue()));
            });

        this._map.set(
            "native int operator~(int)",
            func => {
                func.implementation = ([value]) => EPtr.box(castToInt(~value.loadValue()));
            });

        this._map.set(
            "native int operator<<(int,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToInt(left.loadValue() << right.loadValue()));
            });

        this._map.set(
            "native int operator>>(int,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToInt(left.loadValue() >> right.loadValue()));
            });

        this._map.set(
            "native uint operator&(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToUint(left.loadValue() & right.loadValue()));
            });

        this._map.set(
            "native uint operator|(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToUint(left.loadValue() | right.loadValue()));
            });

        this._map.set(
            "native uint operator^(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToUint(left.loadValue() ^ right.loadValue()));
            });

        this._map.set(
            "native uint operator~(uint)",
            func => {
                func.implementation = ([value]) => EPtr.box(castToUint(~value.loadValue()));
            });

        this._map.set(
            "native uint operator<<(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToUint(left.loadValue() << right.loadValue()));
            });

        this._map.set(
            "native uint operator>>(uint,uint)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToUint(left.loadValue() >>> right.loadValue()));
            });

        this._map.set(
            "native float cos(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.cos(value.loadValue())));
            });

        this._map.set(
            "native float sin(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.sin(value.loadValue())));
            });

        this._map.set(
            "native float tan(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.tan(value.loadValue())));
            });

        this._map.set(
            "native float acos(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.acos(value.loadValue())));
            });

        this._map.set(
            "native float asin(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.asin(value.loadValue())));
            });

        this._map.set(
            "native float atan(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.atan(value.loadValue())));
            });

        this._map.set(
            "native float cosh(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.cosh(value.loadValue())));
            });

        this._map.set(
            "native float sinh(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.sinh(value.loadValue())));
            });

        this._map.set(
            "native float tanh(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.tanh(value.loadValue())));
            });

        this._map.set(
            "native float ceil(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.ceil(value.loadValue())));
            });

        this._map.set(
            "native float exp(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.exp(value.loadValue())));
            });

        this._map.set(
            "native float floor(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.floor(value.loadValue())));
            });

        this._map.set(
            "native float log(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.log(value.loadValue())));
            });

        this._map.set(
            "native float round(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.round(value.loadValue())));
            });

        this._map.set(
            "native float sqrt(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.sqrt(value.loadValue())));
            });

        this._map.set(
            "native float trunc(float)",
            func => {
                func.implementation = ([value]) =>
                    EPtr.box(castToFloat(Math.trunc(value.loadValue())));
            });

        this._map.set(
            "native fragment float ddx(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(0);
            });

        this._map.set(
            "native fragment float ddy(float)",
            func => {
                func.implementation = ([value]) => EPtr.box(0);
            });

        this._map.set(
            "native float pow(float,float)",
            func => {
                func.implementation = ([left, right]) =>
                    EPtr.box(castToFloat(Math.pow(left.loadValue(), right.loadValue())));
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
                    EPtr.box(castToFloat(Math.atan2(left.loadValue(), right.loadValue())));
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
            "native compute void AllMemoryBarrierWithGroupSync()",
            func => {
                func.implementation = function() {};
            });

        this._map.set(
            "native compute void DeviceMemoryBarrierWithGroupSync()",
            func => {
                func.implementation = function() {};
            });

        this._map.set(
            "native compute void GroupMemoryBarrierWithGroupSync()",
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

        for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
            for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                this._map.set(
                    `native void InterlockedAdd(atomic_uint* ${addressSpace1},uint,uint* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToUint(a + b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedAdd(atomic_int* ${addressSpace1},int,int* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToInt(a + b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedAnd(atomic_uint* ${addressSpace1},uint,uint* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToUint(a & b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedAnd(atomic_int* ${addressSpace1},int,int* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToInt(a & b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedExchange(atomic_uint* ${addressSpace1},uint,uint* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(b), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedExchange(atomic_int* ${addressSpace1},int,int* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(b), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedMax(atomic_uint* ${addressSpace1},uint,uint* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToUint(a > b ? a : b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedMax(atomic_int* ${addressSpace1},int,int* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToInt(a > b ? a : b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedMin(atomic_uint* ${addressSpace1},uint,uint* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToUint(a < b ? a : b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedMin(atomic_int* ${addressSpace1},int,int* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToInt(a < b ? a : b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedOr(atomic_uint* ${addressSpace1},uint,uint* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToUint(a | b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedOr(atomic_int* ${addressSpace1},int,int* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToInt(a | b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedXor(atomic_uint* ${addressSpace1},uint,uint* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToUint(a ^ b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedXor(atomic_int* ${addressSpace1},int,int* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = value.loadValue();
                            let result = castToInt(a ^ b);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                            atomic.loadValue().copyFrom(EPtr.box(result), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedCompareExchange(atomic_uint* ${addressSpace1},uint,uint,uint* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, compareValue, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = compareValue.loadValue();
                            let c = value.loadValue();
                            if (a == b)
                                atomic.loadValue().copyFrom(EPtr.box(c), 1);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                        }
                    });

                this._map.set(
                    `native void InterlockedCompareExchange(atomic_int* ${addressSpace1},int,int,int* ${addressSpace2})`,
                    func => {
                        func.implementation = function([atomic, compareValue, value, originalValue], node) {
                            if (!atomic.loadValue())
                                throw new WTrapError(node.origin.originString, "Null atomic pointer");
                            let a = atomic.loadValue().loadValue();
                            let b = compareValue.loadValue();
                            let c = value.loadValue();
                            if (a == b)
                                atomic.loadValue().copyFrom(EPtr.box(c), 1);
                            if (originalValue.loadValue())
                                originalValue.loadValue().copyFrom(EPtr.box(a), 1);
                        }
                    });
            }
        }

        function checkUndefined(origin, explanation, value)
        {
            if (value == undefined)
                throw new WTrapError(origin, "Texture read out of bounds");
            return value;
        }

        function checkFalse(origin, explanation, value)
        {
            if (value == false)
                throw new WTrapError(origin, "Texture store out of bounds");
        }

        function boxVector(a)
        {
            if (a instanceof Array) {
                let result = new EPtr(new EBuffer(a.length), 0);
                for (let i = 0; i < a.length; ++i)
                    result.set(i, a[i]);
                return result;
            } else
                return EPtr.box(a);
        }

        function unboxVector(a, length)
        {
            if (length != "") {
                length = Number.parseInt(length);
                let result = [];
                for (let i = 0; i < length; ++i)
                    result.push(a.get(i));
                return result;
            } else
                return a.loadValue();
        }

        for (let type of ["uchar", "ushort", "uint", "char", "short", "int", "half", "float"]) {
            for (var lengthVariable of [``, `2`, `3`, `4`]) {
                let length = lengthVariable;
                this._map.set(
                    `native ${type}${length} Sample(Texture1D<${type}${length}>,sampler,float location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.loadValue(), 0, 0, 1, 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Sample(Texture1D<${type}${length}>,sampler,float location,int offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, offset]) {
                            let locationArray = [location.loadValue(), 0, 0, 1, 0];
                            let deltaArray = [offset.loadValue(), 0, 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Load(Texture1D<${type}${length}>,int2 location)`,
                    func => {
                        func.implementation = function ([texture, location], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, location.get(1), 0, 0, location.get(0))));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Load(Texture1D<${type}${length}>,int2 location,int offset)`,
                    func => {
                        func.implementation = function ([texture, location, offset], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, location.get(1), 0, 0, location.get(0) + offset.loadValue())));
                        }
                    });
                for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                        this._map.set(
                            `native void GetDimensions(Texture1D<${type}${length}>,uint MipLevel,uint* ${addressSpace1} Width,uint* ${addressSpace2} NumberOfLevels)`,
                            func => {
                                func.implementation = function([texture, miplevel, width, numberOfLevels], node) {
                                    let tex = texture.loadValue();
                                    let mipID = miplevel.loadValue();
                                    if (mipID >= tex.levelCount)
                                        throw new WTrapError(node.origin.originString, "Reading from nonexistant mip level of texture");
                                    if (width.loadValue())
                                        width.loadValue().copyFrom(EPtr.box(tex.widthAtLevel(mipID)), 1);
                                    if (numberOfLevels.loadValue())
                                        numberOfLevels.loadValue().copyFrom(EPtr.box(tex.levelCount), 1);
                                }
                            });
                    }
                }

                this._map.set(
                    `native ${type}${length} Sample(Texture1DArray<${type}${length}>,sampler,float2 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), 0, 0, 1, location.get(1)];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Sample(Texture1DArray<${type}${length}>,sampler,float2 location,int offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, offset]) {
                            let locationArray = [location.get(0), 0, 0, 1, location.get(1)];
                            let deltaArray = [offset.loadValue(), 0, 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Load(Texture1DArray<${type}${length}>,int3 location)`,
                    func => {
                        func.implementation = function ([texture, location], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(location.get(2), location.get(1), 0, 0, location.get(0))));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Load(Texture1DArray<${type}${length}>,int3 location,int offset)`,
                    func => {
                        func.implementation = function ([texture, location, offset], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(location.get(2), location.get(1), 0, 0, location.get(0) + offset.loadValue())));
                        }
                    });
                for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                        for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                            this._map.set(
                                `native void GetDimensions(Texture1DArray<${type}${length}>,uint MipLevel,uint* ${addressSpace1} Width,uint* ${addressSpace2} Elements,uint* ${addressSpace3} NumberOfLevels)`,
                                func => {
                                    func.implementation = function([texture, miplevel, width, elements, numberOfLevels], node) {
                                        let tex = texture.loadValue();
                                        let mipID = miplevel.loadValue();
                                        if (mipID >= tex.levelCount)
                                            throw new WTrapError(node.origin.originString, "Reading from nonexistant mip level of texture");
                                        if (width.loadValue())
                                            width.loadValue().copyFrom(EPtr.box(tex.widthAtLevel(mipID)), 1);
                                        if (elements.loadValue())
                                            elements.loadValue().copyFrom(EPtr.box(tex.layerCount), 1);
                                        if (numberOfLevels.loadValue())
                                            numberOfLevels.loadValue().copyFrom(EPtr.box(tex.levelCount), 1);
                                    }
                                });
                        }
                    }
                }

                this._map.set(
                    `native ${type}${length} Sample(Texture2D<${type}${length}>,sampler,float2 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Sample(Texture2D<${type}${length}>,sampler,float2 location,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleBias(Texture2D<${type}${length}>,sampler,float2 location,float Bias)`,
                    func => {
                        func.implementation = function([texture, sampler, location, bias]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, bias.loadValue(), undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleBias(Texture2D<${type}${length}>,sampler,float2 location,float Bias,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, bias, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, bias.loadValue(), undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleGrad(Texture2D<${type}${length}>,sampler,float2 location,float2 DDX,float2 DDY)`,
                    func => {
                        func.implementation = function([texture, sampler, location, ddx, ddy]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            let ddxArray = [ddx.get(0), ddx.get(1), 0];
                            let ddyArray = [ddy.get(0), ddy.get(1), 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, ddxArray, ddyArray, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleGrad(Texture2D<${type}${length}>,sampler,float2 location,float2 DDX,float2 DDY,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, ddx, ddy, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            let ddxArray = [ddx.get(0), ddx.get(1), 0];
                            let ddyArray = [ddy.get(0), ddy.get(1), 0];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, ddxArray, ddyArray, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleLevel(Texture2D<${type}${length}>,sampler,float2 location,float LOD)`,
                    func => {
                        func.implementation = function([texture, sampler, location, lod]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, lod.loadValue(), undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleLevel(Texture2D<${type}${length}>,sampler,float2 location,float LOD,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, lod, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, lod.loadValue(), undefined));
                        }
                    });
                this._map.set(
                    `native ${type}4 Gather(Texture2D<${type}${length}>,sampler,float2 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                        }
                    });
                this._map.set(
                    `native ${type}4 Gather(Texture2D<${type}${length}>,sampler,float2 location,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 0));
                        }
                    });
                this._map.set(
                    `native ${type}4 GatherRed(Texture2D<${type}${length}>,sampler,float2 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                        }
                    });
                this._map.set(
                    `native ${type}4 GatherRed(Texture2D<${type}${length}>,sampler,float2 location,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 0));
                        }
                    });
                if (length == "2" || length == "3" || length == "4") {
                    this._map.set(
                        `native ${type}4 GatherGreen(Texture2D<${type}${length}>,sampler,float2 location)`,
                        func => {
                            func.implementation = function([texture, sampler, location]) {
                                let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                                return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 1));
                            }
                        });
                    this._map.set(
                        `native ${type}4 GatherGreen(Texture2D<${type}${length}>,sampler,float2 location,int2 offset)`,
                        func => {
                            func.implementation = function([texture, sampler, location, offset]) {
                                let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                                let deltaArray = [offset.get(0), offset.get(1), 0];
                                return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 1));
                            }
                        });
                    if (length == "3" || length == "4") {
                        this._map.set(
                            `native ${type}4 GatherBlue(Texture2D<${type}${length}>,sampler,float2 location)`,
                            func => {
                                func.implementation = function([texture, sampler, location]) {
                                    let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                                    return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 2));
                                }
                            });
                        this._map.set(
                            `native ${type}4 GatherBlue(Texture2D<${type}${length}>,sampler,float2 location,int2 offset)`,
                            func => {
                                func.implementation = function([texture, sampler, location, offset]) {
                                    let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                                    let deltaArray = [offset.get(0), offset.get(1), 0];
                                    return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 2));
                                }
                            });
                        if (length == "4") {
                            this._map.set(
                                `native ${type}4 GatherAlpha(Texture2D<${type}${length}>,sampler,float2 location)`,
                                func => {
                                    func.implementation = function([texture, sampler, location]) {
                                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 3));
                                    }
                                });
                            this._map.set(
                                `native ${type}4 GatherAlpha(Texture2D<${type}${length}>,sampler,float2 location,int2 offset)`,
                                func => {
                                    func.implementation = function([texture, sampler, location, offset]) {
                                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                                        let deltaArray = [offset.get(0), offset.get(1), 0];
                                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 3));
                                    }
                                });
                        }
                    }
                }
                this._map.set(
                    `native ${type}${length} Load(Texture2D<${type}${length}>,int3 location)`,
                    func => {
                        func.implementation = function ([texture, location], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, location.get(2), 0, location.get(1), location.get(0))));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Load(Texture2D<${type}${length}>,int3 location,int2 offset)`,
                    func => {
                        func.implementation = function ([texture, location, offset], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, location.get(2), 0, location.get(1) + offset.get(1), location.get(0) + offset.get(0))));
                        }
                    });
                for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                        for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                            this._map.set(
                                `native void GetDimensions(Texture2D<${type}${length}>,uint MipLevel,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height,uint* ${addressSpace3} NumberOfLevels)`,
                                func => {
                                    func.implementation = function([texture, miplevel, width, height, numberOfLevels], node) {
                                        let tex = texture.loadValue();
                                        let mipID = miplevel.loadValue();
                                        if (mipID >= tex.levelCount)
                                            throw new WTrapError(node.origin.originString, "Reading from nonexistant mip level of texture");
                                        if (width.loadValue())
                                            width.loadValue().copyFrom(EPtr.box(tex.widthAtLevel(mipID)), 1);
                                        if (height.loadValue())
                                            height.loadValue().copyFrom(EPtr.box(tex.heightAtLevel(mipID)), 1);
                                        if (numberOfLevels.loadValue())
                                            numberOfLevels.loadValue().copyFrom(EPtr.box(tex.levelCount), 1);
                                    }
                                });
                        }
                    }
                }

                this._map.set(
                    `native ${type}${length} Sample(Texture2DArray<${type}${length}>,sampler,float3 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Sample(Texture2DArray<${type}${length}>,sampler,float3 location,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleBias(Texture2DArray<${type}${length}>,sampler,float3 location,float Bias)`,
                    func => {
                        func.implementation = function([texture, sampler, location, bias]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, bias.loadValue(), undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleBias(Texture2DArray<${type}${length}>,sampler,float3 location,float Bias,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, bias, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, bias.loadValue(), undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleGrad(Texture2DArray<${type}${length}>,sampler,float3 location,float2 DDX,float2 DDY)`,
                    func => {
                        func.implementation = function([texture, sampler, location, ddx, ddy]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            let ddxArray = [ddx.get(0), ddx.get(1), 0];
                            let ddyArray = [ddy.get(0), ddy.get(1), 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, ddxArray, ddyArray, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleGrad(Texture2DArray<${type}${length}>,sampler,float3 location,float2 DDX,float2 DDY,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, ddx, ddy, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            let ddxArray = [ddx.get(0), ddx.get(1), 0];
                            let ddyArray = [ddy.get(0), ddy.get(1), 0];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, ddxArray, ddyArray, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleLevel(Texture2DArray<${type}${length}>,sampler,float3 location,float LOD)`,
                    func => {
                        func.implementation = function([texture, sampler, location, lod]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, lod.loadValue(), undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleLevel(Texture2DArray<${type}${length}>,sampler,float3 location,float LOD,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, lod, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, lod.loadValue(), undefined));
                        }
                    });
                this._map.set(
                    `native ${type}4 Gather(Texture2DArray<${type}${length}>,sampler,float3 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                        }
                    });
                this._map.set(
                    `native ${type}4 Gather(Texture2DArray<${type}${length}>,sampler,float3 location,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 0));
                        }
                    });
                this._map.set(
                    `native ${type}4 GatherRed(Texture2DArray<${type}${length}>,sampler,float3 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                        }
                    });
                this._map.set(
                    `native ${type}4 GatherRed(Texture2DArray<${type}${length}>,sampler,float3 location,int2 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, offset]) {
                            let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                            let deltaArray = [offset.get(0), offset.get(1), 0];
                            return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 0));
                        }
                    });
                if (length == "2" || length == "3" || length == "4") {
                    this._map.set(
                        `native ${type}4 GatherGreen(Texture2DArray<${type}${length}>,sampler,float3 location)`,
                        func => {
                            func.implementation = function([texture, sampler, location]) {
                                let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                                return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 1));
                            }
                        });
                    this._map.set(
                        `native ${type}4 GatherGreen(Texture2DArray<${type}${length}>,sampler,float3 location,int2 offset)`,
                        func => {
                            func.implementation = function([texture, sampler, location, offset]) {
                                let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                                let deltaArray = [offset.get(0), offset.get(1), 0];
                                return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 1));
                            }
                        });
                    if (length == "3" || length == "4") {
                        this._map.set(
                            `native ${type}4 GatherBlue(Texture2DArray<${type}${length}>,sampler,float3 location)`,
                            func => {
                                func.implementation = function([texture, sampler, location]) {
                                    let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                                    return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 2));
                                }
                            });
                        this._map.set(
                            `native ${type}4 GatherBlue(Texture2DArray<${type}${length}>,sampler,float3 location,int2 offset)`,
                            func => {
                                func.implementation = function([texture, sampler, location, offset]) {
                                    let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                                    let deltaArray = [offset.get(0), offset.get(1), 0];
                                    return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 2));
                                }
                            });
                        if (length == "4") {
                            this._map.set(
                                `native ${type}4 GatherAlpha(Texture2DArray<${type}${length}>,sampler,float3 location)`,
                                func => {
                                    func.implementation = function([texture, sampler, location]) {
                                    let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                                    return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 3));
                                    }
                                });
                            this._map.set(
                                `native ${type}4 GatherAlpha(Texture2DArray<${type}${length}>,sampler,float3 location,int2 offset)`,
                                func => {
                                    func.implementation = function([texture, sampler, location, offset]) {
                                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                                        let deltaArray = [offset.get(0), offset.get(1), 0];
                                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 4));
                                    }
                                });
                        }
                    }
                }
                this._map.set(
                    `native ${type}${length} Load(Texture2DArray<${type}${length}>,int4 location)`,
                    func => {
                        func.implementation = function ([texture, location], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(location.get(3), location.get(2), 0, location.get(1), location.get(0))));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Load(Texture2DArray<${type}${length}>,int4 location,int2 offset)`,
                    func => {
                        func.implementation = function ([texture, location, offset], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(location.get(3), location.get(2), 0, location.get(1) + offset.get(1), location.get(0) + offset.get(0))));
                        }
                    });
                for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                        for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                            for (let addressSpace4 of ["thread", "threadgroup", "device"]) {
                                this._map.set(
                                    `native void GetDimensions(Texture2DArray<${type}${length}>,uint MipLevel,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height,uint* ${addressSpace3} Elements,uint* ${addressSpace4} NumberOfLevels)`,
                                    func => {
                                        func.implementation = function([texture, miplevel, width, height, elements, numberOfLevels], node) {
                                            let tex = texture.loadValue();
                                            let mipID = miplevel.loadValue();
                                            if (mipID >= tex.levelCount)
                                                throw new WTrapError(node.origin.originString, "Reading from nonexistant mip level of texture");
                                            if (width.loadValue())
                                                width.loadValue().copyFrom(EPtr.box(tex.widthAtLevel(mipID)), 1);
                                            if (height.loadValue())
                                                height.loadValue().copyFrom(EPtr.box(tex.heightAtLevel(mipID)), 1);
                                            if (elements.loadValue())
                                                elements.loadValue().copyFrom(EPtr.box(tex.layerCount), 1);
                                            if (numberOfLevels.loadValue())
                                                numberOfLevels.loadValue().copyFrom(EPtr.box(tex.levelCount), 1);
                                        }
                                    });
                            }
                        }
                    }
                }

                this._map.set(
                    `native ${type}${length} Sample(Texture3D<${type}${length}>,sampler,float3 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Sample(Texture3D<${type}${length}>,sampler,float3 location,int3 offset)`,
                    func => {
                        func.implementation = function([texture, sampler, location, offset]) {
                            let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                            let deltaArray = [offset.get(0), offset.get(1), offset.get(2)];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Load(Texture3D<${type}${length}>,int4 location)`,
                    func => {
                        func.implementation = function ([texture, location], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, location.get(3), location.get(2), location.get(1), location.get(0))));
                        }
                    });
                this._map.set(
                    `native ${type}${length} Load(Texture3D<${type}${length}>,int4 location,int3 offset)`,
                    func => {
                        func.implementation = function ([texture, location, offset], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, location.get(3), location.get(2) + offset.get(2), location.get(1) + offset.get(1), location.get(0) + offset.get(0))));
                        }
                    });
                for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                        for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                            for (let addressSpace4 of ["thread", "threadgroup", "device"]) {
                                this._map.set(
                                    `native void GetDimensions(Texture3D<${type}${length}>,uint MipLevel,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height,uint* ${addressSpace3} Depth,uint* ${addressSpace4} NumberOfLevels)`,
                                    func => {
                                        func.implementation = function([texture, miplevel, width, height, depth, numberOfLevels], node) {
                                            let tex = texture.loadValue();
                                            let mipID = miplevel.loadValue();
                                            if (mipID >= tex.levelCount)
                                                throw new WTrapError(node.origin.originString, "Reading from nonexistant mip level of texture");
                                            if (width.loadValue())
                                                width.loadValue().copyFrom(EPtr.box(tex.widthAtLevel(mipID)), 1);
                                            if (height.loadValue())
                                                height.loadValue().copyFrom(EPtr.box(tex.heightAtLevel(mipID)), 1);
                                            if (depth.loadValue())
                                                depth.loadValue().copyFrom(EPtr.box(tex.depthAtLevel(mipID)), 1);
                                            if (numberOfLevels.loadValue())
                                                numberOfLevels.loadValue().copyFrom(EPtr.box(tex.levelCount), 1);
                                        }
                                    });
                            }
                        }
                    }
                }

                this._map.set(
                    `native ${type}${length} Sample(TextureCube<${type}${length}>,sampler,float3 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleBias(TextureCube<${type}${length}>,sampler,float3 location,float Bias)`,
                    func => {
                        func.implementation = function([texture, sampler, location, bias]) {
                            let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, bias.loadValue(), undefined, undefined, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleGrad(TextureCube<${type}${length}>,sampler,float3 location,float3 DDX,float3 DDY)`,
                    func => {
                        func.implementation = function([texture, sampler, location, ddx, ddy]) {
                            let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                            let ddxArray = [ddx.get(0), ddx.get(1), ddx.get(2)];
                            let ddyArray = [ddy.get(0), ddy.get(1), ddy.get(2)];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, ddxArray, ddyArray, undefined, undefined));
                        }
                    });
                this._map.set(
                    `native ${type}${length} SampleLevel(TextureCube<${type}${length}>,sampler,float3 location,float LOD)`,
                    func => {
                        func.implementation = function([texture, sampler, location, lod]) {
                            let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                            return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, lod.loadValue(), undefined));
                        }
                    });
                this._map.set(
                    `native ${type}4 Gather(TextureCube<${type}${length}>,sampler,float3 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                            return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                        }
                    });
                this._map.set(
                    `native ${type}4 GatherRed(TextureCube<${type}${length}>,sampler,float3 location)`,
                    func => {
                        func.implementation = function([texture, sampler, location]) {
                            let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                            return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                        }
                    });
                if (length == "2" || length == "3" || length == "4") {
                    this._map.set(
                        `native ${type}4 GatherGreen(TextureCube<${type}${length}>,sampler,float3 location)`,
                        func => {
                            func.implementation = function([texture, sampler, location]) {
                                let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                                return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 1));
                            }
                        });
                    if (length == "3" || length == "4") {
                        this._map.set(
                            `native ${type}4 GatherBlue(TextureCube<${type}${length}>,sampler,float3 location)`,
                            func => {
                                func.implementation = function([texture, sampler, location]) {
                                    let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                                    return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 2));
                                }
                            });
                        if (length == "4") {
                            this._map.set(
                                `native ${type}4 GatherAlpha(TextureCube<${type}${length}>,sampler,float3 location)`,
                                func => {
                                    func.implementation = function([texture, sampler, location]) {
                                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 3));
                                    }
                                });
                        }
                    }
                }
                for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                        for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                            this._map.set(
                                `native void GetDimensions(TextureCube<${type}${length}>,uint MipLevel,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height,uint* ${addressSpace3} NumberOfLevels)`,
                                func => {
                                    func.implementation = function([texture, miplevel, width, height, numberOfLevels], node) {
                                        let tex = texture.loadValue();
                                        let mipID = miplevel.loadValue();
                                        if (tex.layerCount != 6)
                                            throw new Error("Cube texture doesn't have 6 faces");
                                        if (mipID >= tex.levelCount)
                                            throw new WTrapError(node.origin.originString, "Reading from nonexistant mip level of texture");
                                        if (width.loadValue())
                                            width.loadValue().copyFrom(EPtr.box(tex.widthAtLevel(mipID)), 1);
                                        if (height.loadValue())
                                            height.loadValue().copyFrom(EPtr.box(tex.heightAtLevel(mipID)), 1);
                                        if (numberOfLevels.loadValue())
                                            numberOfLevels.loadValue().copyFrom(EPtr.box(tex.levelCount), 1);
                                    }
                                });
                        }
                    }
                }

                for (let addressSpace of ["thread", "threadgroup", "device"]) {
                    this._map.set(
                        `native void GetDimensions(RWTexture1D<${type}${length}>,uint* ${addressSpace} Width)`,
                        func => {
                            func.implementation = function([texture, width]) {
                                if (width.loadValue())
                                    width.loadValue().copyFrom(EPtr.box(texture.loadValue().width), 1);
                            }
                        });
                    this._map.set(
                        `native void GetDimensions(RWTexture1D<${type}${length}>,float* ${addressSpace} Width)`,
                        func => {
                            func.implementation = function([texture, width]) {
                                if (width.loadValue())
                                    width.loadValue().copyFrom(EPtr.box(texture.loadValue().width), 1);
                            }
                        });
                }
                this._map.set(
                    `native ${type}${length} Load(RWTexture1D<${type}${length}>,int location)`,
                    func => {
                        func.implementation = function ([texture, location], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, 0, 0, 0, location.loadValue())));
                        }
                    });
                this._map.set(
                    `native void Store(RWTexture1D<${type}${length}>,${type}${length},uint location)`,
                    func => {
                        func.implementation = function ([texture, value, location], node) {
                            checkFalse(node.origin.originString, "Texture write out of bounds", texture.loadValue().setElementChecked(0, 0, 0, 0, location.loadValue(), unboxVector(value, length)));
                        }
                    });

                for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                        this._map.set(
                            `native void GetDimensions(RWTexture1DArray<${type}${length}>,uint* ${addressSpace1} Width,uint* ${addressSpace2} Elements)`,
                            func => {
                                func.implementation = function([texture, width, elements]) {
                                    let tex = texture.loadValue();
                                    if (width.loadValue())
                                        width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                    if (elements.loadValue())
                                        elements.loadValue().copyFrom(EPtr.box(tex.layerCount), 1);
                                }
                            });
                        this._map.set(
                            `native void GetDimensions(RWTexture1DArray<${type}${length}>,float* ${addressSpace1} Width,uint* ${addressSpace2} Elements)`,
                            func => {
                                func.implementation = function([texture, width, elements]) {
                                    let tex = texture.loadValue();
                                    if (width.loadValue())
                                        width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                    if (elements.loadValue())
                                        elements.loadValue().copyFrom(EPtr.box(tex.layerCount), 1);
                                }
                            });
                    }
                }
                this._map.set(
                    `native ${type}${length} Load(RWTexture1DArray<${type}${length}>,int2 location)`,
                    func => {
                        func.implementation = function ([texture, location], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(location.get(1), 0, 0, 0, location.get(0))));
                        }
                    });
                this._map.set(
                    `native void Store(RWTexture1DArray<${type}${length}>,${type}${length},uint2 location)`,
                    func => {
                        func.implementation = function ([texture, value, location], node) {
                            checkFalse(node.origin.originString, "Texture write out of bounds", texture.loadValue().setElementChecked(location.get(1), 0, 0, 0, location.get(0), unboxVector(value, length)));
                        }
                    });

                for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                        this._map.set(
                            `native void GetDimensions(RWTexture2D<${type}${length}>,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height)`,
                            func => {
                                func.implementation = function([texture, width, height]) {
                                    let tex = texture.loadValue();
                                    if (width.loadValue())
                                        width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                    if (height.loadValue())
                                        height.loadValue().copyFrom(EPtr.box(tex.height), 1);
                                }
                            });
                        this._map.set(
                            `native void GetDimensions(RWTexture2D<${type}${length}>,float* ${addressSpace1} Width,float* ${addressSpace2} Height)`,
                            func => {
                                func.implementation = function([texture, width, height]) {
                                    let tex = texture.loadValue();
                                    if (width.loadValue())
                                        width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                    if (height.loadValue())
                                        height.loadValue().copyFrom(EPtr.box(tex.height), 1);
                                }
                            });
                    }
                }
                this._map.set(
                    `native ${type}${length} Load(RWTexture2D<${type}${length}>,int2 location)`,
                    func => {
                        func.implementation = function ([texture, location], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, 0, 0, location.get(1), location.get(0))));
                        }
                    });
                this._map.set(
                    `native void Store(RWTexture2D<${type}${length}>,${type}${length},uint2 location)`,
                    func => {
                        func.implementation = function ([texture, value, location], node) {
                            checkFalse(node.origin.originString, "Texture write out of bounds", texture.loadValue().setElementChecked(0, 0, 0, location.get(1), location.get(0), unboxVector(value, length)));
                        }
                    });

                for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                        for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                            this._map.set(
                                `native void GetDimensions(RWTexture2DArray<${type}${length}>,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height,uint* ${addressSpace3} Elements)`,
                                func => {
                                    func.implementation = function([texture, width, height, elements]) {
                                        let tex = texture.loadValue();
                                        if (width.loadValue())
                                            width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                        if (height.loadValue())
                                            height.loadValue().copyFrom(EPtr.box(tex.height), 1);
                                        if (elements.loadValue())
                                            elements.loadValue().copyFrom(EPtr.box(tex.layerCount), 1);
                                    }
                                });
                            this._map.set(
                                `native void GetDimensions(RWTexture2DArray<${type}${length}>,float* ${addressSpace1} Width,float* ${addressSpace2} Height,float* ${addressSpace3} Elements)`,
                                func => {
                                    func.implementation = function([texture, width, height, elements]) {
                                        let tex = texture.loadValue();
                                        if (width.loadValue())
                                            width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                        if (height.loadValue())
                                            height.loadValue().copyFrom(EPtr.box(tex.height), 1);
                                        if (elements.loadValue())
                                            elements.loadValue().copyFrom(EPtr.box(tex.layerCount), 1);
                                    }
                                });
                        }
                    }
                }
                this._map.set(
                    `native ${type}${length} Load(RWTexture2DArray<${type}${length}>,int3 location)`,
                    func => {
                        func.implementation = function ([texture, location], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(location.get(2), 0, 0, location.get(1), location.get(0))));
                        }
                    });
                this._map.set(
                    `native void Store(RWTexture2DArray<${type}${length}>,${type}${length},uint3 location)`,
                    func => {
                        func.implementation = function ([texture, value, location], node) {
                            checkFalse(node.origin.originString, "Texture write out of bounds", texture.loadValue().setElementChecked(location.get(2), 0, 0, location.get(1), location.get(0), unboxVector(value, length)));
                        }
                    });

                for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                        for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                             this._map.set(
                                `native void GetDimensions(RWTexture3D<${type}${length}>,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height,uint* ${addressSpace3} Depth)`,
                                func => {
                                    func.implementation = function([texture, width, height, depth]) {
                                        let tex = texture.loadValue();
                                        if (width.loadValue())
                                            width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                        if (height.loadValue())
                                            height.loadValue().copyFrom(EPtr.box(tex.height), 1);
                                        if (depth.loadValue())
                                            depth.loadValue().copyFrom(EPtr.box(tex.depth), 1);
                                    }
                                });
                            this._map.set(
                                `native void GetDimensions(RWTexture3D<${type}${length}>,float* ${addressSpace1} Width,float* ${addressSpace2} Height,float* ${addressSpace3} Depth)`,
                                func => {
                                    func.implementation = function([texture, width, height, depth]) {
                                        let tex = texture.loadValue();
                                        if (width.loadValue())
                                            width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                        if (height.loadValue())
                                            height.loadValue().copyFrom(EPtr.box(tex.height), 1);
                                        if (depth.loadValue())
                                            depth.loadValue().copyFrom(EPtr.box(tex.depth), 1);
                                    }
                                });
                        }
                    }
                }
                this._map.set(
                    `native ${type}${length} Load(RWTexture3D<${type}${length}>,int3 location)`,
                    func => {
                        func.implementation = function ([texture, location], node) {
                            return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, 0, location.get(2), location.get(1), location.get(0))));
                        }
                    });
                this._map.set(
                    `native void Store(RWTexture3D<${type}${length}>,${type}${length},uint3 location)`,
                    func => {
                        func.implementation = function ([texture, value, location], node) {
                            checkFalse(node.origin.originString, "Texture write out of bounds", texture.loadValue().setElementChecked(0, 0, location.get(2), location.get(1), location.get(0), unboxVector(value, length)));
                        }
                    });
            }
        }

        for (let type of ["half", "float"]) {
            this._map.set(
                `native ${type} Sample(TextureDepth2D<${type}>,sampler,float2 location)`,
                func => {
                    func.implementation = function ([texture, sampler, location]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} Sample(TextureDepth2D<${type}>,sampler,float2 location,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleCmp(TextureDepth2D<${type}>,sampler,float2 location,${type} CompareValue)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, compareValue.loadValue()));
                    }
                });
            this._map.set(
                `native ${type} SampleCmp(TextureDepth2D<${type}>,sampler,float2 location,${type} CompareValue,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, compareValue.loadValue()));
                    }
                });
            this._map.set(
                `native ${type} SampleCmpLevelZero(TextureDepth2D<${type}>,sampler,float2 location,${type} CompareValue)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, 0, compareValue.loadValue()));
                    }
                });
            this._map.set(
                `native ${type} SampleCmpLevelZero(TextureDepth2D<${type}>,sampler,float2 location,${type} CompareValue,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, 0, compareValue.loadValue()));
                    }
                });
            this._map.set(
                `native ${type} SampleBias(TextureDepth2D<${type}>,sampler,float2 location,float Bias)`,
                func => {
                    func.implementation = function ([texture, sampler, location, bias]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, bias.loadValue(), undefined, undefined, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleBias(TextureDepth2D<${type}>,sampler,float2 location,float Bias,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, bias, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, bias.loadValue(), undefined, undefined, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleGrad(TextureDepth2D<${type}>,sampler,float2 location,float2 DDX,float2 DDY)`,
                func => {
                    func.implementation = function ([texture, sampler, location, ddx, ddy]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let ddxArray = [ddx.get(0), ddx.get(1), 0];
                        let ddyArray = [ddy.get(0), ddy.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, ddxArray, ddyArray, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleGrad(TextureDepth2D<${type}>,sampler,float2 location,float2 DDX,float2 DDY,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, ddx, ddy, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let ddxArray = [ddx.get(0), ddx.get(1), 0];
                        let ddyArray = [ddy.get(0), ddy.get(1), 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, ddxArray, ddyArray, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleLevel(TextureDepth2D<${type}>,sampler,float2 location,float LOD)`,
                func => {
                    func.implementation = function ([texture, sampler, location, lod]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, lod.loadValue(), undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleLevel(TextureDepth2D<${type}>,sampler,float2 location,float LOD,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, lod, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, lod.loadValue(), undefined));
                    }
                });
            this._map.set(
                `native ${type}4 Gather(TextureDepth2D<${type}>,sampler,float2 location)`,
                func => {
                    func.implementation = function ([texture, sampler, location]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                    }
                });
            this._map.set(
                `native ${type}4 Gather(TextureDepth2D<${type}>,sampler,float2 location,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherRed(TextureDepth2D<${type}>,sampler,float2 location)`,
                func => {
                    func.implementation = function ([texture, sampler, location]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherRed(TextureDepth2D<${type}>,sampler,float2 location,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherCmp(TextureDepth2D<${type}>,sampler,float2 location,float compare_value)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, compareValue.loadValue(), 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherCmp(TextureDepth2D<${type}>,sampler,float2 location,float compare_value,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, compareValue.loadValue(), 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherCmpRed(TextureDepth2D<${type}>,sampler,float2 location,float compare_value)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, compareValue.loadValue(), 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherCmpRed(TextureDepth2D<${type}>,sampler,float2 location,float compare_value,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, compareValue.loadValue(), 0));
                    }
                });
            this._map.set(
                `native ${type} Load(TextureDepth2D<${type}>,int3 location)`,
                func => {
                    func.implementation = function ([texture, location], node) {
                        return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, location.get(2), 0, location.get(1), location.get(0))));
                    }
                });
            this._map.set(
                `native ${type} Load(TextureDepth2D<${type}>,int3 location,int2 offset)`,
                func => {
                    func.implementation = function ([texture, location, offset], node) {
                        return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, location.get(2), 0, location.get(1) + offset.get(1), location.get(0) + offset.get(0))));
                    }
                });
            for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                        this._map.set(
                            `native void GetDimensions(TextureDepth2D<${type}>,uint MipLevel,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height,uint* ${addressSpace3} NumberOfLevels)`,
                            func => {
                                func.implementation = function ([texture, miplevel, width, height, numberOfLevels], node) {
                                    let tex = texture.loadValue();
                                    let mipID = miplevel.loadValue();
                                    if (mipID >= tex.levelCount)
                                        throw new WTrapError(node.origin.originString, "Reading from nonexistant mip level of texture");
                                    if (width.loadValue())
                                        width.loadValue().copyFrom(EPtr.box(tex.widthAtLevel(mipID)), 1);
                                    if (height.loadValue())
                                        height.loadValue().copyFrom(EPtr.box(tex.heightAtLevel(mipID)), 1);
                                    if (numberOfLevels.loadValue())
                                        numberOfLevels.loadValue().copyFrom(EPtr.box(tex.levelCount), 1);
                                }
                            });
                    }
                }
            }
            this._map.set(
                `native ${type} Sample(TextureDepth2DArray<${type}>,sampler,float3 location)`,
                func => {
                    func.implementation = function ([texture, sampler, location]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} Sample(TextureDepth2DArray<${type}>,sampler,float3 location,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleCmp(TextureDepth2DArray<${type}>,sampler,float3 location,${type} CompareValue)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, compareValue.loadValue()));
                    }
                });
            this._map.set(
                `native ${type} SampleCmp(TextureDepth2DArray<${type}>,sampler,float3 location,${type} CompareValue,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, compareValue.loadValue()));
                    }
                });
            this._map.set(
                `native ${type} SampleCmpLevelZero(TextureDepth2DArray<${type}>,sampler,float3 location,${type} CompareValue)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, 0, compareValue.loadValue()));
                    }
                });
            this._map.set(
                `native ${type} SampleCmpLevelZero(TextureDepth2DArray<${type}>,sampler,float3 location,${type} CompareValue,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, 0, compareValue.loadValue()));
                    }
                });
            this._map.set(
                `native ${type} SampleBias(TextureDepth2DArray<${type}>,sampler,float3 location,float Bias)`,
                func => {
                    func.implementation = function ([texture, sampler, location, bias]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, bias.loadValue(), undefined, undefined, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleBias(TextureDepth2DArray<${type}>,sampler,float3 location,float Bias,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, bias, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, bias.loadValue(), undefined, undefined, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleGrad(TextureDepth2DArray<${type}>,sampler,float3 location,float2 DDX,float2 DDY)`,
                func => {
                    func.implementation = function ([texture, sampler, location, ddx, ddy]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let ddxArray = [ddx.get(0), ddx.get(1), 0];
                        let ddyArray = [ddy.get(0), ddy.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, ddxArray, ddyArray, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleGrad(TextureDepth2DArray<${type}>,sampler,float3 location,float2 DDX,float2 DDY,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, ddx, ddy, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let ddxArray = [ddx.get(0), ddx.get(1), 0];
                        let ddyArray = [ddy.get(0), ddy.get(1), 0];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, ddxArray, ddyArray, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleLevel(TextureDepth2DArray<${type}>,sampler,float3 location,float LOD)`,
                func => {
                    func.implementation = function ([texture, sampler, location, lod]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, lod.loadValue(), undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleLevel(TextureDepth2DArray<${type}>,sampler,float3 location,float LOD,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, lod, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, lod.loadValue(), undefined));
                    }
                });
            this._map.set(
                `native ${type}4 Gather(TextureDepth2DArray<${type}>,sampler,float3 location)`,
                func => {
                    func.implementation = function ([texture, sampler, location]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                    }
                });
            this._map.set(
                `native ${type}4 Gather(TextureDepth2DArray<${type}>,sampler,float3 location,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherRed(TextureDepth2DArray<${type}>,sampler,float3 location)`,
                func => {
                    func.implementation = function ([texture, sampler, location]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherRed(TextureDepth2DArray<${type}>,sampler,float3 location,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, undefined, 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherCmp(TextureDepth2DArray<${type}>,sampler,float3 location,float compare_value)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, compareValue.loadValue(), 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherCmp(TextureDepth2DArray<${type}>,sampler,float3 location,float compare_value,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, compareValue.loadValue(), 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherCmpRed(TextureDepth2DArray<${type}>,sampler,float3 location,float compare_value)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, compareValue.loadValue(), 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherCmpRed(TextureDepth2DArray<${type}>,sampler,float3 location,float compare_value,int2 offset)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue, offset]) {
                        let locationArray = [location.get(0), location.get(1), 0, 1, location.get(2)];
                        let deltaArray = [offset.get(0), offset.get(1), 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, deltaArray, undefined, undefined, undefined, undefined, compareValue.loadValue(), 0));
                    }
                });
            this._map.set(
                `native ${type} Load(TextureDepth2DArray<${type}>,int4 location)`,
                func => {
                    func.implementation = function ([texture, location], node) {
                        return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(location.get(3), location.get(2), 0, location.get(1), location.get(0))));
                    }
                });
            this._map.set(
                `native ${type} Load(TextureDepth2DArray<${type}>,int4 location,int2 offset)`,
                func => {
                    func.implementation = function ([texture, location, offset], node) {
                        return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(location.get(3), location.get(2), 0, location.get(1) + offset.get(1), location.get(0) + offset.get(0))));
                    }
                });
            for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                        for (let addressSpace4 of ["thread", "threadgroup", "device"]) {
                            this._map.set(
                                `native void GetDimensions(TextureDepth2DArray<${type}>,uint MipLevel,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height,uint* ${addressSpace3} Elements,uint* ${addressSpace4} NumberOfLevels)`,
                                func => {
                                    func.implementation = function ([texture, miplevel, width, height, elements, numberOfLevels], node) {
                                        let tex = texture.loadValue();
                                        let mipID = miplevel.loadValue();
                                        if (mipID >= tex.levelCount)
                                            throw new WTrapError(node.origin.originString, "Reading from nonexistant mip level of texture");
                                        if (width.loadValue())
                                            width.loadValue().copyFrom(EPtr.box(tex.widthAtLevel(mipID)), 1);
                                        if (height.loadValue())
                                            height.loadValue().copyFrom(EPtr.box(tex.heightAtLevel(mipID)), 1);
                                        if (elements.loadValue())
                                            elements.loadValue().copyFrom(EPtr.box(tex.layerCount), 1);
                                        if (numberOfLevels.loadValue())
                                            numberOfLevels.loadValue().copyFrom(EPtr.box(tex.levelCount), 1);
                                    }
                                });
                        }
                    }
                }
            }
            this._map.set(
                `native ${type} Sample(TextureDepthCube<${type}>,sampler,float3 location)`,
                func => {
                    func.implementation = function ([texture, sampler, location]) {
                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleCmp(TextureDepthCube<${type}>,sampler,float3 location,${type} CompareValue)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, compareValue.loadValue()));
                    }
                });
            this._map.set(
                `native ${type} SampleCmpLevelZero(TextureDepthCube<${type}>,sampler,float3 location,${type} CompareValue)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, 0, compareValue.loadValue()));
                    }
                });
            this._map.set(
                `native ${type} SampleBias(TextureDepthCube<${type}>,sampler,float3 location,float Bias)`,
                func => {
                    func.implementation = function ([texture, sampler, location, bias]) {
                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, bias.loadValue(), undefined, undefined, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleGrad(TextureDepthCube<${type}>,sampler,float3 location,float3 DDX,float3 DDY)`,
                func => {
                    func.implementation = function ([texture, sampler, location, ddx, ddy]) {
                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                        let ddxArray = [ddx.get(0), ddx.get(1), ddx.get(2)];
                        let ddyArray = [ddy.get(0), ddy.get(1), ddy.get(2)];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, ddxArray, ddyArray, undefined, undefined));
                    }
                });
            this._map.set(
                `native ${type} SampleLevel(TextureDepthCube<${type}>,sampler,float3 location,float LOD)`,
                func => {
                    func.implementation = function ([texture, sampler, location, lod]) {
                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                        return boxVector(sampleTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, lod.loadValue(), undefined));
                    }
                });
            this._map.set(
                `native ${type}4 Gather(TextureDepthCube<${type}>,sampler,float3 location)`,
                func => {
                    func.implementation = function ([texture, sampler, location]) {
                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherRed(TextureDepthCube<${type}>,sampler,float3 location)`,
                func => {
                    func.implementation = function ([texture, sampler, location]) {
                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, undefined, 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherCmp(TextureDepthCube<${type}>,sampler,float3 location,float compare_value)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, compareValue.loadValue(), 0));
                    }
                });
            this._map.set(
                `native ${type}4 GatherCmpRed(TextureDepthCube<${type}>,sampler,float3 location,float compare_value)`,
                func => {
                    func.implementation = function ([texture, sampler, location, compareValue]) {
                        let locationArray = [location.get(0), location.get(1), location.get(2), 1, 0];
                        return boxVector(gatherTexture(texture.loadValue(), sampler.loadValue(), locationArray, undefined, undefined, undefined, undefined, undefined, compareValue.loadValue(), 0));
                    }
                });
            for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                        this._map.set(
                            `native void GetDimensions(TextureDepthCube<${type}>,uint MipLevel,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height,uint* ${addressSpace3} NumberOfLevels)`,
                            func => {
                                func.implementation = function ([texture, miplevel, width, height, numberOfLevels], node) {
                                    let tex = texture.loadValue();
                                    let mipID = miplevel.loadValue();
                                    if (tex.layerCount != 6)
                                        throw new Error("Cube texture doesn't have 6 faces");
                                    if (mipID >= tex.levelCount)
                                        throw new WTrapError(node.origin.originString, "Reading from nonexistant mip level of texture");
                                    if (width.loadValue())
                                        width.loadValue().copyFrom(EPtr.box(tex.widthAtLevel(mipID)), 1);
                                    if (height.loadValue())
                                        height.loadValue().copyFrom(EPtr.box(tex.heightAtLevel(mipID)), 1);
                                    if (numberOfLevels.loadValue())
                                        numberOfLevels.loadValue().copyFrom(EPtr.box(tex.levelCount), 1);
                                }
                            });
                    }
                }
            }
            for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                    this._map.set(
                        `native void GetDimensions(RWTextureDepth2D<${type}>,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height)`,
                        func => {
                            func.implementation = function ([texture, width, height]) {
                                let tex = texture.loadValue();
                                if (width.loadValue())
                                    width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                if (height.loadValue())
                                    height.loadValue().copyFrom(EPtr.box(tex.height), 1);
                            }
                        });
                    this._map.set(
                        `native void GetDimensions(RWTextureDepth2D<${type}>,float* ${addressSpace1} Width,float* ${addressSpace2} Height)`,
                        func => {
                            func.implementation = function ([texture, width, height]) {
                                let tex = texture.loadValue();
                                if (width.loadValue())
                                    width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                if (height.loadValue())
                                    height.loadValue().copyFrom(EPtr.box(tex.height), 1);
                            }
                        });
                }
            }
            this._map.set(
                `native ${type} Load(RWTextureDepth2D<${type}>,int2 location)`,
                func => {
                    func.implementation = function ([texture, location], node) {
                        return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(0, 0, 0, location.get(1), location.get(0))));
                    }
                });
            this._map.set(
                `native void Store(RWTextureDepth2D<${type}>,${type},uint2 location)`,
                func => {
                    func.implementation = function ([texture, value, location], node) {
                        checkFalse(node.origin.originString, "Texture write out of bounds", texture.loadValue().setElementChecked(0, 0, 0, location.get(1), location.get(0), unboxVector(value, "")));
                    }
                });
            for (let addressSpace1 of ["thread", "threadgroup", "device"]) {
                for (let addressSpace2 of ["thread", "threadgroup", "device"]) {
                    for (let addressSpace3 of ["thread", "threadgroup", "device"]) {
                        this._map.set(
                            `native void GetDimensions(RWTextureDepth2DArray<${type}>,uint* ${addressSpace1} Width,uint* ${addressSpace2} Height,uint* ${addressSpace3} Elements)`,
                            func => {
                                func.implementation = function ([texture, width, height, elements]) {
                                    let tex = texture.loadValue();
                                    if (width.loadValue())
                                        width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                    if (height.loadValue())
                                        height.loadValue().copyFrom(EPtr.box(tex.height), 1);
                                    if (elements.loadValue())
                                        elements.loadValue().copyFrom(EPtr.box(tex.layerCount), 1);
                                }
                            });
                        this._map.set(
                            `native void GetDimensions(RWTextureDepth2DArray<${type}>,float* ${addressSpace1} Width,float* ${addressSpace2} Height,float* ${addressSpace3} Elements)`,
                            func => {
                                func.implementation = function ([texture, width, height, elements]) {
                                    let tex = texture.loadValue();
                                    if (width.loadValue())
                                        width.loadValue().copyFrom(EPtr.box(tex.width), 1);
                                    if (height.loadValue())
                                        height.loadValue().copyFrom(EPtr.box(tex.height), 1);
                                    if (elements.loadValue())
                                        elements.loadValue().copyFrom(EPtr.box(tex.layerCount), 1);
                                }
                            });
                    }
                }
            }
            this._map.set(
                `native ${type} Load(RWTextureDepth2DArray<${type}>,int3 location)`,
                func => {
                    func.implementation = function ([texture, location], node) {
                        return boxVector(checkUndefined(node.origin.originString, "Texture read out of bounds", texture.loadValue().elementChecked(location.get(2), 0, 0, location.get(1), location.get(0))));
                    }
                });
            this._map.set(
                `native void Store(RWTextureDepth2DArray<${type}>,${type},uint3 location)`,
                func => {
                    func.implementation = function ([texture, value, location], node) {
                        checkFalse(node.origin.originString, "Texture write out of bounds", texture.loadValue().setElementChecked(location.get(2), 0, 0, location.get(1), location.get(0), unboxVector(value, "")));
                    }
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

