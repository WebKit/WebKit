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

function mslNativeFunctionCall(node, resultVariable, args)
{
    const key = node.toString();

    // FIXME: Implement the sampling functions.
    // FIXME: Implement functions like f16tof32, asfloat, etc.
    // FIXME: Implement tests for all native functions https://bugs.webkit.org/show_bug.cgi?id=189535.
    const functionsWithTheSameCallingConvention = {
        "native bool isfinite(float)" : "isfinite",
        "native bool isinf(float)" : "isinf",
        "native bool isnormal(float)" : "isnormal",
        "native bool isnormal(half)" : "isnormal",
        "native float acos(float)" : "acos",
        "native float asfloat(int)" : "static_cast<float>",
        "native float asfloat(uint)" : "static_cast<float>",
        "native int asint(float)" : "static_cast<int>",
        "native uint asuint(float)" : "static_cast<uint>",
        "native float asin(float)" : "asin",
        "native float atan(float)" : "atan",
        "native float atan2(float,float)" : "atan2",
        "native float ceil(float)" : "ceil",
        "native float cos(float)" : "cos",
        "native float cosh(float)" : "cosh",
        "native float ddx(float)" : "dfdx",
        "native float ddy(float)" : "dfdy",
        "native float exp(float)" : "exp",
        "native float floor(float)" : "floor",
        "native float log(float)" : "log",
        "native float pow(float,float)" : "pow",
        "native float round(float)" : "round",
        "native float sin(float)" : "sin",
        "native float sinh(float)" : "sinh",
        "native float sqrt(float)" : "sqrt",
        "native float tan(float)" : "tan",
        "native float tanh(float)" : "tanh",
        "native float trunc(float)" : "trunc",
    };

    if (key in functionsWithTheSameCallingConvention) {
        const callString = `${functionsWithTheSameCallingConvention[key]}(${args.join(", ")})`;
        if (resultVariable)
            return `${resultVariable} = ${callString};`;
        else
            return `${callString};`;
    }

    const functionsWithDifferentCallingConventions = {
        "native uint f32tof16(float)" : () => `${resultVariable} = uint(static_cast<ushort>(half(${args[0]})));`,
        "native float f16tof32(uint)" : () => `${resultVariable} = float(static_cast<half>(ushort(${args[0]})));`
    };
    if (key in functionsWithDifferentCallingConventions)
        return functionsWithDifferentCallingConventions[key]();

    throw new Error(`${node} doesn't have mapping to a native Metal function.`);
}