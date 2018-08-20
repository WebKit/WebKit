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

// NOTE: The next line is line 28, and we rely on this in Prepare.js.
let standardLibrary = `
// This is the WSL standard library. Implementations of all of these things are in
// Intrinsics.js.

// Need to bootstrap void first.
native typedef void;
native typedef bool;
native typedef uchar;
native typedef uint;
native typedef int;
native typedef float;

// FIXME: Add support for these types to Intrinsics.js
// native typedef ushort;
// native typedef char;
// native typedef short;
// native typedef half;
// native typedef atomic_int;
// native typedef atomic_uint;

native operator int(uint);
native operator int(uchar);
native operator int(float);
native operator uint(int);
native operator uint(uchar);
native operator uint(float);
native operator uchar(int);
native operator uchar(uint);
native operator uchar(float);
native operator float(int);
native operator float(uint);
native operator float(uchar);

native int operator+(int, int);
native uint operator+(uint, uint);
uchar operator+(uchar a, uchar b) { return uchar(uint(a) + uint(b)); }
native float operator+(float, float);
int operator++(int value) { return value + 1; }
uint operator++(uint value) { return value + 1; }
uchar operator++(uchar value) { return value + 1; }
native int operator-(int, int);
native uint operator-(uint, uint);
uchar operator-(uchar a, uchar b) { return uchar(uint(a) - uint(b)); }
native float operator-(float, float);
int operator--(int value) { return value - 1; }
uint operator--(uint value) { return value - 1; }
uchar operator--(uchar value) { return value - 1; }
native int operator*(int, int);
native uint operator*(uint, uint);
uchar operator*(uchar a, uchar b) { return uchar(uint(a) * uint(b)); }
native float operator*(float, float);
native int operator/(int, int);
native uint operator/(uint, uint);
uchar operator/(uchar a, uchar b) { return uchar(uint(a) / uint(b)); }
native int operator&(int, int);
native int operator|(int, int);
native int operator^(int, int);
native int operator~(int);
native int operator<<(int, uint);
native int operator>>(int, uint);
native uint operator&(uint, uint);
native uint operator|(uint, uint);
native uint operator^(uint, uint);
native uint operator~(uint);
native uint operator<<(uint, uint);
native uint operator>>(uint, uint);
uchar operator&(uchar a, uchar b) { return uchar(uint(a) & uint(b)); }
uchar operator|(uchar a, uchar b) { return uchar(uint(a) | uint(b)); }
uchar operator^(uchar a, uchar b) { return uchar(uint(a) ^ uint(b)); }
uchar operator~(uchar value) { return uchar(~uint(value)); }
uchar operator<<(uchar a, uint b) { return uchar(uint(a) << (b & 7)); }
uchar operator>>(uchar a, uint b) { return uchar(uint(a) >> (b & 7)); }
native float operator/(float, float);
native bool operator==(int, int);
native bool operator==(uint, uint);
bool operator==(uchar a, uchar b) { return uint(a) == uint(b); }
native bool operator==(bool, bool);
native bool operator==(float, float);
native bool operator<(int, int);
native bool operator<(uint, uint);
bool operator<(uchar a, uchar b) { return uint(a) < uint(b); }
native bool operator<(float, float);
native bool operator<=(int, int);
native bool operator<=(uint, uint);
bool operator<=(uchar a, uchar b) { return uint(a) <= uint(b); }
native bool operator<=(float, float);
native bool operator>(int, int);
native bool operator>(uint, uint);
bool operator>(uchar a, uchar b) { return uint(a) > uint(b); }
native bool operator>(float, float);
native bool operator>=(int, int);
native bool operator>=(uint, uint);
bool operator>=(uchar a, uchar b) { return uint(a) >= uint(b); }
native bool operator>=(float, float);

bool operator&(bool a, bool b)
{
    if (a)
        return b;
    return false;
}

bool operator|(bool a, bool b)
{
    if (a)
        return true;
    if (b)
        return true;
    return false;
}

bool operator^(bool a, bool b)
{
    if (a)
        return !b;
    return b;
}

bool operator~(bool value)
{
    return !value;
}

native typedef uchar2;
native typedef uchar3;
native typedef uchar4;

native typedef uint2;
native typedef uint3;
native typedef uint4;

native typedef int2;
native typedef int3;
native typedef int4;

native typedef float2;
native typedef float3;
native typedef float4;
`;

// FIXME: Once the standard library has been replaced with a new version, this comments should be removed.
// This list is used to restrict the availability of vector types available in the langauge.
// Permissible vector element types must appear in this list and in the standard library
const VectorElementTypes = [ /*"bool",*/ "uchar", /*"char", "ushort", "short",*/ "uint", "int", /* "half", */"float" ];
const VectorElementSizes = [ 2, 3, 4 ];

function allVectorTypeNames()
{
    const names = [];
    for (let elementType of VectorElementTypes) {
        for (let size of VectorElementSizes)
            names.push(`${elementType}${size}`);
    }
    return names;
}

// Provides operator&[]
standardLibrary += OperatorAnderIndexer.functions().join(";\n") + ";\n";

// Native vector types are like structs in the langauge, but they do not have the ander field access.
// It is not possible to take the address of a vector field.
standardLibrary += BuiltinVectorGetter.functions().join(";\n") + ";\n";
standardLibrary += BuiltinVectorSetter.functions().join(";\n") + ";\n";
standardLibrary += BuiltinVectorIndexGetter.functions().join(";\n") + ";\n";
standardLibrary += BuiltinVectorIndexSetter.functions().join(";\n") + ";\n";

// FIXME: For native types these could be included as source in the standard library.
// https://bugs.webkit.org/show_bug.cgi?id=188685
standardLibrary += OperatorBool.functions().join(";\n") + ";\n";
standardLibrary += BuiltinVectorEqualityOperator.functions().join(";\n") + ";\n";

// FIXME: These need to be included as source in the standard library.
standardLibrary += SwizzleOp.functions().join(";\n") + ";\n";
standardLibrary += BuiltinVectorConstructors.functions().join(";\n") + ";\n";
