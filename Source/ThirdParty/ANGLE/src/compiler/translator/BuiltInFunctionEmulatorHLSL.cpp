//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "angle_gl.h"
#include "compiler/translator/BuiltInFunctionEmulator.h"
#include "compiler/translator/BuiltInFunctionEmulatorHLSL.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/VersionGLSL.h"

namespace sh
{

void InitBuiltInIsnanFunctionEmulatorForHLSLWorkarounds(BuiltInFunctionEmulator *emu,
                                                        int targetGLSLVersion)
{
    if (targetGLSLVersion < GLSL_VERSION_130)
        return;

    TType *float1 = new TType(EbtFloat);
    TType *float2 = new TType(EbtFloat, 2);
    TType *float3 = new TType(EbtFloat, 3);
    TType *float4 = new TType(EbtFloat, 4);

    emu->addEmulatedFunction(EOpIsNan, float1,
                             "bool webgl_isnan_emu(float x)\n"
                             "{\n"
                             "    return (x > 0.0 || x < 0.0) ? false : x != 0.0;\n"
                             "}\n"
                             "\n");

    emu->addEmulatedFunction(
        EOpIsNan, float2,
        "bool2 webgl_isnan_emu(float2 x)\n"
        "{\n"
        "    bool2 isnan;\n"
        "    for (int i = 0; i < 2; i++)\n"
        "    {\n"
        "        isnan[i] = (x[i] > 0.0 || x[i] < 0.0) ? false : x[i] != 0.0;\n"
        "    }\n"
        "    return isnan;\n"
        "}\n");

    emu->addEmulatedFunction(
        EOpIsNan, float3,
        "bool3 webgl_isnan_emu(float3 x)\n"
        "{\n"
        "    bool3 isnan;\n"
        "    for (int i = 0; i < 3; i++)\n"
        "    {\n"
        "        isnan[i] = (x[i] > 0.0 || x[i] < 0.0) ? false : x[i] != 0.0;\n"
        "    }\n"
        "    return isnan;\n"
        "}\n");

    emu->addEmulatedFunction(
        EOpIsNan, float4,
        "bool4 webgl_isnan_emu(float4 x)\n"
        "{\n"
        "    bool4 isnan;\n"
        "    for (int i = 0; i < 4; i++)\n"
        "    {\n"
        "        isnan[i] = (x[i] > 0.0 || x[i] < 0.0) ? false : x[i] != 0.0;\n"
        "    }\n"
        "    return isnan;\n"
        "}\n");
}

void InitBuiltInFunctionEmulatorForHLSL(BuiltInFunctionEmulator *emu)
{
    TType *float1 = new TType(EbtFloat);
    TType *float2 = new TType(EbtFloat, 2);
    TType *float3 = new TType(EbtFloat, 3);
    TType *float4 = new TType(EbtFloat, 4);
    TType *int1   = new TType(EbtInt);
    TType *int2   = new TType(EbtInt, 2);
    TType *int3   = new TType(EbtInt, 3);
    TType *int4   = new TType(EbtInt, 4);
    TType *uint1  = new TType(EbtUInt);
    TType *uint2  = new TType(EbtUInt, 2);
    TType *uint3  = new TType(EbtUInt, 3);
    TType *uint4  = new TType(EbtUInt, 4);

    emu->addEmulatedFunction(EOpMod, float1, float1,
                             "float webgl_mod_emu(float x, float y)\n"
                             "{\n"
                             "    return x - y * floor(x / y);\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpMod, float2, float2,
                             "float2 webgl_mod_emu(float2 x, float2 y)\n"
                             "{\n"
                             "    return x - y * floor(x / y);\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpMod, float2, float1,
                             "float2 webgl_mod_emu(float2 x, float y)\n"
                             "{\n"
                             "    return x - y * floor(x / y);\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpMod, float3, float3,
                             "float3 webgl_mod_emu(float3 x, float3 y)\n"
                             "{\n"
                             "    return x - y * floor(x / y);\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpMod, float3, float1,
                             "float3 webgl_mod_emu(float3 x, float y)\n"
                             "{\n"
                             "    return x - y * floor(x / y);\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpMod, float4, float4,
                             "float4 webgl_mod_emu(float4 x, float4 y)\n"
                             "{\n"
                             "    return x - y * floor(x / y);\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpMod, float4, float1,
                             "float4 webgl_mod_emu(float4 x, float y)\n"
                             "{\n"
                             "    return x - y * floor(x / y);\n"
                             "}\n"
                             "\n");

    emu->addEmulatedFunction(EOpFrexp, float1, int1,
                             "float webgl_frexp_emu(float x, out int exp)\n"
                             "{\n"
                             "    float fexp;\n"
                             "    float mantissa = frexp(abs(x), fexp) * sign(x);\n"
                             "    exp = int(fexp);\n"
                             "    return mantissa;\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpFrexp, float2, int2,
                             "float2 webgl_frexp_emu(float2 x, out int2 exp)\n"
                             "{\n"
                             "    float2 fexp;\n"
                             "    float2 mantissa = frexp(abs(x), fexp) * sign(x);\n"
                             "    exp = int2(fexp);\n"
                             "    return mantissa;\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpFrexp, float3, int3,
                             "float3 webgl_frexp_emu(float3 x, out int3 exp)\n"
                             "{\n"
                             "    float3 fexp;\n"
                             "    float3 mantissa = frexp(abs(x), fexp) * sign(x);\n"
                             "    exp = int3(fexp);\n"
                             "    return mantissa;\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpFrexp, float4, int4,
                             "float4 webgl_frexp_emu(float4 x, out int4 exp)\n"
                             "{\n"
                             "    float4 fexp;\n"
                             "    float4 mantissa = frexp(abs(x), fexp) * sign(x);\n"
                             "    exp = int4(fexp);\n"
                             "    return mantissa;\n"
                             "}\n"
                             "\n");

    emu->addEmulatedFunction(EOpLdexp, float1, int1,
                             "float webgl_ldexp_emu(float x, int exp)\n"
                             "{\n"
                             "    return ldexp(x, float(exp));\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpLdexp, float2, int2,
                             "float2 webgl_ldexp_emu(float2 x, int2 exp)\n"
                             "{\n"
                             "    return ldexp(x, float2(exp));\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpLdexp, float3, int3,
                             "float3 webgl_ldexp_emu(float3 x, int3 exp)\n"
                             "{\n"
                             "    return ldexp(x, float3(exp));\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpLdexp, float4, int4,
                             "float4 webgl_ldexp_emu(float4 x, int4 exp)\n"
                             "{\n"
                             "    return ldexp(x, float4(exp));\n"
                             "}\n"
                             "\n");

    emu->addEmulatedFunction(EOpFaceForward, float1, float1, float1,
                             "float webgl_faceforward_emu(float N, float I, float Nref)\n"
                             "{\n"
                             "    if(dot(Nref, I) >= 0)\n"
                             "    {\n"
                             "        return -N;\n"
                             "    }\n"
                             "    else\n"
                             "    {\n"
                             "        return N;\n"
                             "    }\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpFaceForward, float2, float2, float2,
                             "float2 webgl_faceforward_emu(float2 N, float2 I, float2 Nref)\n"
                             "{\n"
                             "    if(dot(Nref, I) >= 0)\n"
                             "    {\n"
                             "        return -N;\n"
                             "    }\n"
                             "    else\n"
                             "    {\n"
                             "        return N;\n"
                             "    }\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpFaceForward, float3, float3, float3,
                             "float3 webgl_faceforward_emu(float3 N, float3 I, float3 Nref)\n"
                             "{\n"
                             "    if(dot(Nref, I) >= 0)\n"
                             "    {\n"
                             "        return -N;\n"
                             "    }\n"
                             "    else\n"
                             "    {\n"
                             "        return N;\n"
                             "    }\n"
                             "}\n"
                             "\n");
    emu->addEmulatedFunction(EOpFaceForward, float4, float4, float4,
                             "float4 webgl_faceforward_emu(float4 N, float4 I, float4 Nref)\n"
                             "{\n"
                             "    if(dot(Nref, I) >= 0)\n"
                             "    {\n"
                             "        return -N;\n"
                             "    }\n"
                             "    else\n"
                             "    {\n"
                             "        return N;\n"
                             "    }\n"
                             "}\n"
                             "\n");

    emu->addEmulatedFunction(EOpAtan, float1, float1,
                             "float webgl_atan_emu(float y, float x)\n"
                             "{\n"
                             "    if(x == 0 && y == 0) x = 1;\n"  // Avoid producing a NaN
                             "    return atan2(y, x);\n"
                             "}\n");
    emu->addEmulatedFunction(EOpAtan, float2, float2,
                             "float2 webgl_atan_emu(float2 y, float2 x)\n"
                             "{\n"
                             "    if(x[0] == 0 && y[0] == 0) x[0] = 1;\n"
                             "    if(x[1] == 0 && y[1] == 0) x[1] = 1;\n"
                             "    return float2(atan2(y[0], x[0]), atan2(y[1], x[1]));\n"
                             "}\n");
    emu->addEmulatedFunction(
        EOpAtan, float3, float3,
        "float3 webgl_atan_emu(float3 y, float3 x)\n"
        "{\n"
        "    if(x[0] == 0 && y[0] == 0) x[0] = 1;\n"
        "    if(x[1] == 0 && y[1] == 0) x[1] = 1;\n"
        "    if(x[2] == 0 && y[2] == 0) x[2] = 1;\n"
        "    return float3(atan2(y[0], x[0]), atan2(y[1], x[1]), atan2(y[2], x[2]));\n"
        "}\n");
    emu->addEmulatedFunction(EOpAtan, float4, float4,
                             "float4 webgl_atan_emu(float4 y, float4 x)\n"
                             "{\n"
                             "    if(x[0] == 0 && y[0] == 0) x[0] = 1;\n"
                             "    if(x[1] == 0 && y[1] == 0) x[1] = 1;\n"
                             "    if(x[2] == 0 && y[2] == 0) x[2] = 1;\n"
                             "    if(x[3] == 0 && y[3] == 0) x[3] = 1;\n"
                             "    return float4(atan2(y[0], x[0]), atan2(y[1], x[1]), atan2(y[2], "
                             "x[2]), atan2(y[3], x[3]));\n"
                             "}\n");

    emu->addEmulatedFunction(EOpAsinh, float1,
                             "float webgl_asinh_emu(in float x) {\n"
                             "    return log(x + sqrt(pow(x, 2.0) + 1.0));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpAsinh, float2,
                             "float2 webgl_asinh_emu(in float2 x) {\n"
                             "    return log(x + sqrt(pow(x, 2.0) + 1.0));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpAsinh, float3,
                             "float3 webgl_asinh_emu(in float3 x) {\n"
                             "    return log(x + sqrt(pow(x, 2.0) + 1.0));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpAsinh, float4,
                             "float4 webgl_asinh_emu(in float4 x) {\n"
                             "    return log(x + sqrt(pow(x, 2.0) + 1.0));\n"
                             "}\n");

    emu->addEmulatedFunction(EOpAcosh, float1,
                             "float webgl_acosh_emu(in float x) {\n"
                             "    return log(x + sqrt(x + 1.0) * sqrt(x - 1.0));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpAcosh, float2,
                             "float2 webgl_acosh_emu(in float2 x) {\n"
                             "    return log(x + sqrt(x + 1.0) * sqrt(x - 1.0));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpAcosh, float3,
                             "float3 webgl_acosh_emu(in float3 x) {\n"
                             "    return log(x + sqrt(x + 1.0) * sqrt(x - 1.0));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpAcosh, float4,
                             "float4 webgl_acosh_emu(in float4 x) {\n"
                             "    return log(x + sqrt(x + 1.0) * sqrt(x - 1.0));\n"
                             "}\n");

    emu->addEmulatedFunction(EOpAtanh, float1,
                             "float webgl_atanh_emu(in float x) {\n"
                             "    return 0.5 * log((1.0 + x) / (1.0 - x));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpAtanh, float2,
                             "float2 webgl_atanh_emu(in float2 x) {\n"
                             "    return 0.5 * log((1.0 + x) / (1.0 - x));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpAtanh, float3,
                             "float3 webgl_atanh_emu(in float3 x) {\n"
                             "    return 0.5 * log((1.0 + x) / (1.0 - x));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpAtanh, float4,
                             "float4 webgl_atanh_emu(in float4 x) {\n"
                             "    return 0.5 * log((1.0 + x) / (1.0 - x));\n"
                             "}\n");

    emu->addEmulatedFunction(
        EOpRoundEven, float1,
        "float webgl_roundEven_emu(in float x) {\n"
        "    return (frac(x) == 0.5 && trunc(x) % 2.0 == 0.0) ? trunc(x) : round(x);\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpRoundEven, float2,
        "float2 webgl_roundEven_emu(in float2 x) {\n"
        "    float2 v;\n"
        "    v[0] = (frac(x[0]) == 0.5 && trunc(x[0]) % 2.0 == 0.0) ? trunc(x[0]) : round(x[0]);\n"
        "    v[1] = (frac(x[1]) == 0.5 && trunc(x[1]) % 2.0 == 0.0) ? trunc(x[1]) : round(x[1]);\n"
        "    return v;\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpRoundEven, float3,
        "float3 webgl_roundEven_emu(in float3 x) {\n"
        "    float3 v;\n"
        "    v[0] = (frac(x[0]) == 0.5 && trunc(x[0]) % 2.0 == 0.0) ? trunc(x[0]) : round(x[0]);\n"
        "    v[1] = (frac(x[1]) == 0.5 && trunc(x[1]) % 2.0 == 0.0) ? trunc(x[1]) : round(x[1]);\n"
        "    v[2] = (frac(x[2]) == 0.5 && trunc(x[2]) % 2.0 == 0.0) ? trunc(x[2]) : round(x[2]);\n"
        "    return v;\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpRoundEven, float4,
        "float4 webgl_roundEven_emu(in float4 x) {\n"
        "    float4 v;\n"
        "    v[0] = (frac(x[0]) == 0.5 && trunc(x[0]) % 2.0 == 0.0) ? trunc(x[0]) : round(x[0]);\n"
        "    v[1] = (frac(x[1]) == 0.5 && trunc(x[1]) % 2.0 == 0.0) ? trunc(x[1]) : round(x[1]);\n"
        "    v[2] = (frac(x[2]) == 0.5 && trunc(x[2]) % 2.0 == 0.0) ? trunc(x[2]) : round(x[2]);\n"
        "    v[3] = (frac(x[3]) == 0.5 && trunc(x[3]) % 2.0 == 0.0) ? trunc(x[3]) : round(x[3]);\n"
        "    return v;\n"
        "}\n");

    emu->addEmulatedFunction(EOpPackSnorm2x16, float2,
                             "int webgl_toSnorm16(in float x) {\n"
                             "    return int(round(clamp(x, -1.0, 1.0) * 32767.0));\n"
                             "}\n"
                             "\n"
                             "uint webgl_packSnorm2x16_emu(in float2 v) {\n"
                             "    int x = webgl_toSnorm16(v.x);\n"
                             "    int y = webgl_toSnorm16(v.y);\n"
                             "    return (asuint(y) << 16) | (asuint(x) & 0xffffu);\n"
                             "}\n");
    emu->addEmulatedFunction(EOpPackUnorm2x16, float2,
                             "uint webgl_toUnorm16(in float x) {\n"
                             "    return uint(round(clamp(x, 0.0, 1.0) * 65535.0));\n"
                             "}\n"
                             "\n"
                             "uint webgl_packUnorm2x16_emu(in float2 v) {\n"
                             "    uint x = webgl_toUnorm16(v.x);\n"
                             "    uint y = webgl_toUnorm16(v.y);\n"
                             "    return (y << 16) | x;\n"
                             "}\n");
    emu->addEmulatedFunction(EOpPackHalf2x16, float2,
                             "uint webgl_packHalf2x16_emu(in float2 v) {\n"
                             "    uint x = f32tof16(v.x);\n"
                             "    uint y = f32tof16(v.y);\n"
                             "    return (y << 16) | x;\n"
                             "}\n");

    emu->addEmulatedFunction(EOpUnpackSnorm2x16, uint1,
                             "float webgl_fromSnorm16(in uint x) {\n"
                             "    int xi = asint(x & 0x7fffu) - asint(x & 0x8000u);\n"
                             "    return clamp(float(xi) / 32767.0, -1.0, 1.0);\n"
                             "}\n"
                             "\n"
                             "float2 webgl_unpackSnorm2x16_emu(in uint u) {\n"
                             "    uint y = (u >> 16);\n"
                             "    uint x = u;\n"
                             "    return float2(webgl_fromSnorm16(x), webgl_fromSnorm16(y));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpUnpackUnorm2x16, uint1,
                             "float webgl_fromUnorm16(in uint x) {\n"
                             "    return float(x) / 65535.0;\n"
                             "}\n"
                             "\n"
                             "float2 webgl_unpackUnorm2x16_emu(in uint u) {\n"
                             "    uint y = (u >> 16);\n"
                             "    uint x = u & 0xffffu;\n"
                             "    return float2(webgl_fromUnorm16(x), webgl_fromUnorm16(y));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpUnpackHalf2x16, uint1,
                             "float2 webgl_unpackHalf2x16_emu(in uint u) {\n"
                             "    uint y = (u >> 16);\n"
                             "    uint x = u & 0xffffu;\n"
                             "    return float2(f16tof32(x), f16tof32(y));\n"
                             "}\n");

    emu->addEmulatedFunction(EOpPackSnorm4x8, float4,
                             "int webgl_toSnorm8(in float x) {\n"
                             "    return int(round(clamp(x, -1.0, 1.0) * 127.0));\n"
                             "}\n"
                             "\n"
                             "uint webgl_packSnorm4x8_emu(in float4 v) {\n"
                             "    int x = webgl_toSnorm8(v.x);\n"
                             "    int y = webgl_toSnorm8(v.y);\n"
                             "    int z = webgl_toSnorm8(v.z);\n"
                             "    int w = webgl_toSnorm8(v.w);\n"
                             "    return ((asuint(w) & 0xffu) << 24) | ((asuint(z) & 0xffu) << 16) "
                             "| ((asuint(y) & 0xffu) << 8) | (asuint(x) & 0xffu);\n"
                             "}\n");
    emu->addEmulatedFunction(EOpPackUnorm4x8, float4,
                             "uint webgl_toUnorm8(in float x) {\n"
                             "    return uint(round(clamp(x, 0.0, 1.0) * 255.0));\n"
                             "}\n"
                             "\n"
                             "uint webgl_packUnorm4x8_emu(in float4 v) {\n"
                             "    uint x = webgl_toUnorm8(v.x);\n"
                             "    uint y = webgl_toUnorm8(v.y);\n"
                             "    uint z = webgl_toUnorm8(v.z);\n"
                             "    uint w = webgl_toUnorm8(v.w);\n"
                             "    return (w << 24) | (z << 16) | (y << 8) | x;\n"
                             "}\n");

    emu->addEmulatedFunction(EOpUnpackSnorm4x8, uint1,
                             "float webgl_fromSnorm8(in uint x) {\n"
                             "    int xi = asint(x & 0x7fu) - asint(x & 0x80u);\n"
                             "    return clamp(float(xi) / 127.0, -1.0, 1.0);\n"
                             "}\n"
                             "\n"
                             "float4 webgl_unpackSnorm4x8_emu(in uint u) {\n"
                             "    uint w = (u >> 24);\n"
                             "    uint z = (u >> 16);\n"
                             "    uint y = (u >> 8);\n"
                             "    uint x = u;\n"
                             "    return float4(webgl_fromSnorm8(x), webgl_fromSnorm8(y), "
                             "webgl_fromSnorm8(z), webgl_fromSnorm8(w));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpUnpackUnorm4x8, uint1,
                             "float webgl_fromUnorm8(in uint x) {\n"
                             "    return float(x) / 255.0;\n"
                             "}\n"
                             "\n"
                             "float4 webgl_unpackUnorm4x8_emu(in uint u) {\n"
                             "    uint w = (u >> 24) & 0xffu;\n"
                             "    uint z = (u >> 16) & 0xffu;\n"
                             "    uint y = (u >> 8) & 0xffu;\n"
                             "    uint x = u & 0xffu;\n"
                             "    return float4(webgl_fromUnorm8(x), webgl_fromUnorm8(y), "
                             "webgl_fromUnorm8(z), webgl_fromUnorm8(w));\n"
                             "}\n");

    // The matrix resulting from outer product needs to be transposed
    // (matrices are stored as transposed to simplify element access in HLSL).
    // So the function should return transpose(c * r) where c is a column vector
    // and r is a row vector. This can be simplified by using the following
    // formula:
    //   transpose(c * r) = transpose(r) * transpose(c)
    // transpose(r) and transpose(c) are in a sense free, since to get the
    // transpose of r, we simply can build a column matrix out of the original
    // vector instead of a row matrix.
    emu->addEmulatedFunction(EOpOuterProduct, float2, float2,
                             "float2x2 webgl_outerProduct_emu(in float2 c, in float2 r) {\n"
                             "    return mul(float2x1(r), float1x2(c));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpOuterProduct, float3, float3,
                             "float3x3 webgl_outerProduct_emu(in float3 c, in float3 r) {\n"
                             "    return mul(float3x1(r), float1x3(c));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpOuterProduct, float4, float4,
                             "float4x4 webgl_outerProduct_emu(in float4 c, in float4 r) {\n"
                             "    return mul(float4x1(r), float1x4(c));\n"
                             "}\n");

    emu->addEmulatedFunction(EOpOuterProduct, float3, float2,
                             "float2x3 webgl_outerProduct_emu(in float3 c, in float2 r) {\n"
                             "    return mul(float2x1(r), float1x3(c));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpOuterProduct, float2, float3,
                             "float3x2 webgl_outerProduct_emu(in float2 c, in float3 r) {\n"
                             "    return mul(float3x1(r), float1x2(c));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpOuterProduct, float4, float2,
                             "float2x4 webgl_outerProduct_emu(in float4 c, in float2 r) {\n"
                             "    return mul(float2x1(r), float1x4(c));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpOuterProduct, float2, float4,
                             "float4x2 webgl_outerProduct_emu(in float2 c, in float4 r) {\n"
                             "    return mul(float4x1(r), float1x2(c));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpOuterProduct, float4, float3,
                             "float3x4 webgl_outerProduct_emu(in float4 c, in float3 r) {\n"
                             "    return mul(float3x1(r), float1x4(c));\n"
                             "}\n");
    emu->addEmulatedFunction(EOpOuterProduct, float3, float4,
                             "float4x3 webgl_outerProduct_emu(in float3 c, in float4 r) {\n"
                             "    return mul(float4x1(r), float1x3(c));\n"
                             "}\n");

    TType *mat2 = new TType(EbtFloat, 2, 2);
    TType *mat3 = new TType(EbtFloat, 3, 3);
    TType *mat4 = new TType(EbtFloat, 4, 4);

    // Remember here that the parameter matrix is actually the transpose
    // of the matrix that we're trying to invert, and the resulting matrix
    // should also be the transpose of the inverse.

    // When accessing the parameter matrix with m[a][b] it can be thought of so
    // that a is the column and b is the row of the matrix that we're inverting.

    // We calculate the inverse as the adjugate matrix divided by the
    // determinant of the matrix being inverted. However, as the result needs
    // to be transposed, we actually use of the transpose of the adjugate matrix
    // which happens to be the cofactor matrix. That's stored in "cof".

    // We don't need to care about divide-by-zero since results are undefined
    // for singular or poorly-conditioned matrices.

    emu->addEmulatedFunction(EOpInverse, mat2,
                             "float2x2 webgl_inverse_emu(in float2x2 m) {\n"
                             "    float2x2 cof = { m[1][1], -m[0][1], -m[1][0], m[0][0] };\n"
                             "    return cof / determinant(transpose(m));\n"
                             "}\n");

    // cofAB is the cofactor for column A and row B.

    emu->addEmulatedFunction(
        EOpInverse, mat3,
        "float3x3 webgl_inverse_emu(in float3x3 m) {\n"
        "    float cof00 = m[1][1] * m[2][2] - m[2][1] * m[1][2];\n"
        "    float cof01 = -(m[1][0] * m[2][2] - m[2][0] * m[1][2]);\n"
        "    float cof02 = m[1][0] * m[2][1] - m[2][0] * m[1][1];\n"
        "    float cof10 = -(m[0][1] * m[2][2] - m[2][1] * m[0][2]);\n"
        "    float cof11 = m[0][0] * m[2][2] - m[2][0] * m[0][2];\n"
        "    float cof12 = -(m[0][0] * m[2][1] - m[2][0] * m[0][1]);\n"
        "    float cof20 = m[0][1] * m[1][2] - m[1][1] * m[0][2];\n"
        "    float cof21 = -(m[0][0] * m[1][2] - m[1][0] * m[0][2]);\n"
        "    float cof22 = m[0][0] * m[1][1] - m[1][0] * m[0][1];\n"
        "    float3x3 cof = { cof00, cof10, cof20, cof01, cof11, cof21, cof02, cof12, cof22 };\n"
        "    return cof / determinant(transpose(m));\n"
        "}\n");

    emu->addEmulatedFunction(
        EOpInverse, mat4,
        "float4x4 webgl_inverse_emu(in float4x4 m) {\n"
        "    float cof00 = m[1][1] * m[2][2] * m[3][3] + m[2][1] * m[3][2] * m[1][3] + m[3][1] * "
        "m[1][2] * m[2][3]"
        " - m[1][1] * m[3][2] * m[2][3] - m[2][1] * m[1][2] * m[3][3] - m[3][1] * m[2][2] * "
        "m[1][3];\n"
        "    float cof01 = -(m[1][0] * m[2][2] * m[3][3] + m[2][0] * m[3][2] * m[1][3] + m[3][0] * "
        "m[1][2] * m[2][3]"
        " - m[1][0] * m[3][2] * m[2][3] - m[2][0] * m[1][2] * m[3][3] - m[3][0] * m[2][2] * "
        "m[1][3]);\n"
        "    float cof02 = m[1][0] * m[2][1] * m[3][3] + m[2][0] * m[3][1] * m[1][3] + m[3][0] * "
        "m[1][1] * m[2][3]"
        " - m[1][0] * m[3][1] * m[2][3] - m[2][0] * m[1][1] * m[3][3] - m[3][0] * m[2][1] * "
        "m[1][3];\n"
        "    float cof03 = -(m[1][0] * m[2][1] * m[3][2] + m[2][0] * m[3][1] * m[1][2] + m[3][0] * "
        "m[1][1] * m[2][2]"
        " - m[1][0] * m[3][1] * m[2][2] - m[2][0] * m[1][1] * m[3][2] - m[3][0] * m[2][1] * "
        "m[1][2]);\n"
        "    float cof10 = -(m[0][1] * m[2][2] * m[3][3] + m[2][1] * m[3][2] * m[0][3] + m[3][1] * "
        "m[0][2] * m[2][3]"
        " - m[0][1] * m[3][2] * m[2][3] - m[2][1] * m[0][2] * m[3][3] - m[3][1] * m[2][2] * "
        "m[0][3]);\n"
        "    float cof11 = m[0][0] * m[2][2] * m[3][3] + m[2][0] * m[3][2] * m[0][3] + m[3][0] * "
        "m[0][2] * m[2][3]"
        " - m[0][0] * m[3][2] * m[2][3] - m[2][0] * m[0][2] * m[3][3] - m[3][0] * m[2][2] * "
        "m[0][3];\n"
        "    float cof12 = -(m[0][0] * m[2][1] * m[3][3] + m[2][0] * m[3][1] * m[0][3] + m[3][0] * "
        "m[0][1] * m[2][3]"
        " - m[0][0] * m[3][1] * m[2][3] - m[2][0] * m[0][1] * m[3][3] - m[3][0] * m[2][1] * "
        "m[0][3]);\n"
        "    float cof13 = m[0][0] * m[2][1] * m[3][2] + m[2][0] * m[3][1] * m[0][2] + m[3][0] * "
        "m[0][1] * m[2][2]"
        " - m[0][0] * m[3][1] * m[2][2] - m[2][0] * m[0][1] * m[3][2] - m[3][0] * m[2][1] * "
        "m[0][2];\n"
        "    float cof20 = m[0][1] * m[1][2] * m[3][3] + m[1][1] * m[3][2] * m[0][3] + m[3][1] * "
        "m[0][2] * m[1][3]"
        " - m[0][1] * m[3][2] * m[1][3] - m[1][1] * m[0][2] * m[3][3] - m[3][1] * m[1][2] * "
        "m[0][3];\n"
        "    float cof21 = -(m[0][0] * m[1][2] * m[3][3] + m[1][0] * m[3][2] * m[0][3] + m[3][0] * "
        "m[0][2] * m[1][3]"
        " - m[0][0] * m[3][2] * m[1][3] - m[1][0] * m[0][2] * m[3][3] - m[3][0] * m[1][2] * "
        "m[0][3]);\n"
        "    float cof22 = m[0][0] * m[1][1] * m[3][3] + m[1][0] * m[3][1] * m[0][3] + m[3][0] * "
        "m[0][1] * m[1][3]"
        " - m[0][0] * m[3][1] * m[1][3] - m[1][0] * m[0][1] * m[3][3] - m[3][0] * m[1][1] * "
        "m[0][3];\n"
        "    float cof23 = -(m[0][0] * m[1][1] * m[3][2] + m[1][0] * m[3][1] * m[0][2] + m[3][0] * "
        "m[0][1] * m[1][2]"
        " - m[0][0] * m[3][1] * m[1][2] - m[1][0] * m[0][1] * m[3][2] - m[3][0] * m[1][1] * "
        "m[0][2]);\n"
        "    float cof30 = -(m[0][1] * m[1][2] * m[2][3] + m[1][1] * m[2][2] * m[0][3] + m[2][1] * "
        "m[0][2] * m[1][3]"
        " - m[0][1] * m[2][2] * m[1][3] - m[1][1] * m[0][2] * m[2][3] - m[2][1] * m[1][2] * "
        "m[0][3]);\n"
        "    float cof31 = m[0][0] * m[1][2] * m[2][3] + m[1][0] * m[2][2] * m[0][3] + m[2][0] * "
        "m[0][2] * m[1][3]"
        " - m[0][0] * m[2][2] * m[1][3] - m[1][0] * m[0][2] * m[2][3] - m[2][0] * m[1][2] * "
        "m[0][3];\n"
        "    float cof32 = -(m[0][0] * m[1][1] * m[2][3] + m[1][0] * m[2][1] * m[0][3] + m[2][0] * "
        "m[0][1] * m[1][3]"
        " - m[0][0] * m[2][1] * m[1][3] - m[1][0] * m[0][1] * m[2][3] - m[2][0] * m[1][1] * "
        "m[0][3]);\n"
        "    float cof33 = m[0][0] * m[1][1] * m[2][2] + m[1][0] * m[2][1] * m[0][2] + m[2][0] * "
        "m[0][1] * m[1][2]"
        " - m[0][0] * m[2][1] * m[1][2] - m[1][0] * m[0][1] * m[2][2] - m[2][0] * m[1][1] * "
        "m[0][2];\n"
        "    float4x4 cof = { cof00, cof10, cof20, cof30, cof01, cof11, cof21, cof31,"
        " cof02, cof12, cof22, cof32, cof03, cof13, cof23, cof33 };\n"
        "    return cof / determinant(transpose(m));\n"
        "}\n");

    TType *bool1 = new TType(EbtBool);
    TType *bool2 = new TType(EbtBool, 2);
    TType *bool3 = new TType(EbtBool, 3);
    TType *bool4 = new TType(EbtBool, 4);

    // Emulate ESSL3 variant of mix that takes last argument as boolean vector.
    // genType mix (genType x, genType y, genBType a): Selects which vector each returned component
    // comes from.
    // For a component of 'a' that is false, the corresponding component of 'x' is returned.For a
    // component of 'a' that is true,
    // the corresponding component of 'y' is returned.
    emu->addEmulatedFunction(EOpMix, float1, float1, bool1,
                             "float webgl_mix_emu(float x, float y, bool a)\n"
                             "{\n"
                             "    return a ? y : x;\n"
                             "}\n");
    emu->addEmulatedFunction(EOpMix, float2, float2, bool2,
                             "float2 webgl_mix_emu(float2 x, float2 y, bool2 a)\n"
                             "{\n"
                             "    return a ? y : x;\n"
                             "}\n");
    emu->addEmulatedFunction(EOpMix, float3, float3, bool3,
                             "float3 webgl_mix_emu(float3 x, float3 y, bool3 a)\n"
                             "{\n"
                             "    return a ? y : x;\n"
                             "}\n");
    emu->addEmulatedFunction(EOpMix, float4, float4, bool4,
                             "float4 webgl_mix_emu(float4 x, float4 y, bool4 a)\n"
                             "{\n"
                             "    return a ? y : x;\n"
                             "}\n");

    emu->addEmulatedFunction(
        EOpBitfieldExtract, uint1, int1, int1,
        "uint webgl_bitfieldExtract_emu(uint value, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return 0u;\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint mask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    return (value & mask) >> offset;\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldExtract, uint2, int1, int1,
        "uint2 webgl_bitfieldExtract_emu(uint2 value, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return uint2(0u, 0u);\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint mask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    return (value & mask) >> offset;\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldExtract, uint3, int1, int1,
        "uint3 webgl_bitfieldExtract_emu(uint3 value, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return uint3(0u, 0u, 0u);\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint mask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    return (value & mask) >> offset;\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldExtract, uint4, int1, int1,
        "uint4 webgl_bitfieldExtract_emu(uint4 value, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return uint4(0u, 0u, 0u, 0u);\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint mask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    return (value & mask) >> offset;\n"
        "}\n");

    emu->addEmulatedFunction(
        EOpBitfieldExtract, int1, int1, int1,
        "int webgl_bitfieldExtract_emu(int value, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return 0;\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint mask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint resultUnsigned = (asuint(value) & mask) >> offset;\n"
        "    if (bits != 32 && (resultUnsigned & maskMsb) != 0)\n"
        "    {\n"
        "        uint higherBitsMask = ((1u << (32 - bits)) - 1u) << bits;\n"
        "        resultUnsigned |= higherBitsMask;\n"
        "    }\n"
        "    return asint(resultUnsigned);\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldExtract, int2, int1, int1,
        "int2 webgl_bitfieldExtract_emu(int2 value, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return int2(0, 0);\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint mask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint2 resultUnsigned = (asuint(value) & mask) >> offset;\n"
        "    if (bits != 32)\n"
        "    {\n"
        "        uint higherBitsMask = ((1u << (32 - bits)) - 1u) << bits;\n"
        "        resultUnsigned |= ((resultUnsigned & maskMsb) >> (bits - 1)) * higherBitsMask;\n"
        "    }\n"
        "    return asint(resultUnsigned);\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldExtract, int3, int1, int1,
        "int3 webgl_bitfieldExtract_emu(int3 value, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return int3(0, 0, 0);\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint mask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint3 resultUnsigned = (asuint(value) & mask) >> offset;\n"
        "    if (bits != 32)\n"
        "    {\n"
        "        uint higherBitsMask = ((1u << (32 - bits)) - 1u) << bits;\n"
        "        resultUnsigned |= ((resultUnsigned & maskMsb) >> (bits - 1)) * higherBitsMask;\n"
        "    }\n"
        "    return asint(resultUnsigned);\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldExtract, int4, int1, int1,
        "int4 webgl_bitfieldExtract_emu(int4 value, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return int4(0, 0, 0, 0);\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint mask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint4 resultUnsigned = (asuint(value) & mask) >> offset;\n"
        "    if (bits != 32)\n"
        "    {\n"
        "        uint higherBitsMask = ((1u << (32 - bits)) - 1u) << bits;\n"
        "        resultUnsigned |= ((resultUnsigned & maskMsb) >> (bits - 1)) * higherBitsMask;\n"
        "    }\n"
        "    return asint(resultUnsigned);\n"
        "}\n");

    emu->addEmulatedFunction(
        EOpBitfieldInsert, uint1, uint1, int1, int1,
        "uint webgl_bitfieldInsert_emu(uint base, uint insert, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return base;\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint insertMask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint baseMask = ~insertMask;\n"
        "    return (base & baseMask) | ((insert << offset) & insertMask);\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldInsert, uint2, uint2, int1, int1,
        "uint2 webgl_bitfieldInsert_emu(uint2 base, uint2 insert, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return base;\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint insertMask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint baseMask = ~insertMask;\n"
        "    return (base & baseMask) | ((insert << offset) & insertMask);\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldInsert, uint3, uint3, int1, int1,
        "uint3 webgl_bitfieldInsert_emu(uint3 base, uint3 insert, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return base;\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint insertMask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint baseMask = ~insertMask;\n"
        "    return (base & baseMask) | ((insert << offset) & insertMask);\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldInsert, uint4, uint4, int1, int1,
        "uint4 webgl_bitfieldInsert_emu(uint4 base, uint4 insert, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return base;\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint insertMask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint baseMask = ~insertMask;\n"
        "    return (base & baseMask) | ((insert << offset) & insertMask);\n"
        "}\n");

    emu->addEmulatedFunction(
        EOpBitfieldInsert, int1, int1, int1, int1,
        "int webgl_bitfieldInsert_emu(int base, int insert, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return base;\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint insertMask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint baseMask = ~insertMask;\n"
        "    uint resultUnsigned = (asuint(base) & baseMask) | ((asuint(insert) << offset) & "
        "insertMask);\n"
        "    return asint(resultUnsigned);\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldInsert, int2, int2, int1, int1,
        "int2 webgl_bitfieldInsert_emu(int2 base, int2 insert, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return base;\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint insertMask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint baseMask = ~insertMask;\n"
        "    uint2 resultUnsigned = (asuint(base) & baseMask) | ((asuint(insert) << offset) & "
        "insertMask);\n"
        "    return asint(resultUnsigned);\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldInsert, int3, int3, int1, int1,
        "int3 webgl_bitfieldInsert_emu(int3 base, int3 insert, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return base;\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint insertMask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint baseMask = ~insertMask;\n"
        "    uint3 resultUnsigned = (asuint(base) & baseMask) | ((asuint(insert) << offset) & "
        "insertMask);\n"
        "    return asint(resultUnsigned);\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpBitfieldInsert, int4, int4, int1, int1,
        "int4 webgl_bitfieldInsert_emu(int4 base, int4 insert, int offset, int bits)\n"
        "{\n"
        "    if (offset < 0 || bits <= 0 || offset >= 32 || bits > 32 || offset + bits > 32)\n"
        "    {\n"
        "        return base;\n"
        "    }\n"
        "    uint maskMsb = (1u << (bits - 1));\n"
        "    uint insertMask = ((maskMsb - 1u) | maskMsb) << offset;\n"
        "    uint baseMask = ~insertMask;\n"
        "    uint4 resultUnsigned = (asuint(base) & baseMask) | ((asuint(insert) << offset) & "
        "insertMask);\n"
        "    return asint(resultUnsigned);\n"
        "}\n");

    emu->addEmulatedFunction(EOpUaddCarry, uint1, uint1, uint1,
                             "uint webgl_uaddCarry_emu(uint x, uint y, out uint carry)\n"
                             "{\n"
                             "    carry = uint(x > (0xffffffffu - y));\n"
                             "    return x + y;\n"
                             "}\n");
    emu->addEmulatedFunction(EOpUaddCarry, uint2, uint2, uint2,
                             "uint2 webgl_uaddCarry_emu(uint2 x, uint2 y, out uint2 carry)\n"
                             "{\n"
                             "    carry = uint2(x > (0xffffffffu - y));\n"
                             "    return x + y;\n"
                             "}\n");
    emu->addEmulatedFunction(EOpUaddCarry, uint3, uint3, uint3,
                             "uint3 webgl_uaddCarry_emu(uint3 x, uint3 y, out uint3 carry)\n"
                             "{\n"
                             "    carry = uint3(x > (0xffffffffu - y));\n"
                             "    return x + y;\n"
                             "}\n");
    emu->addEmulatedFunction(EOpUaddCarry, uint4, uint4, uint4,
                             "uint4 webgl_uaddCarry_emu(uint4 x, uint4 y, out uint4 carry)\n"
                             "{\n"
                             "    carry = uint4(x > (0xffffffffu - y));\n"
                             "    return x + y;\n"
                             "}\n");

    emu->addEmulatedFunction(EOpUsubBorrow, uint1, uint1, uint1,
                             "uint webgl_usubBorrow_emu(uint x, uint y, out uint borrow)\n"
                             "{\n"
                             "    borrow = uint(x < y);\n"
                             "    return x - y;\n"
                             "}\n");
    emu->addEmulatedFunction(EOpUsubBorrow, uint2, uint2, uint2,
                             "uint2 webgl_usubBorrow_emu(uint2 x, uint2 y, out uint2 borrow)\n"
                             "{\n"
                             "    borrow = uint2(x < y);\n"
                             "    return x - y;\n"
                             "}\n");
    emu->addEmulatedFunction(EOpUsubBorrow, uint3, uint3, uint3,
                             "uint3 webgl_usubBorrow_emu(uint3 x, uint3 y, out uint3 borrow)\n"
                             "{\n"
                             "    borrow = uint3(x < y);\n"
                             "    return x - y;\n"
                             "}\n");
    emu->addEmulatedFunction(EOpUsubBorrow, uint4, uint4, uint4,
                             "uint4 webgl_usubBorrow_emu(uint4 x, uint4 y, out uint4 borrow)\n"
                             "{\n"
                             "    borrow = uint4(x < y);\n"
                             "    return x - y;\n"
                             "}\n");

    // (a + b2^16) * (c + d2^16) = ac + (ad + bc) * 2^16 + bd * 2^32
    // Also note that below, a * d + ((a * c) >> 16) is guaranteed not to overflow, because:
    // a <= 0xffff, d <= 0xffff, ((a * c) >> 16) <= 0xffff and 0xffff * 0xffff + 0xffff = 0xffff0000
    BuiltInFunctionEmulator::FunctionId umulExtendedUint1 = emu->addEmulatedFunction(
        EOpUmulExtended, uint1, uint1, uint1, uint1,
        "void webgl_umulExtended_emu(uint x, uint y, out uint msb, out uint lsb)\n"
        "{\n"
        "    lsb = x * y;\n"
        "    uint a = (x & 0xffffu);\n"
        "    uint b = (x >> 16);\n"
        "    uint c = (y & 0xffffu);\n"
        "    uint d = (y >> 16);\n"
        "    uint ad = a * d + ((a * c) >> 16);\n"
        "    uint bc = b * c;\n"
        "    uint carry = uint(ad > (0xffffffffu - bc));\n"
        "    msb = ((ad + bc) >> 16) + (carry << 16) + b * d;\n"
        "}\n");
    emu->addEmulatedFunctionWithDependency(
        umulExtendedUint1, EOpUmulExtended, uint2, uint2, uint2, uint2,
        "void webgl_umulExtended_emu(uint2 x, uint2 y, out uint2 msb, out uint2 lsb)\n"
        "{\n"
        "    webgl_umulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    webgl_umulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "}\n");
    emu->addEmulatedFunctionWithDependency(
        umulExtendedUint1, EOpUmulExtended, uint3, uint3, uint3, uint3,
        "void webgl_umulExtended_emu(uint3 x, uint3 y, out uint3 msb, out uint3 lsb)\n"
        "{\n"
        "    webgl_umulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    webgl_umulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "    webgl_umulExtended_emu(x.z, y.z, msb.z, lsb.z);\n"
        "}\n");
    emu->addEmulatedFunctionWithDependency(
        umulExtendedUint1, EOpUmulExtended, uint4, uint4, uint4, uint4,
        "void webgl_umulExtended_emu(uint4 x, uint4 y, out uint4 msb, out uint4 lsb)\n"
        "{\n"
        "    webgl_umulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    webgl_umulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "    webgl_umulExtended_emu(x.z, y.z, msb.z, lsb.z);\n"
        "    webgl_umulExtended_emu(x.w, y.w, msb.w, lsb.w);\n"
        "}\n");

    // The imul emulation does two's complement negation on the lsb and msb manually in case the
    // result needs to be negative.
    // TODO(oetuaho): Note that this code doesn't take one edge case into account, where x or y is
    // -2^31. abs(-2^31) is undefined.
    BuiltInFunctionEmulator::FunctionId imulExtendedInt1 = emu->addEmulatedFunctionWithDependency(
        umulExtendedUint1, EOpImulExtended, int1, int1, int1, int1,
        "void webgl_imulExtended_emu(int x, int y, out int msb, out int lsb)\n"
        "{\n"
        "    uint unsignedMsb;\n"
        "    uint unsignedLsb;\n"
        "    bool negative = (x < 0) != (y < 0);\n"
        "    webgl_umulExtended_emu(uint(abs(x)), uint(abs(y)), unsignedMsb, unsignedLsb);\n"
        "    lsb = asint(unsignedLsb);\n"
        "    msb = asint(unsignedMsb);\n"
        "    if (negative)\n"
        "    {\n"
        "        lsb = ~lsb;\n"
        "        msb = ~msb;\n"
        "        if (lsb == 0xffffffff)\n"
        "        {\n"
        "            lsb = 0;\n"
        "            msb += 1;\n"
        "        }\n"
        "        else\n"
        "        {\n"
        "            lsb += 1;\n"
        "        }\n"
        "    }\n"
        "}\n");
    emu->addEmulatedFunctionWithDependency(
        imulExtendedInt1, EOpImulExtended, int2, int2, int2, int2,
        "void webgl_imulExtended_emu(int2 x, int2 y, out int2 msb, out int2 lsb)\n"
        "{\n"
        "    webgl_imulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    webgl_imulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "}\n");
    emu->addEmulatedFunctionWithDependency(
        imulExtendedInt1, EOpImulExtended, int3, int3, int3, int3,
        "void webgl_imulExtended_emu(int3 x, int3 y, out int3 msb, out int3 lsb)\n"
        "{\n"
        "    webgl_imulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    webgl_imulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "    webgl_imulExtended_emu(x.z, y.z, msb.z, lsb.z);\n"
        "}\n");
    emu->addEmulatedFunctionWithDependency(
        imulExtendedInt1, EOpImulExtended, int4, int4, int4, int4,
        "void webgl_imulExtended_emu(int4 x, int4 y, out int4 msb, out int4 lsb)\n"
        "{\n"
        "    webgl_imulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    webgl_imulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "    webgl_imulExtended_emu(x.z, y.z, msb.z, lsb.z);\n"
        "    webgl_imulExtended_emu(x.w, y.w, msb.w, lsb.w);\n"
        "}\n");
}

}  // namespace sh
