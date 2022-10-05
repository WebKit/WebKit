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

// NOTE: The next line is line 28, and we rely on this in Prepare.js.
let standardLibrary = `
// This is the WSL standard library. Implementations of all of these things are in
// Intrinsics.js.

// Need to bootstrap void first.
native typedef void;

native typedef uint8;
native typedef int32;
native typedef uint32;
native typedef bool;
typedef int = int32;
typedef uint = uint32;

native typedef float32;
native typedef float64;
typedef float = float32;
typedef double = float64;

native operator int32(uint32);
native operator int32(uint8);
native operator int32(float);
native operator int32(double);
native operator uint32(int32);
native operator uint32(uint8);
native operator uint32(float);
native operator uint32(double);
native operator uint8(int32);
native operator uint8(uint32);
native operator uint8(float);
native operator uint8(double);
native operator float(int32);
native operator float(uint32);
native operator float(uint8);
native operator float(double);
native operator double(float);
native operator double(int32);
native operator double(uint32);
native operator double(uint8);

native int operator+(int, int);
native uint operator+(uint, uint);
uint8 operator+(uint8 a, uint8 b) { return uint8(uint(a) + uint(b)); }
native float operator+(float, float);
native double operator+(double, double);
int operator++(int value) { return value + 1; }
uint operator++(uint value) { return value + 1; }
uint8 operator++(uint8 value) { return value + 1; }
native int operator-(int, int);
native uint operator-(uint, uint);
uint8 operator-(uint8 a, uint8 b) { return uint8(uint(a) - uint(b)); }
native float operator-(float, float);
native double operator-(double, double);
int operator--(int value) { return value - 1; }
uint operator--(uint value) { return value - 1; }
uint8 operator--(uint8 value) { return value - 1; }
native int operator*(int, int);
native uint operator*(uint, uint);
uint8 operator*(uint8 a, uint8 b) { return uint8(uint(a) * uint(b)); }
native float operator*(float, float);
native double operator*(double, double);
native int operator/(int, int);
native uint operator/(uint, uint);
uint8 operator/(uint8 a, uint8 b) { return uint8(uint(a) / uint(b)); }
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
uint8 operator&(uint8 a, uint8 b) { return uint8(uint(a) & uint(b)); }
uint8 operator|(uint8 a, uint8 b) { return uint8(uint(a) | uint(b)); }
uint8 operator^(uint8 a, uint8 b) { return uint8(uint(a) ^ uint(b)); }
uint8 operator~(uint8 value) { return uint8(~uint(value)); }
uint8 operator<<(uint8 a, uint b) { return uint8(uint(a) << (b & 7)); }
uint8 operator>>(uint8 a, uint b) { return uint8(uint(a) >> (b & 7)); }
native float operator/(float, float);
native double operator/(double, double);
native bool operator==(int, int);
native bool operator==(uint, uint);
bool operator==(uint8 a, uint8 b) { return uint(a) == uint(b); }
native bool operator==(bool, bool);
native bool operator==(float, float);
native bool operator==(double, double);
native bool operator<(int, int);
native bool operator<(uint, uint);
bool operator<(uint8 a, uint8 b) { return uint(a) < uint(b); }
native bool operator<(float, float);
native bool operator<(double, double);
native bool operator<=(int, int);
native bool operator<=(uint, uint);
bool operator<=(uint8 a, uint8 b) { return uint(a) <= uint(b); }
native bool operator<=(float, float);
native bool operator<=(double, double);
native bool operator>(int, int);
native bool operator>(uint, uint);
bool operator>(uint8 a, uint8 b) { return uint(a) > uint(b); }
native bool operator>(float, float);
native bool operator>(double, double);
native bool operator>=(int, int);
native bool operator>=(uint, uint);
bool operator>=(uint8 a, uint8 b) { return uint(a) >= uint(b); }
native bool operator>=(float, float);
native bool operator>=(double, double);

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

protocol Addable {
    Addable operator+(Addable, Addable);
}

protocol Equatable {
    bool operator==(Equatable, Equatable);
}

restricted operator<T> T()
{
    T defaultValue;
    return defaultValue;
}

restricted operator<T> T(T x)
{
    return x;
}

operator<T:Equatable> bool(T x)
{
    return x != T();
}

struct vec2<T> {
    T x;
    T y;
}

typedef int2 = vec2<int>;
typedef uint2 = vec2<uint>;
typedef float2 = vec2<float>;
typedef double2 = vec2<double>;

operator<T> vec2<T>(T x, T y)
{
    vec2<T> result;
    result.x = x;
    result.y = y;
    return result;
}

bool operator==<T:Equatable>(vec2<T> a, vec2<T> b)
{
    return a.x == b.x && a.y == b.y;
}

thread T* operator&[]<T>(thread vec2<T>* foo, uint index)
{
    if (index == 0)
        return &foo->x;
    if (index == 1)
        return &foo->y;
    trap;
}

struct vec3<T> {
    T x;
    T y;
    T z;
}

typedef int3 = vec3<int>;
typedef uint3 = vec3<uint>;
typedef float3 = vec3<float>;
typedef double3 = vec3<double>;

operator<T> vec3<T>(T x, T y, T z)
{
    vec3<T> result;
    result.x = x;
    result.y = y;
    result.z = z;
    return result;
}

operator<T> vec3<T>(vec2<T> v2, T z)
{
    vec3<T> result;
    result.x = v2.x;
    result.y = v2.y;
    result.z = z;
    return result;
}

operator<T> vec3<T>(T x, vec2<T> v2)
{
    vec3<T> result;
    result.x = x;
    result.y = v2.x;
    result.z = v2.y;
    return result;
}

bool operator==<T:Equatable>(vec3<T> a, vec3<T> b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

thread T* operator&[]<T>(thread vec3<T>* foo, uint index)
{
    if (index == 0)
        return &foo->x;
    if (index == 1)
        return &foo->y;
    if (index == 2)
        return &foo->z;
    trap;
}

struct vec4<T> {
    T x;
    T y;
    T z;
    T w;
}

typedef int4 = vec4<int>;
typedef uint4 = vec4<uint>;
typedef float4 = vec4<float>;
typedef double4 = vec4<double>;

operator<T> vec4<T>(T x, T y, T z, T w)
{
    vec4<T> result;
    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;
    return result;
}

operator<T> vec4<T>(vec2<T> v2, T z, T w)
{
    vec4<T> result;
    result.x = v2.x;
    result.y = v2.y;
    result.z = z;
    result.w = w;
    return result;
}

operator<T> vec4<T>(T x, vec2<T> v2, T w)
{
    vec4<T> result;
    result.x = x;
    result.y = v2.x;
    result.z = v2.y;
    result.w = w;
    return result;
}

operator<T> vec4<T>(T x, T y, vec2<T> v2)
{
    vec4<T> result;
    result.x = x;
    result.y = y;
    result.z = v2.x;
    result.w = v2.y;
    return result;
}

operator<T> vec4<T>(vec2<T> v2a, vec2<T> v2b)
{
    vec4<T> result;
    result.x = v2a.x;
    result.y = v2a.y;
    result.z = v2b.x;
    result.w = v2b.y;
    return result;
}

operator<T> vec4<T>(vec3<T> v3, T w)
{
    vec4<T> result;
    result.x = v3.x;
    result.y = v3.y;
    result.z = v3.z;
    result.w = w;
    return result;
}

operator<T> vec4<T>(T x, vec3<T> v3)
{
    vec4<T> result;
    result.x = x;
    result.y = v3.x;
    result.z = v3.y;
    result.w = v3.z;
    return result;
}

bool operator==<T:Equatable>(vec4<T> a, vec4<T> b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

thread T* operator&[]<T>(thread vec4<T>* foo, uint index)
{
    if (index == 0)
        return &foo->x;
    if (index == 1)
        return &foo->y;
    if (index == 2)
        return &foo->z;
    if (index == 3)
        return &foo->w;
    trap;
}

native thread T* operator&[]<T>(thread T[], uint);
native threadgroup T* operator&[]<T>(threadgroup T[], uint);
native device T* operator&[]<T>(device T[], uint);
native constant T* operator&[]<T>(constant T[], uint);

native uint operator.length<T>(thread T[]);
native uint operator.length<T>(threadgroup T[]);
native uint operator.length<T>(device T[]);
native uint operator.length<T>(constant T[]);

uint operator.length<T, uint length>(T[length])
{
    return length;
}
`;

function intToString(x)
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

// There are 481 swizzle operators. Let's not list them explicitly.
function _generateSwizzle(maxDepth, maxItems, array)
{
    if (!array)
        array = [];
    if (array.length == maxDepth) {
        let result = `vec${array.length}<T> operator.${array.join("")}<T>(vec${maxItems}<T> v)
{
    vec${array.length}<T> result;
`;
        for (let i = 0; i < array.length; ++i) {
            result += `    result.${intToString(i)} = v.${array[i]};
`;
        }
        result += `    return result;
}
`;
        return result;
    }
    let result = "";
    for (let i = 0; i < maxItems; ++i) {
        array.push(intToString(i));
        result += _generateSwizzle(maxDepth, maxItems, array);
        array.pop();
    }
    return result;
}

for (let maxDepth = 2; maxDepth <= 4; maxDepth++) {
    for (let maxItems = 2; maxItems <= 4; maxItems++)
        standardLibrary += _generateSwizzle(maxDepth, maxItems);
}
