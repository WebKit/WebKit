//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "angle_gl.h"
#include "compiler/translator/BuiltInFunctionEmulator.h"
#include "compiler/translator/BuiltInFunctionEmulatorGLSL.h"
#include "compiler/translator/Cache.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/VersionGLSL.h"

namespace sh
{

void InitBuiltInAbsFunctionEmulatorForGLSLWorkarounds(BuiltInFunctionEmulator *emu,
                                                      sh::GLenum shaderType)
{
    if (shaderType == GL_VERTEX_SHADER)
    {
        const TType *int1 = TCache::getType(EbtInt);
        emu->addEmulatedFunction(EOpAbs, int1, "int abs_emu(int x) { return x * sign(x); }");
    }
}

void InitBuiltInIsnanFunctionEmulatorForGLSLWorkarounds(BuiltInFunctionEmulator *emu,
                                                        int targetGLSLVersion)
{
    // isnan() is supported since GLSL 1.3.
    if (targetGLSLVersion < GLSL_VERSION_130)
        return;

    const TType *float1 = TCache::getType(EbtFloat);
    const TType *float2 = TCache::getType(EbtFloat, 2);
    const TType *float3 = TCache::getType(EbtFloat, 3);
    const TType *float4 = TCache::getType(EbtFloat, 4);

    // !(x > 0.0 || x < 0.0 || x == 0.0) will be optimized and always equal to false.
    emu->addEmulatedFunction(
        EOpIsNan, float1,
        "bool isnan_emu(float x) { return (x > 0.0 || x < 0.0) ? false : x != 0.0; }");
    emu->addEmulatedFunction(
        EOpIsNan, float2,
        "bvec2 isnan_emu(vec2 x)\n"
        "{\n"
        "    bvec2 isnan;\n"
        "    for (int i = 0; i < 2; i++)\n"
        "    {\n"
        "        isnan[i] = (x[i] > 0.0 || x[i] < 0.0) ? false : x[i] != 0.0;\n"
        "    }\n"
        "    return isnan;\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpIsNan, float3,
        "bvec3 isnan_emu(vec3 x)\n"
        "{\n"
        "    bvec3 isnan;\n"
        "    for (int i = 0; i < 3; i++)\n"
        "    {\n"
        "        isnan[i] = (x[i] > 0.0 || x[i] < 0.0) ? false : x[i] != 0.0;\n"
        "    }\n"
        "    return isnan;\n"
        "}\n");
    emu->addEmulatedFunction(
        EOpIsNan, float4,
        "bvec4 isnan_emu(vec4 x)\n"
        "{\n"
        "    bvec4 isnan;\n"
        "    for (int i = 0; i < 4; i++)\n"
        "    {\n"
        "        isnan[i] = (x[i] > 0.0 || x[i] < 0.0) ? false : x[i] != 0.0;\n"
        "    }\n"
        "    return isnan;\n"
        "}\n");
}

void InitBuiltInAtanFunctionEmulatorForGLSLWorkarounds(BuiltInFunctionEmulator *emu)
{
    const TType *float1 = TCache::getType(EbtFloat);
    auto floatFuncId    = emu->addEmulatedFunction(
        EOpAtan, float1, float1,
        "emu_precision float atan_emu(emu_precision float y, emu_precision "
        "float x)\n"
        "{\n"
        "    if (x > 0.0) return atan(y / x);\n"
        "    else if (x < 0.0 && y >= 0.0) return atan(y / x) + 3.14159265;\n"
        "    else if (x < 0.0 && y < 0.0) return atan(y / x) - 3.14159265;\n"
        "    else return 1.57079632 * sign(y);\n"
        "}\n");
    for (int dim = 2; dim <= 4; ++dim)
    {
        const TType *floatVec = TCache::getType(EbtFloat, static_cast<unsigned char>(dim));
        std::stringstream ss;
        ss << "emu_precision vec" << dim << " atan_emu(emu_precision vec" << dim
           << " y, emu_precision vec" << dim << " x)\n"
           << "{\n"
              "    return vec"
           << dim << "(";
        for (int i = 0; i < dim; ++i)
        {
            ss << "atan_emu(y[" << i << "], x[" << i << "])";
            if (i < dim - 1)
            {
                ss << ", ";
            }
        }
        ss << ");\n"
              "}\n";
        emu->addEmulatedFunctionWithDependency(floatFuncId, EOpAtan, floatVec, floatVec,
                                               ss.str().c_str());
    }
}

// Emulate built-in functions missing from GLSL 1.30 and higher
void InitBuiltInFunctionEmulatorForGLSLMissingFunctions(BuiltInFunctionEmulator *emu,
                                                        sh::GLenum shaderType,
                                                        int targetGLSLVersion)
{
    // Emulate packUnorm2x16 and unpackUnorm2x16 (GLSL 4.10)
    if (targetGLSLVersion < GLSL_VERSION_410)
    {
        const TType *float2 = TCache::getType(EbtFloat, 2);
        const TType *uint1  = TCache::getType(EbtUInt);

        // clang-format off
        emu->addEmulatedFunction(EOpPackUnorm2x16, float2,
            "uint packUnorm2x16_emu(vec2 v)\n"
            "{\n"
            "    int x = int(round(clamp(v.x, 0.0, 1.0) * 65535.0));\n"
            "    int y = int(round(clamp(v.y, 0.0, 1.0) * 65535.0));\n"
            "    return uint((y << 16) | (x & 0xFFFF));\n"
            "}\n");

        emu->addEmulatedFunction(EOpUnpackUnorm2x16, uint1,
            "vec2 unpackUnorm2x16_emu(uint u)\n"
            "{\n"
            "    float x = float(u & 0xFFFFu) / 65535.0;\n"
            "    float y = float(u >> 16) / 65535.0;\n"
            "    return vec2(x, y);\n"
            "}\n");
        // clang-format on
    }

    // Emulate packSnorm2x16, packHalf2x16, unpackSnorm2x16, and unpackHalf2x16 (GLSL 4.20)
    // by using floatBitsToInt, floatBitsToUint, intBitsToFloat, and uintBitsToFloat (GLSL 3.30).
    if (targetGLSLVersion >= GLSL_VERSION_330 && targetGLSLVersion < GLSL_VERSION_420)
    {
        const TType *float2 = TCache::getType(EbtFloat, 2);
        const TType *uint1  = TCache::getType(EbtUInt);

        // clang-format off
        emu->addEmulatedFunction(EOpPackSnorm2x16, float2,
            "uint packSnorm2x16_emu(vec2 v)\n"
            "{\n"
            "    #if defined(GL_ARB_shading_language_packing)\n"
            "        return packSnorm2x16(v);\n"
            "    #else\n"
            "        int x = int(round(clamp(v.x, -1.0, 1.0) * 32767.0));\n"
            "        int y = int(round(clamp(v.y, -1.0, 1.0) * 32767.0));\n"
            "        return uint((y << 16) | (x & 0xFFFF));\n"
            "    #endif\n"
            "}\n");
        emu->addEmulatedFunction(EOpUnpackSnorm2x16, uint1,
            "#if !defined(GL_ARB_shading_language_packing)\n"
            "    float fromSnorm(uint x)\n"
            "    {\n"
            "        int xi = (int(x) & 0x7FFF) - (int(x) & 0x8000);\n"
            "        return clamp(float(xi) / 32767.0, -1.0, 1.0);\n"
            "    }\n"
            "#endif\n"
            "\n"
            "vec2 unpackSnorm2x16_emu(uint u)\n"
            "{\n"
            "    #if defined(GL_ARB_shading_language_packing)\n"
            "        return unpackSnorm2x16(u);\n"
            "    #else\n"
            "        uint y = (u >> 16);\n"
            "        uint x = u;\n"
            "        return vec2(fromSnorm(x), fromSnorm(y));\n"
            "    #endif\n"
            "}\n");
        // Functions uint f32tof16(float val) and float f16tof32(uint val) are
        // based on the OpenGL redbook Appendix Session "Floating-Point Formats Used in OpenGL".
        emu->addEmulatedFunction(EOpPackHalf2x16, float2,
            "#if !defined(GL_ARB_shading_language_packing)\n"
            "    uint f32tof16(float val)\n"
            "    {\n"
            "        uint f32 = floatBitsToUint(val);\n"
            "        uint f16 = 0u;\n"
            "        uint sign = (f32 >> 16) & 0x8000u;\n"
            "        int exponent = int((f32 >> 23) & 0xFFu) - 127;\n"
            "        uint mantissa = f32 & 0x007FFFFFu;\n"
            "        if (exponent == 128)\n"
            "        {\n"
            "            // Infinity or NaN\n"
            "            // NaN bits that are masked out by 0x3FF get discarded.\n"
            "            // This can turn some NaNs to infinity, but this is allowed by the spec.\n"
            "            f16 = sign | (0x1Fu << 10);\n"
            "            f16 |= (mantissa & 0x3FFu);\n"
            "        }\n"
            "        else if (exponent > 15)\n"
            "        {\n"
            "            // Overflow - flush to Infinity\n"
            "            f16 = sign | (0x1Fu << 10);\n"
            "        }\n"
            "        else if (exponent > -15)\n"
            "        {\n"
            "            // Representable value\n"
            "            exponent += 15;\n"
            "            mantissa >>= 13;\n"
            "            f16 = sign | uint(exponent << 10) | mantissa;\n"
            "        }\n"
            "        else\n"
            "        {\n"
            "            f16 = sign;\n"
            "        }\n"
            "        return f16;\n"
            "    }\n"
            "#endif\n"
            "\n"
            "uint packHalf2x16_emu(vec2 v)\n"
            "{\n"
            "    #if defined(GL_ARB_shading_language_packing)\n"
            "        return packHalf2x16(v);\n"
            "    #else\n"
            "        uint x = f32tof16(v.x);\n"
            "        uint y = f32tof16(v.y);\n"
            "        return (y << 16) | x;\n"
            "    #endif\n"
            "}\n");
        emu->addEmulatedFunction(EOpUnpackHalf2x16, uint1,
            "#if !defined(GL_ARB_shading_language_packing)\n"
            "    float f16tof32(uint val)\n"
            "    {\n"
            "        uint sign = (val & 0x8000u) << 16;\n"
            "        int exponent = int((val & 0x7C00u) >> 10);\n"
            "        uint mantissa = val & 0x03FFu;\n"
            "        float f32 = 0.0;\n"
            "        if(exponent == 0)\n"
            "        {\n"
            "            if (mantissa != 0u)\n"
            "            {\n"
            "                const float scale = 1.0 / (1 << 24);\n"
            "                f32 = scale * mantissa;\n"
            "            }\n"
            "        }\n"
            "        else if (exponent == 31)\n"
            "        {\n"
            "            return uintBitsToFloat(sign | 0x7F800000u | mantissa);\n"
            "        }\n"
            "        else\n"
            "        {\n"
            "            exponent -= 15;\n"
            "            float scale;\n"
            "            if(exponent < 0)\n"
            "            {\n"
            "                // The negative unary operator is buggy on OSX.\n"
            "                // Work around this by using abs instead.\n"
            "                scale = 1.0 / (1 << abs(exponent));\n"
            "            }\n"
            "            else\n"
            "            {\n"
            "                scale = 1 << exponent;\n"
            "            }\n"
            "            float decimal = 1.0 + float(mantissa) / float(1 << 10);\n"
            "            f32 = scale * decimal;\n"
            "        }\n"
            "\n"
            "        if (sign != 0u)\n"
            "        {\n"
            "            f32 = -f32;\n"
            "        }\n"
            "\n"
            "        return f32;\n"
            "    }\n"
            "#endif\n"
            "\n"
            "vec2 unpackHalf2x16_emu(uint u)\n"
            "{\n"
            "    #if defined(GL_ARB_shading_language_packing)\n"
            "        return unpackHalf2x16(u);\n"
            "    #else\n"
            "        uint y = (u >> 16);\n"
            "        uint x = u & 0xFFFFu;\n"
            "        return vec2(f16tof32(x), f16tof32(y));\n"
            "    #endif\n"
            "}\n");
        // clang-format on
    }
}

}  // namespace sh
