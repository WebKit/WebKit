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

function castToBool(value)
{
    return !!value;
}

function castToUchar(number)
{
    return number & 0xFF;
}

function castToUshort(number)
{
    return number & 0xFFFF;
}

function castToUint(number)
{
    return number >>> 0;
}

function castToChar(number)
{
    return cast(Int8Array, number);
}

function castToShort(number)
{
    return cast(Int16Array, number);
}

function castToInt(number)
{
    return number | 0;
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

function castToFloat(number)
{
    return Math.fround(number);
}

function castAndCheckValue(castFunction, value)
{
    const castedValue = castFunction(value);
    if (!isBitwiseEquivalent(castedValue, value))
        throw new Error(`${value} was casted and yielded ${castedValue}, which is not bitwise equivalent.`);
    return castedValue;
}

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
