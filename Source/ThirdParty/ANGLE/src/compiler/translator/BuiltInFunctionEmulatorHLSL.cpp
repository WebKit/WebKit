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

// Defined in emulated_builtin_functions_hlsl_autogen.cpp.
const char *FindHLSLFunction(const FunctionId &functionID);

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
                             "bool isnan_emu(float x)\n"
                             "{\n"
                             "    return (x > 0.0 || x < 0.0) ? false : x != 0.0;\n"
                             "}\n"
                             "\n");

    emu->addEmulatedFunction(
        EOpIsNan, float2,
        "bool2 isnan_emu(float2 x)\n"
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
        "bool3 isnan_emu(float3 x)\n"
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
        "bool4 isnan_emu(float4 x)\n"
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
    TType *int1   = new TType(EbtInt);
    TType *int2   = new TType(EbtInt, 2);
    TType *int3   = new TType(EbtInt, 3);
    TType *int4   = new TType(EbtInt, 4);
    TType *uint1  = new TType(EbtUInt);
    TType *uint2  = new TType(EbtUInt, 2);
    TType *uint3  = new TType(EbtUInt, 3);
    TType *uint4  = new TType(EbtUInt, 4);

    emu->addFunctionMap(FindHLSLFunction);

    // (a + b2^16) * (c + d2^16) = ac + (ad + bc) * 2^16 + bd * 2^32
    // Also note that below, a * d + ((a * c) >> 16) is guaranteed not to overflow, because:
    // a <= 0xffff, d <= 0xffff, ((a * c) >> 16) <= 0xffff and 0xffff * 0xffff + 0xffff = 0xffff0000
    FunctionId umulExtendedUint1 = emu->addEmulatedFunction(
        EOpUmulExtended, uint1, uint1, uint1, uint1,
        "void umulExtended_emu(uint x, uint y, out uint msb, out uint lsb)\n"
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
        "void umulExtended_emu(uint2 x, uint2 y, out uint2 msb, out uint2 lsb)\n"
        "{\n"
        "    umulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    umulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "}\n");
    emu->addEmulatedFunctionWithDependency(
        umulExtendedUint1, EOpUmulExtended, uint3, uint3, uint3, uint3,
        "void umulExtended_emu(uint3 x, uint3 y, out uint3 msb, out uint3 lsb)\n"
        "{\n"
        "    umulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    umulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "    umulExtended_emu(x.z, y.z, msb.z, lsb.z);\n"
        "}\n");
    emu->addEmulatedFunctionWithDependency(
        umulExtendedUint1, EOpUmulExtended, uint4, uint4, uint4, uint4,
        "void umulExtended_emu(uint4 x, uint4 y, out uint4 msb, out uint4 lsb)\n"
        "{\n"
        "    umulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    umulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "    umulExtended_emu(x.z, y.z, msb.z, lsb.z);\n"
        "    umulExtended_emu(x.w, y.w, msb.w, lsb.w);\n"
        "}\n");

    // The imul emulation does two's complement negation on the lsb and msb manually in case the
    // result needs to be negative.
    // TODO(oetuaho): Note that this code doesn't take one edge case into account, where x or y is
    // -2^31. abs(-2^31) is undefined.
    FunctionId imulExtendedInt1 = emu->addEmulatedFunctionWithDependency(
        umulExtendedUint1, EOpImulExtended, int1, int1, int1, int1,
        "void imulExtended_emu(int x, int y, out int msb, out int lsb)\n"
        "{\n"
        "    uint unsignedMsb;\n"
        "    uint unsignedLsb;\n"
        "    bool negative = (x < 0) != (y < 0);\n"
        "    umulExtended_emu(uint(abs(x)), uint(abs(y)), unsignedMsb, unsignedLsb);\n"
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
        "void imulExtended_emu(int2 x, int2 y, out int2 msb, out int2 lsb)\n"
        "{\n"
        "    imulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    imulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "}\n");
    emu->addEmulatedFunctionWithDependency(
        imulExtendedInt1, EOpImulExtended, int3, int3, int3, int3,
        "void imulExtended_emu(int3 x, int3 y, out int3 msb, out int3 lsb)\n"
        "{\n"
        "    imulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    imulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "    imulExtended_emu(x.z, y.z, msb.z, lsb.z);\n"
        "}\n");
    emu->addEmulatedFunctionWithDependency(
        imulExtendedInt1, EOpImulExtended, int4, int4, int4, int4,
        "void imulExtended_emu(int4 x, int4 y, out int4 msb, out int4 lsb)\n"
        "{\n"
        "    imulExtended_emu(x.x, y.x, msb.x, lsb.x);\n"
        "    imulExtended_emu(x.y, y.y, msb.y, lsb.y);\n"
        "    imulExtended_emu(x.z, y.z, msb.z, lsb.z);\n"
        "    imulExtended_emu(x.w, y.w, msb.w, lsb.w);\n"
        "}\n");
}

}  // namespace sh
