//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Create symbols that declare built-in definitions, add built-ins that
// cannot be expressed in the files, and establish mappings between
// built-in functions and operators.
//

#include "compiler/translator/Initialize.h"
#include "compiler/translator/Cache.h"

#include "compiler/translator/IntermNode.h"
#include "angle_gl.h"

namespace sh
{

void InsertBuiltInFunctions(sh::GLenum type,
                            ShShaderSpec spec,
                            const ShBuiltInResources &resources,
                            TSymbolTable &symbolTable)
{
    const TType *voidType = TCache::getType(EbtVoid);
    const TType *float1   = TCache::getType(EbtFloat);
    const TType *float2   = TCache::getType(EbtFloat, 2);
    const TType *float3   = TCache::getType(EbtFloat, 3);
    const TType *float4   = TCache::getType(EbtFloat, 4);
    const TType *int1     = TCache::getType(EbtInt);
    const TType *int2     = TCache::getType(EbtInt, 2);
    const TType *int3     = TCache::getType(EbtInt, 3);
    const TType *uint1    = TCache::getType(EbtUInt);
    const TType *bool1    = TCache::getType(EbtBool);
    const TType *genType  = TCache::getType(EbtGenType);
    const TType *genIType = TCache::getType(EbtGenIType);
    const TType *genUType = TCache::getType(EbtGenUType);
    const TType *genBType = TCache::getType(EbtGenBType);

    //
    // Angle and Trigonometric Functions.
    //
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpRadians, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpDegrees, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpSin, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpCos, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpTan, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpAsin, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpAcos, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpAtan, genType, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpAtan, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpSinh, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpCosh, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTanh, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpAsinh, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpAcosh, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpAtanh, genType, genType);

    //
    // Exponential Functions.
    //
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpPow, genType, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpExp, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpLog, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpExp2, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpLog2, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpSqrt, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpInverseSqrt, genType, genType);

    //
    // Common Functions.
    //
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpAbs, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpAbs, genIType, genIType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpSign, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpSign, genIType, genIType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpFloor, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTrunc, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpRound, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpRoundEven, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpCeil, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpFract, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMod, genType, genType, float1);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMod, genType, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMin, genType, genType, float1);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMin, genType, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMin, genIType, genIType, genIType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMin, genIType, genIType, int1);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMin, genUType, genUType, genUType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMin, genUType, genUType, uint1);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMax, genType, genType, float1);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMax, genType, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMax, genIType, genIType, genIType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMax, genIType, genIType, int1);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMax, genUType, genUType, genUType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMax, genUType, genUType, uint1);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpClamp, genType, genType, float1, float1);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpClamp, genType, genType, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpClamp, genIType, genIType, int1, int1);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpClamp, genIType, genIType, genIType, genIType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpClamp, genUType, genUType, uint1, uint1);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpClamp, genUType, genUType, genUType, genUType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMix, genType, genType, genType, float1);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMix, genType, genType, genType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMix, genType, genType, genType, genBType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpStep, genType, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpStep, genType, float1, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpSmoothStep, genType, genType, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpSmoothStep, genType, float1, float1, genType);

    const TType *outGenType = TCache::getType(EbtGenType, EvqOut);
    const TType *outGenIType = TCache::getType(EbtGenIType, EvqOut);

    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpModf, genType, genType, outGenType);

    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpIsNan, genBType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpIsInf, genBType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpFloatBitsToInt, genIType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpFloatBitsToUint, genUType, genType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpIntBitsToFloat, genType, genIType);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpUintBitsToFloat, genType, genUType);

    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpFrexp, genType, genType, outGenIType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpLdexp, genType, genType, genIType);

    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpPackSnorm2x16, uint1, float2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpPackUnorm2x16, uint1, float2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpPackHalf2x16, uint1, float2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpUnpackSnorm2x16, float2, uint1);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpUnpackUnorm2x16, float2, uint1);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpUnpackHalf2x16, float2, uint1);

    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpPackUnorm4x8, uint1, float4);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpPackSnorm4x8, uint1, float4);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpUnpackUnorm4x8, float4, uint1);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpUnpackSnorm4x8, float4, uint1);

    //
    // Geometric Functions.
    //
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpLength, float1, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpDistance, float1, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpDot, float1, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpCross, float3, float3, float3);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpNormalize, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpFaceforward, genType, genType, genType,
                                genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpReflect, genType, genType, genType);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpRefract, genType, genType, genType, float1);

    const TType *mat2   = TCache::getType(EbtFloat, 2, 2);
    const TType *mat3   = TCache::getType(EbtFloat, 3, 3);
    const TType *mat4   = TCache::getType(EbtFloat, 4, 4);
    const TType *mat2x3 = TCache::getType(EbtFloat, 2, 3);
    const TType *mat3x2 = TCache::getType(EbtFloat, 3, 2);
    const TType *mat2x4 = TCache::getType(EbtFloat, 2, 4);
    const TType *mat4x2 = TCache::getType(EbtFloat, 4, 2);
    const TType *mat3x4 = TCache::getType(EbtFloat, 3, 4);
    const TType *mat4x3 = TCache::getType(EbtFloat, 4, 3);

    //
    // Matrix Functions.
    //
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMulMatrixComponentWise, mat2, mat2, mat2);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMulMatrixComponentWise, mat3, mat3, mat3);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpMulMatrixComponentWise, mat4, mat4, mat4);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMulMatrixComponentWise, mat2x3, mat2x3, mat2x3);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMulMatrixComponentWise, mat3x2, mat3x2, mat3x2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMulMatrixComponentWise, mat2x4, mat2x4, mat2x4);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMulMatrixComponentWise, mat4x2, mat4x2, mat4x2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMulMatrixComponentWise, mat3x4, mat3x4, mat3x4);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpMulMatrixComponentWise, mat4x3, mat4x3, mat4x3);

    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpOuterProduct, mat2, float2, float2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpOuterProduct, mat3, float3, float3);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpOuterProduct, mat4, float4, float4);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpOuterProduct, mat2x3, float3, float2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpOuterProduct, mat3x2, float2, float3);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpOuterProduct, mat2x4, float4, float2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpOuterProduct, mat4x2, float2, float4);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpOuterProduct, mat3x4, float4, float3);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpOuterProduct, mat4x3, float3, float4);

    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTranspose, mat2, mat2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTranspose, mat3, mat3);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTranspose, mat4, mat4);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTranspose, mat2x3, mat3x2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTranspose, mat3x2, mat2x3);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTranspose, mat2x4, mat4x2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTranspose, mat4x2, mat2x4);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTranspose, mat3x4, mat4x3);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpTranspose, mat4x3, mat3x4);

    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpDeterminant, float1, mat2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpDeterminant, float1, mat3);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpDeterminant, float1, mat4);

    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpInverse, mat2, mat2);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpInverse, mat3, mat3);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpInverse, mat4, mat4);

    const TType *vec  = TCache::getType(EbtVec);
    const TType *ivec = TCache::getType(EbtIVec);
    const TType *uvec = TCache::getType(EbtUVec);
    const TType *bvec = TCache::getType(EbtBVec);

    //
    // Vector relational functions.
    //
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpLessThanComponentWise, bvec, vec, vec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpLessThanComponentWise, bvec, ivec, ivec);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpLessThanComponentWise, bvec, uvec, uvec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpLessThanEqualComponentWise, bvec, vec, vec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpLessThanEqualComponentWise, bvec, ivec, ivec);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpLessThanEqualComponentWise, bvec, uvec, uvec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpGreaterThanComponentWise, bvec, vec, vec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpGreaterThanComponentWise, bvec, ivec, ivec);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpGreaterThanComponentWise, bvec, uvec, uvec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpGreaterThanEqualComponentWise, bvec, vec, vec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpGreaterThanEqualComponentWise, bvec, ivec,
                                ivec);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpGreaterThanEqualComponentWise, bvec, uvec, uvec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpEqualComponentWise, bvec, vec, vec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpEqualComponentWise, bvec, ivec, ivec);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpEqualComponentWise, bvec, uvec, uvec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpEqualComponentWise, bvec, bvec, bvec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpNotEqualComponentWise, bvec, vec, vec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpNotEqualComponentWise, bvec, ivec, ivec);
    symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpNotEqualComponentWise, bvec, uvec, uvec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpNotEqualComponentWise, bvec, bvec, bvec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpAny, bool1, bvec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpAll, bool1, bvec);
    symbolTable.insertBuiltInOp(COMMON_BUILTINS, EOpLogicalNotComponentWise, bvec, bvec);

    //
    // Integer functions
    //
    const TType *outGenUType = TCache::getType(EbtGenUType, EvqOut);

    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpBitfieldExtract, genIType, genIType, int1,
                                int1);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpBitfieldExtract, genUType, genUType, int1,
                                int1);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpBitfieldInsert, genIType, genIType, genIType,
                                int1, int1);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpBitfieldInsert, genUType, genUType, genUType,
                                int1, int1);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpBitfieldReverse, genIType, genIType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpBitfieldReverse, genUType, genUType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpBitCount, genIType, genIType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpBitCount, genIType, genUType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpFindLSB, genIType, genIType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpFindLSB, genIType, genUType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpFindMSB, genIType, genIType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpFindMSB, genIType, genUType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpUaddCarry, genUType, genUType, genUType,
                                outGenUType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpUsubBorrow, genUType, genUType, genUType,
                                outGenUType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpUmulExtended, voidType, genUType, genUType,
                                outGenUType, outGenUType);
    symbolTable.insertBuiltInOp(ESSL3_1_BUILTINS, EOpImulExtended, voidType, genIType, genIType,
                                outGenIType, outGenIType);

    const TType *sampler2D   = TCache::getType(EbtSampler2D);
    const TType *samplerCube = TCache::getType(EbtSamplerCube);

    //
    // Texture Functions for GLSL ES 1.0
    //
    symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2D", sampler2D, float2);
    symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DProj", sampler2D, float3);
    symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DProj", sampler2D, float4);
    symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "textureCube", samplerCube, float3);

    if (resources.OES_EGL_image_external || resources.NV_EGL_stream_consumer_external)
    {
        const TType *samplerExternalOES = TCache::getType(EbtSamplerExternalOES);

        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2D", samplerExternalOES, float2);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DProj", samplerExternalOES,
                                  float3);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DProj", samplerExternalOES,
                                  float4);
    }

    if (resources.ARB_texture_rectangle)
    {
        const TType *sampler2DRect = TCache::getType(EbtSampler2DRect);

        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DRect", sampler2DRect, float2);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DRectProj", sampler2DRect,
                                  float3);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DRectProj", sampler2DRect,
                                  float4);
    }

    if (resources.EXT_shader_texture_lod)
    {
        /* The *Grad* variants are new to both vertex and fragment shaders; the fragment
         * shader specific pieces are added separately below.
         */
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, TExtension::EXT_shader_texture_lod, float4,
                                  "texture2DGradEXT", sampler2D, float2, float2, float2);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, TExtension::EXT_shader_texture_lod, float4,
                                  "texture2DProjGradEXT", sampler2D, float3, float2, float2);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, TExtension::EXT_shader_texture_lod, float4,
                                  "texture2DProjGradEXT", sampler2D, float4, float2, float2);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, TExtension::EXT_shader_texture_lod, float4,
                                  "textureCubeGradEXT", samplerCube, float3, float3, float3);
    }

    if (type == GL_FRAGMENT_SHADER)
    {
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2D", sampler2D, float2, float1);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DProj", sampler2D, float3,
                                  float1);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DProj", sampler2D, float4,
                                  float1);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "textureCube", samplerCube, float3,
                                  float1);

        if (resources.OES_standard_derivatives)
        {
            symbolTable.insertBuiltInOp(ESSL1_BUILTINS, EOpDFdx,
                                        TExtension::OES_standard_derivatives, genType, genType);
            symbolTable.insertBuiltInOp(ESSL1_BUILTINS, EOpDFdy,
                                        TExtension::OES_standard_derivatives, genType, genType);
            symbolTable.insertBuiltInOp(ESSL1_BUILTINS, EOpFwidth,
                                        TExtension::OES_standard_derivatives, genType, genType);
        }

        if (resources.EXT_shader_texture_lod)
        {
            symbolTable.insertBuiltIn(ESSL1_BUILTINS, TExtension::EXT_shader_texture_lod, float4,
                                      "texture2DLodEXT", sampler2D, float2, float1);
            symbolTable.insertBuiltIn(ESSL1_BUILTINS, TExtension::EXT_shader_texture_lod, float4,
                                      "texture2DProjLodEXT", sampler2D, float3, float1);
            symbolTable.insertBuiltIn(ESSL1_BUILTINS, TExtension::EXT_shader_texture_lod, float4,
                                      "texture2DProjLodEXT", sampler2D, float4, float1);
            symbolTable.insertBuiltIn(ESSL1_BUILTINS, TExtension::EXT_shader_texture_lod, float4,
                                      "textureCubeLodEXT", samplerCube, float3, float1);
        }
    }

    if (type == GL_VERTEX_SHADER)
    {
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DLod", sampler2D, float2,
                                  float1);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DProjLod", sampler2D, float3,
                                  float1);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "texture2DProjLod", sampler2D, float4,
                                  float1);
        symbolTable.insertBuiltIn(ESSL1_BUILTINS, float4, "textureCubeLod", samplerCube, float3,
                                  float1);
    }

    const TType *gvec4 = TCache::getType(EbtGVec4);

    const TType *gsampler2D      = TCache::getType(EbtGSampler2D);
    const TType *gsamplerCube    = TCache::getType(EbtGSamplerCube);
    const TType *gsampler3D      = TCache::getType(EbtGSampler3D);
    const TType *gsampler2DArray = TCache::getType(EbtGSampler2DArray);
    const TType *gsampler2DMS    = TCache::getType(EbtGSampler2DMS);

    //
    // Texture Functions for GLSL ES 3.0
    //
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texture", gsampler2D, float2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texture", gsampler3D, float3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texture", gsamplerCube, float3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texture", gsampler2DArray, float3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProj", gsampler2D, float3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProj", gsampler2D, float4);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProj", gsampler3D, float4);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureLod", gsampler2D, float2, float1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureLod", gsampler3D, float3, float1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureLod", gsamplerCube, float3, float1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureLod", gsampler2DArray, float3, float1);

    if (resources.OES_EGL_image_external_essl3)
    {
        const TType *samplerExternalOES = TCache::getType(EbtSamplerExternalOES);

        symbolTable.insertBuiltIn(ESSL3_BUILTINS, float4, "texture", samplerExternalOES, float2);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, float4, "textureProj", samplerExternalOES,
                                  float3);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, float4, "textureProj", samplerExternalOES,
                                  float4);
    }

    if (resources.EXT_YUV_target)
    {
        const TType *samplerExternal2DY2YEXT = TCache::getType(EbtSamplerExternal2DY2YEXT);

        symbolTable.insertBuiltIn(ESSL3_BUILTINS, TExtension::EXT_YUV_target, float4, "texture",
                                  samplerExternal2DY2YEXT, float2);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, TExtension::EXT_YUV_target, float4, "textureProj",
                                  samplerExternal2DY2YEXT, float3);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, TExtension::EXT_YUV_target, float4, "textureProj",
                                  samplerExternal2DY2YEXT, float4);

        const TType *yuvCscStandardEXT = TCache::getType(EbtYuvCscStandardEXT);

        symbolTable.insertBuiltIn(ESSL3_BUILTINS, TExtension::EXT_YUV_target, float3, "rgb_2_yuv",
                                  float3, yuvCscStandardEXT);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, TExtension::EXT_YUV_target, float3, "yuv_2_rgb",
                                  float3, yuvCscStandardEXT);
    }

    if (type == GL_FRAGMENT_SHADER)
    {
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texture", gsampler2D, float2, float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texture", gsampler3D, float3, float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texture", gsamplerCube, float3, float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texture", gsampler2DArray, float3,
                                  float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProj", gsampler2D, float3, float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProj", gsampler2D, float4, float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProj", gsampler3D, float4, float1);

        if (resources.OES_EGL_image_external_essl3)
        {
            const TType *samplerExternalOES = TCache::getType(EbtSamplerExternalOES);

            symbolTable.insertBuiltIn(ESSL3_BUILTINS, float4, "texture", samplerExternalOES, float2,
                                      float1);
            symbolTable.insertBuiltIn(ESSL3_BUILTINS, float4, "textureProj", samplerExternalOES,
                                      float3, float1);
            symbolTable.insertBuiltIn(ESSL3_BUILTINS, float4, "textureProj", samplerExternalOES,
                                      float4, float1);
        }

        if (resources.EXT_YUV_target)
        {
            const TType *samplerExternal2DY2YEXT = TCache::getType(EbtSamplerExternal2DY2YEXT);

            symbolTable.insertBuiltIn(ESSL3_BUILTINS, TExtension::EXT_YUV_target, float4, "texture",
                                      samplerExternal2DY2YEXT, float2, float1);
            symbolTable.insertBuiltIn(ESSL3_BUILTINS, TExtension::EXT_YUV_target, float4,
                                      "textureProj", samplerExternal2DY2YEXT, float3, float1);
            symbolTable.insertBuiltIn(ESSL3_BUILTINS, TExtension::EXT_YUV_target, float4,
                                      "textureProj", samplerExternal2DY2YEXT, float4, float1);
        }
    }

    const TType *sampler2DShadow      = TCache::getType(EbtSampler2DShadow);
    const TType *samplerCubeShadow    = TCache::getType(EbtSamplerCubeShadow);
    const TType *sampler2DArrayShadow = TCache::getType(EbtSampler2DArrayShadow);

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "texture", sampler2DShadow, float3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "texture", samplerCubeShadow, float4);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "texture", sampler2DArrayShadow, float4);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureProj", sampler2DShadow, float4);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureLod", sampler2DShadow, float3,
                              float1);

    if (type == GL_FRAGMENT_SHADER)
    {
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "texture", sampler2DShadow, float3,
                                  float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "texture", samplerCubeShadow, float4,
                                  float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureProj", sampler2DShadow, float4,
                                  float1);
    }

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, int2, "textureSize", gsampler2D, int1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, int3, "textureSize", gsampler3D, int1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, int2, "textureSize", gsamplerCube, int1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, int3, "textureSize", gsampler2DArray, int1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, int2, "textureSize", sampler2DShadow, int1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, int2, "textureSize", samplerCubeShadow, int1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, int3, "textureSize", sampler2DArrayShadow, int1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, int2, "textureSize", gsampler2DMS);

    if (resources.OES_EGL_image_external_essl3)
    {
        const TType *samplerExternalOES = TCache::getType(EbtSamplerExternalOES);

        symbolTable.insertBuiltIn(ESSL3_BUILTINS, int2, "textureSize", samplerExternalOES, int1);
    }

    if (resources.EXT_YUV_target)
    {
        const TType *samplerExternal2DY2YEXT = TCache::getType(EbtSamplerExternal2DY2YEXT);

        symbolTable.insertBuiltIn(ESSL3_BUILTINS, TExtension::EXT_YUV_target, int2, "textureSize",
                                  samplerExternal2DY2YEXT, int1);
    }

    if (type == GL_FRAGMENT_SHADER)
    {
        symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpDFdx, genType, genType);
        symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpDFdy, genType, genType);
        symbolTable.insertBuiltInOp(ESSL3_BUILTINS, EOpFwidth, genType, genType);
    }

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureOffset", gsampler2D, float2, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureOffset", gsampler3D, float3, int3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureOffset", sampler2DShadow, float3,
                              int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureOffset", gsampler2DArray, float3,
                              int2);

    if (type == GL_FRAGMENT_SHADER)
    {
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureOffset", gsampler2D, float2, int2,
                                  float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureOffset", gsampler3D, float3, int3,
                                  float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureOffset", sampler2DShadow, float3,
                                  int2, float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureOffset", gsampler2DArray, float3,
                                  int2, float1);
    }

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjOffset", gsampler2D, float3, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjOffset", gsampler2D, float4, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjOffset", gsampler3D, float4, int3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureProjOffset", sampler2DShadow, float4,
                              int2);

    if (type == GL_FRAGMENT_SHADER)
    {
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjOffset", gsampler2D, float3,
                                  int2, float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjOffset", gsampler2D, float4,
                                  int2, float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjOffset", gsampler3D, float4,
                                  int3, float1);
        symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureProjOffset", sampler2DShadow,
                                  float4, int2, float1);
    }

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureLodOffset", gsampler2D, float2, float1,
                              int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureLodOffset", gsampler3D, float3, float1,
                              int3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureLodOffset", sampler2DShadow, float3,
                              float1, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureLodOffset", gsampler2DArray, float3,
                              float1, int2);

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjLod", gsampler2D, float3, float1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjLod", gsampler2D, float4, float1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjLod", gsampler3D, float4, float1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureProjLod", sampler2DShadow, float4,
                              float1);

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjLodOffset", gsampler2D, float3,
                              float1, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjLodOffset", gsampler2D, float4,
                              float1, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjLodOffset", gsampler3D, float4,
                              float1, int3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureProjLodOffset", sampler2DShadow,
                              float4, float1, int2);

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texelFetch", gsampler2D, int2, int1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texelFetch", gsampler3D, int3, int1);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texelFetch", gsampler2DArray, int3, int1);

    if (resources.OES_EGL_image_external_essl3)
    {
        const TType *samplerExternalOES = TCache::getType(EbtSamplerExternalOES);

        symbolTable.insertBuiltIn(ESSL3_BUILTINS, float4, "texelFetch", samplerExternalOES, int2,
                                  int1);
    }

    if (resources.EXT_YUV_target)
    {
        const TType *samplerExternal2DY2YEXT = TCache::getType(EbtSamplerExternal2DY2YEXT);

        symbolTable.insertBuiltIn(ESSL3_BUILTINS, TExtension::EXT_YUV_target, float4, "texelFetch",
                                  samplerExternal2DY2YEXT, int2, int1);
    }

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texelFetchOffset", gsampler2D, int2, int1,
                              int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texelFetchOffset", gsampler3D, int3, int1,
                              int3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "texelFetchOffset", gsampler2DArray, int3,
                              int1, int2);

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureGrad", gsampler2D, float2, float2,
                              float2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureGrad", gsampler3D, float3, float3,
                              float3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureGrad", gsamplerCube, float3, float3,
                              float3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureGrad", sampler2DShadow, float3,
                              float2, float2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureGrad", samplerCubeShadow, float4,
                              float3, float3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureGrad", gsampler2DArray, float3, float2,
                              float2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureGrad", sampler2DArrayShadow, float4,
                              float2, float2);

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureGradOffset", gsampler2D, float2,
                              float2, float2, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureGradOffset", gsampler3D, float3,
                              float3, float3, int3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureGradOffset", sampler2DShadow, float3,
                              float2, float2, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureGradOffset", gsampler2DArray, float3,
                              float2, float2, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureGradOffset", sampler2DArrayShadow,
                              float4, float2, float2, int2);

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjGrad", gsampler2D, float3, float2,
                              float2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjGrad", gsampler2D, float4, float2,
                              float2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjGrad", gsampler3D, float4, float3,
                              float3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureProjGrad", sampler2DShadow, float4,
                              float2, float2);

    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjGradOffset", gsampler2D, float3,
                              float2, float2, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjGradOffset", gsampler2D, float4,
                              float2, float2, int2);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, gvec4, "textureProjGradOffset", gsampler3D, float4,
                              float3, float3, int3);
    symbolTable.insertBuiltIn(ESSL3_BUILTINS, float1, "textureProjGradOffset", sampler2DShadow,
                              float4, float2, float2, int2);

    const TType *atomicCounter = TCache::getType(EbtAtomicCounter);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicCounter", atomicCounter);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicCounterIncrement", atomicCounter);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicCounterDecrement", atomicCounter);

    // Insert all atomic memory functions
    const TType *int1InOut  = TCache::getType(EbtInt, EvqInOut);
    const TType *uint1InOut = TCache::getType(EbtUInt, EvqInOut);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicAdd", uint1InOut, uint1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int1, "atomicAdd", int1InOut, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicMin", uint1InOut, uint1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int1, "atomicMin", int1InOut, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicMax", uint1InOut, uint1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int1, "atomicMax", int1InOut, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicAnd", uint1InOut, uint1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int1, "atomicAnd", int1InOut, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicOr", uint1InOut, uint1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int1, "atomicOr", int1InOut, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicXor", uint1InOut, uint1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int1, "atomicXor", int1InOut, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicExchange", uint1InOut, uint1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int1, "atomicExchange", int1InOut, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, uint1, "atomicCompSwap", uint1InOut, uint1, uint1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int1, "atomicCompSwap", int1InOut, int1, int1);

    const TType *gimage2D      = TCache::getType(EbtGImage2D);
    const TType *gimage3D      = TCache::getType(EbtGImage3D);
    const TType *gimage2DArray = TCache::getType(EbtGImage2DArray);
    const TType *gimageCube    = TCache::getType(EbtGImageCube);

    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, voidType, "imageStore", gimage2D, int2, gvec4);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, voidType, "imageStore", gimage3D, int3, gvec4);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, voidType, "imageStore", gimage2DArray, int3, gvec4);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, voidType, "imageStore", gimageCube, int3, gvec4);

    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "imageLoad", gimage2D, int2);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "imageLoad", gimage3D, int3);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "imageLoad", gimage2DArray, int3);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "imageLoad", gimageCube, int3);

    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int2, "imageSize", gimage2D);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int3, "imageSize", gimage3D);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int3, "imageSize", gimage2DArray);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, int2, "imageSize", gimageCube);

    symbolTable.insertBuiltInFunctionNoParameters(ESSL3_1_BUILTINS, EOpMemoryBarrier, voidType,
                                                  "memoryBarrier");
    symbolTable.insertBuiltInFunctionNoParameters(ESSL3_1_BUILTINS, EOpMemoryBarrierAtomicCounter,
                                                  voidType, "memoryBarrierAtomicCounter");
    symbolTable.insertBuiltInFunctionNoParameters(ESSL3_1_BUILTINS, EOpMemoryBarrierBuffer,
                                                  voidType, "memoryBarrierBuffer");
    symbolTable.insertBuiltInFunctionNoParameters(ESSL3_1_BUILTINS, EOpMemoryBarrierImage, voidType,
                                                  "memoryBarrierImage");

    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "texelFetch", gsampler2DMS, int2, int1);

    // Insert all variations of textureGather.
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "textureGather", gsampler2D, float2);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "textureGather", gsampler2D, float2, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "textureGather", gsampler2DArray, float3);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "textureGather", gsampler2DArray, float3,
                              int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "textureGather", gsamplerCube, float3);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "textureGather", gsamplerCube, float3, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, float4, "textureGather", sampler2DShadow, float2);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, float4, "textureGather", sampler2DShadow, float2,
                              float1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, float4, "textureGather", sampler2DArrayShadow,
                              float3);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, float4, "textureGather", sampler2DArrayShadow,
                              float3, float1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, float4, "textureGather", samplerCubeShadow, float3);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, float4, "textureGather", samplerCubeShadow, float3,
                              float1);

    // Insert all variations of textureGatherOffset.
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "textureGatherOffset", gsampler2D, float2,
                              int2);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "textureGatherOffset", gsampler2D, float2,
                              int2, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "textureGatherOffset", gsampler2DArray,
                              float3, int2);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, gvec4, "textureGatherOffset", gsampler2DArray,
                              float3, int2, int1);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, float4, "textureGatherOffset", sampler2DShadow,
                              float2, float1, int2);
    symbolTable.insertBuiltIn(ESSL3_1_BUILTINS, float4, "textureGatherOffset", sampler2DArrayShadow,
                              float3, float1, int2);

    if (type == GL_COMPUTE_SHADER)
    {
        symbolTable.insertBuiltInFunctionNoParameters(ESSL3_1_BUILTINS, EOpBarrier, voidType,
                                                      "barrier");
        symbolTable.insertBuiltInFunctionNoParameters(ESSL3_1_BUILTINS, EOpMemoryBarrierShared,
                                                      voidType, "memoryBarrierShared");
        symbolTable.insertBuiltInFunctionNoParameters(ESSL3_1_BUILTINS, EOpGroupMemoryBarrier,
                                                      voidType, "groupMemoryBarrier");
    }

    if (type == GL_GEOMETRY_SHADER_OES)
    {
        TExtension extension = TExtension::OES_geometry_shader;
        symbolTable.insertBuiltInFunctionNoParametersExt(ESSL3_1_BUILTINS, extension, EOpEmitVertex,
                                                         voidType, "EmitVertex");
        symbolTable.insertBuiltInFunctionNoParametersExt(ESSL3_1_BUILTINS, extension,
                                                         EOpEndPrimitive, voidType, "EndPrimitive");
    }

    //
    // Depth range in window coordinates
    //
    TFieldList *fields       = NewPoolTFieldList();
    TSourceLoc zeroSourceLoc = {0, 0, 0, 0};
    auto highpFloat1         = new TType(EbtFloat, EbpHigh, EvqGlobal, 1);
    TField *near             = new TField(highpFloat1, NewPoolTString("near"), zeroSourceLoc);
    TField *far              = new TField(highpFloat1, NewPoolTString("far"), zeroSourceLoc);
    TField *diff             = new TField(highpFloat1, NewPoolTString("diff"), zeroSourceLoc);
    fields->push_back(near);
    fields->push_back(far);
    fields->push_back(diff);
    TStructure *depthRangeStruct =
        new TStructure(&symbolTable, NewPoolTString("gl_DepthRangeParameters"), fields);
    symbolTable.insertStructType(COMMON_BUILTINS, depthRangeStruct);
    TType depthRangeType(depthRangeStruct);
    depthRangeType.setQualifier(EvqUniform);
    symbolTable.insertVariable(COMMON_BUILTINS, "gl_DepthRange", depthRangeType);

    //
    // Implementation dependent built-in constants.
    //
    symbolTable.insertConstInt(COMMON_BUILTINS, "gl_MaxVertexAttribs", resources.MaxVertexAttribs,
                               EbpMedium);
    symbolTable.insertConstInt(COMMON_BUILTINS, "gl_MaxVertexUniformVectors",
                               resources.MaxVertexUniformVectors, EbpMedium);
    symbolTable.insertConstInt(COMMON_BUILTINS, "gl_MaxVertexTextureImageUnits",
                               resources.MaxVertexTextureImageUnits, EbpMedium);
    symbolTable.insertConstInt(COMMON_BUILTINS, "gl_MaxCombinedTextureImageUnits",
                               resources.MaxCombinedTextureImageUnits, EbpMedium);
    symbolTable.insertConstInt(COMMON_BUILTINS, "gl_MaxTextureImageUnits",
                               resources.MaxTextureImageUnits, EbpMedium);
    symbolTable.insertConstInt(COMMON_BUILTINS, "gl_MaxFragmentUniformVectors",
                               resources.MaxFragmentUniformVectors, EbpMedium);

    symbolTable.insertConstInt(ESSL1_BUILTINS, "gl_MaxVaryingVectors", resources.MaxVaryingVectors,
                               EbpMedium);

    symbolTable.insertConstInt(COMMON_BUILTINS, "gl_MaxDrawBuffers", resources.MaxDrawBuffers,
                               EbpMedium);
    if (resources.EXT_blend_func_extended)
    {
        symbolTable.insertConstIntExt(COMMON_BUILTINS, TExtension::EXT_blend_func_extended,
                                      "gl_MaxDualSourceDrawBuffersEXT",
                                      resources.MaxDualSourceDrawBuffers, EbpMedium);
    }

    symbolTable.insertConstInt(ESSL3_BUILTINS, "gl_MaxVertexOutputVectors",
                               resources.MaxVertexOutputVectors, EbpMedium);
    symbolTable.insertConstInt(ESSL3_BUILTINS, "gl_MaxFragmentInputVectors",
                               resources.MaxFragmentInputVectors, EbpMedium);
    symbolTable.insertConstInt(ESSL3_BUILTINS, "gl_MinProgramTexelOffset",
                               resources.MinProgramTexelOffset, EbpMedium);
    symbolTable.insertConstInt(ESSL3_BUILTINS, "gl_MaxProgramTexelOffset",
                               resources.MaxProgramTexelOffset, EbpMedium);

    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxImageUnits", resources.MaxImageUnits,
                               EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxVertexImageUniforms",
                               resources.MaxVertexImageUniforms, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxFragmentImageUniforms",
                               resources.MaxFragmentImageUniforms, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxComputeImageUniforms",
                               resources.MaxComputeImageUniforms, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxCombinedImageUniforms",
                               resources.MaxCombinedImageUniforms, EbpMedium);

    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxCombinedShaderOutputResources",
                               resources.MaxCombinedShaderOutputResources, EbpMedium);

    symbolTable.insertConstIvec3(ESSL3_1_BUILTINS, "gl_MaxComputeWorkGroupCount",
                                 resources.MaxComputeWorkGroupCount, EbpHigh);
    symbolTable.insertConstIvec3(ESSL3_1_BUILTINS, "gl_MaxComputeWorkGroupSize",
                                 resources.MaxComputeWorkGroupSize, EbpHigh);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxComputeUniformComponents",
                               resources.MaxComputeUniformComponents, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxComputeTextureImageUnits",
                               resources.MaxComputeTextureImageUnits, EbpMedium);

    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxComputeAtomicCounters",
                               resources.MaxComputeAtomicCounters, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxComputeAtomicCounterBuffers",
                               resources.MaxComputeAtomicCounterBuffers, EbpMedium);

    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxVertexAtomicCounters",
                               resources.MaxVertexAtomicCounters, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxFragmentAtomicCounters",
                               resources.MaxFragmentAtomicCounters, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxCombinedAtomicCounters",
                               resources.MaxCombinedAtomicCounters, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxAtomicCounterBindings",
                               resources.MaxAtomicCounterBindings, EbpMedium);

    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxVertexAtomicCounterBuffers",
                               resources.MaxVertexAtomicCounterBuffers, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxFragmentAtomicCounterBuffers",
                               resources.MaxFragmentAtomicCounterBuffers, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxCombinedAtomicCounterBuffers",
                               resources.MaxCombinedAtomicCounterBuffers, EbpMedium);
    symbolTable.insertConstInt(ESSL3_1_BUILTINS, "gl_MaxAtomicCounterBufferSize",
                               resources.MaxAtomicCounterBufferSize, EbpMedium);

    if (resources.OES_geometry_shader)
    {
        TExtension ext = TExtension::OES_geometry_shader;
        symbolTable.insertConstIntExt(ESSL3_1_BUILTINS, ext, "gl_MaxGeometryInputComponents",
                                      resources.MaxGeometryInputComponents, EbpMedium);
        symbolTable.insertConstIntExt(ESSL3_1_BUILTINS, ext, "gl_MaxGeometryOutputComponents",
                                      resources.MaxGeometryOutputComponents, EbpMedium);
        symbolTable.insertConstIntExt(ESSL3_1_BUILTINS, ext, "gl_MaxGeometryImageUniforms",
                                      resources.MaxGeometryImageUniforms, EbpMedium);
        symbolTable.insertConstIntExt(ESSL3_1_BUILTINS, ext, "gl_MaxGeometryTextureImageUnits",
                                      resources.MaxGeometryTextureImageUnits, EbpMedium);
        symbolTable.insertConstIntExt(ESSL3_1_BUILTINS, ext, "gl_MaxGeometryOutputVertices",
                                      resources.MaxGeometryOutputVertices, EbpMedium);
        symbolTable.insertConstIntExt(ESSL3_1_BUILTINS, ext, "gl_MaxGeometryTotalOutputComponents",
                                      resources.MaxGeometryTotalOutputComponents, EbpMedium);
        symbolTable.insertConstIntExt(ESSL3_1_BUILTINS, ext, "gl_MaxGeometryUniformComponents",
                                      resources.MaxGeometryUniformComponents, EbpMedium);
        symbolTable.insertConstIntExt(ESSL3_1_BUILTINS, ext, "gl_MaxGeometryAtomicCounters",
                                      resources.MaxGeometryAtomicCounters, EbpMedium);
        symbolTable.insertConstIntExt(ESSL3_1_BUILTINS, ext, "gl_MaxGeometryAtomicCounterBuffers",
                                      resources.MaxGeometryAtomicCounterBuffers, EbpMedium);
    }
}

void IdentifyBuiltIns(sh::GLenum type,
                      ShShaderSpec spec,
                      const ShBuiltInResources &resources,
                      TSymbolTable &symbolTable)
{
    //
    // Insert some special built-in variables that are not in
    // the built-in header files.
    //

    if (resources.OVR_multiview && type != GL_COMPUTE_SHADER)
    {
        symbolTable.insertVariableExt(ESSL3_BUILTINS, TExtension::OVR_multiview, "gl_ViewID_OVR",
                                      TType(EbtUInt, EbpHigh, EvqViewIDOVR, 1));

        // ESSL 1.00 doesn't have unsigned integers, so gl_ViewID_OVR is a signed integer in ESSL
        // 1.00. This is specified in the WEBGL_multiview spec.
        symbolTable.insertVariableExt(ESSL1_BUILTINS, TExtension::OVR_multiview, "gl_ViewID_OVR",
                                      TType(EbtInt, EbpHigh, EvqViewIDOVR, 1));
    }

    switch (type)
    {
        case GL_FRAGMENT_SHADER:
        {
            symbolTable.insertVariable(COMMON_BUILTINS, "gl_FragCoord",
                                       TType(EbtFloat, EbpMedium, EvqFragCoord, 4));
            symbolTable.insertVariable(COMMON_BUILTINS, "gl_FrontFacing",
                                       TType(EbtBool, EbpUndefined, EvqFrontFacing, 1));
            symbolTable.insertVariable(COMMON_BUILTINS, "gl_PointCoord",
                                       TType(EbtFloat, EbpMedium, EvqPointCoord, 2));

            symbolTable.insertVariable(ESSL1_BUILTINS, "gl_FragColor",
                                       TType(EbtFloat, EbpMedium, EvqFragColor, 4));
            TType fragData(EbtFloat, EbpMedium, EvqFragData, 4);
            if (spec != SH_WEBGL2_SPEC && spec != SH_WEBGL3_SPEC)
            {
                fragData.makeArray(resources.MaxDrawBuffers);
            }
            else
            {
                fragData.makeArray(1u);
            }
            symbolTable.insertVariable(ESSL1_BUILTINS, "gl_FragData", fragData);

            if (resources.EXT_blend_func_extended)
            {
                symbolTable.insertVariableExt(
                    ESSL1_BUILTINS, TExtension::EXT_blend_func_extended, "gl_SecondaryFragColorEXT",
                    TType(EbtFloat, EbpMedium, EvqSecondaryFragColorEXT, 4));
                TType secondaryFragData(EbtFloat, EbpMedium, EvqSecondaryFragDataEXT, 4, 1);
                secondaryFragData.makeArray(resources.MaxDualSourceDrawBuffers);
                symbolTable.insertVariableExt(ESSL1_BUILTINS, TExtension::EXT_blend_func_extended,
                                              "gl_SecondaryFragDataEXT", secondaryFragData);
            }

            if (resources.EXT_frag_depth)
            {
                symbolTable.insertVariableExt(
                    ESSL1_BUILTINS, TExtension::EXT_frag_depth, "gl_FragDepthEXT",
                    TType(EbtFloat, resources.FragmentPrecisionHigh ? EbpHigh : EbpMedium,
                          EvqFragDepthEXT, 1));
            }

            symbolTable.insertVariable(ESSL3_BUILTINS, "gl_FragDepth",
                                       TType(EbtFloat, EbpHigh, EvqFragDepth, 1));

            if (resources.EXT_shader_framebuffer_fetch || resources.NV_shader_framebuffer_fetch)
            {
                TType lastFragData(EbtFloat, EbpMedium, EvqLastFragData, 4, 1);
                lastFragData.makeArray(resources.MaxDrawBuffers);

                if (resources.EXT_shader_framebuffer_fetch)
                {
                    symbolTable.insertVariableExt(ESSL1_BUILTINS,
                                                  TExtension::EXT_shader_framebuffer_fetch,
                                                  "gl_LastFragData", lastFragData);
                }
                else if (resources.NV_shader_framebuffer_fetch)
                {
                    symbolTable.insertVariableExt(
                        ESSL1_BUILTINS, TExtension::NV_shader_framebuffer_fetch, "gl_LastFragColor",
                        TType(EbtFloat, EbpMedium, EvqLastFragColor, 4));
                    symbolTable.insertVariableExt(ESSL1_BUILTINS,
                                                  TExtension::NV_shader_framebuffer_fetch,
                                                  "gl_LastFragData", lastFragData);
                }
            }
            else if (resources.ARM_shader_framebuffer_fetch)
            {
                symbolTable.insertVariableExt(
                    ESSL1_BUILTINS, TExtension::ARM_shader_framebuffer_fetch, "gl_LastFragColorARM",
                    TType(EbtFloat, EbpMedium, EvqLastFragColor, 4));
            }

            if (resources.OES_geometry_shader)
            {
                TExtension extension = TExtension::OES_geometry_shader;
                symbolTable.insertVariableExt(ESSL3_1_BUILTINS, extension, "gl_PrimitiveID",
                                              TType(EbtInt, EbpHigh, EvqPrimitiveID, 1));
                symbolTable.insertVariableExt(ESSL3_1_BUILTINS, extension, "gl_Layer",
                                              TType(EbtInt, EbpHigh, EvqLayer, 1));
            }

            break;
        }
        case GL_VERTEX_SHADER:
        {
            symbolTable.insertVariable(COMMON_BUILTINS, "gl_Position",
                                       TType(EbtFloat, EbpHigh, EvqPosition, 4));
            symbolTable.insertVariable(COMMON_BUILTINS, "gl_PointSize",
                                       TType(EbtFloat, EbpMedium, EvqPointSize, 1));
            symbolTable.insertVariable(ESSL3_BUILTINS, "gl_InstanceID",
                                       TType(EbtInt, EbpHigh, EvqInstanceID, 1));
            symbolTable.insertVariable(ESSL3_BUILTINS, "gl_VertexID",
                                       TType(EbtInt, EbpHigh, EvqVertexID, 1));

            // For internal use by ANGLE - not exposed to the parser.
            symbolTable.insertVariable(GLSL_BUILTINS, "gl_ViewportIndex",
                                       TType(EbtInt, EbpHigh, EvqViewportIndex));
            // gl_Layer exists in other shader stages in ESSL, but not in vertex shader so far.
            symbolTable.insertVariable(GLSL_BUILTINS, "gl_Layer", TType(EbtInt, EbpHigh, EvqLayer));
            break;
        }
        case GL_COMPUTE_SHADER:
        {
            symbolTable.insertVariable(ESSL3_1_BUILTINS, "gl_NumWorkGroups",
                                       TType(EbtUInt, EbpUndefined, EvqNumWorkGroups, 3));
            symbolTable.insertVariable(ESSL3_1_BUILTINS, "gl_WorkGroupSize",
                                       TType(EbtUInt, EbpUndefined, EvqWorkGroupSize, 3));
            symbolTable.insertVariable(ESSL3_1_BUILTINS, "gl_WorkGroupID",
                                       TType(EbtUInt, EbpUndefined, EvqWorkGroupID, 3));
            symbolTable.insertVariable(ESSL3_1_BUILTINS, "gl_LocalInvocationID",
                                       TType(EbtUInt, EbpUndefined, EvqLocalInvocationID, 3));
            symbolTable.insertVariable(ESSL3_1_BUILTINS, "gl_GlobalInvocationID",
                                       TType(EbtUInt, EbpUndefined, EvqGlobalInvocationID, 3));
            symbolTable.insertVariable(ESSL3_1_BUILTINS, "gl_LocalInvocationIndex",
                                       TType(EbtUInt, EbpUndefined, EvqLocalInvocationIndex, 1));
            break;
        }

        case GL_GEOMETRY_SHADER_OES:
        {
            TExtension extension = TExtension::OES_geometry_shader;

            // Add built-in interface block gl_PerVertex and the built-in array gl_in.
            // TODO(jiawei.shao@intel.com): implement GL_OES_geometry_point_size.
            const TString *glPerVertexString = NewPoolTString("gl_PerVertex");
            symbolTable.insertInterfaceBlockNameExt(ESSL3_1_BUILTINS, extension, glPerVertexString);

            TFieldList *fieldList    = NewPoolTFieldList();
            TSourceLoc zeroSourceLoc = {0, 0, 0, 0};
            TField *glPositionField  = new TField(new TType(EbtFloat, EbpHigh, EvqPosition, 4),
                                                 NewPoolTString("gl_Position"), zeroSourceLoc);
            fieldList->push_back(glPositionField);

            TInterfaceBlock *glInBlock = new TInterfaceBlock(
                glPerVertexString, fieldList, NewPoolTString("gl_in"), TLayoutQualifier::Create());

            // The array size of gl_in is undefined until we get a valid input primitive
            // declaration.
            TType glInType(glInBlock, EvqPerVertexIn, TLayoutQualifier::Create());
            glInType.makeArray(0u);
            symbolTable.insertVariableExt(ESSL3_1_BUILTINS, extension, "gl_in", glInType);

            TType glPositionType(EbtFloat, EbpHigh, EvqPosition, 4);
            glPositionType.setInterfaceBlock(new TInterfaceBlock(
                glPerVertexString, fieldList, nullptr, TLayoutQualifier::Create()));
            symbolTable.insertVariableExt(ESSL3_1_BUILTINS, extension, "gl_Position",
                                          glPositionType);
            symbolTable.insertVariableExt(ESSL3_1_BUILTINS, extension, "gl_PrimitiveIDIn",
                                          TType(EbtInt, EbpHigh, EvqPrimitiveIDIn, 1));
            symbolTable.insertVariableExt(ESSL3_1_BUILTINS, extension, "gl_InvocationID",
                                          TType(EbtInt, EbpHigh, EvqInvocationID, 1));
            symbolTable.insertVariableExt(ESSL3_1_BUILTINS, extension, "gl_PrimitiveID",
                                          TType(EbtInt, EbpHigh, EvqPrimitiveID, 1));
            symbolTable.insertVariableExt(ESSL3_1_BUILTINS, extension, "gl_Layer",
                                          TType(EbtInt, EbpHigh, EvqLayer, 1));

            break;
        }
        default:
            UNREACHABLE();
    }
}

void InitExtensionBehavior(const ShBuiltInResources &resources, TExtensionBehavior &extBehavior)
{
    if (resources.OES_standard_derivatives)
    {
        extBehavior[TExtension::OES_standard_derivatives] = EBhUndefined;
    }
    if (resources.OES_EGL_image_external)
    {
        extBehavior[TExtension::OES_EGL_image_external] = EBhUndefined;
    }
    if (resources.OES_EGL_image_external_essl3)
    {
        extBehavior[TExtension::OES_EGL_image_external_essl3] = EBhUndefined;
    }
    if (resources.NV_EGL_stream_consumer_external)
    {
        extBehavior[TExtension::NV_EGL_stream_consumer_external] = EBhUndefined;
    }
    if (resources.ARB_texture_rectangle)
    {
        // Special: ARB_texture_rectangle extension does not follow the standard for #extension
        // directives - it is enabled by default. An extension directive may still disable it.
        extBehavior[TExtension::ARB_texture_rectangle] = EBhEnable;
    }
    if (resources.EXT_blend_func_extended)
    {
        extBehavior[TExtension::EXT_blend_func_extended] = EBhUndefined;
    }
    if (resources.EXT_draw_buffers)
    {
        extBehavior[TExtension::EXT_draw_buffers] = EBhUndefined;
    }
    if (resources.EXT_frag_depth)
    {
        extBehavior[TExtension::EXT_frag_depth] = EBhUndefined;
    }
    if (resources.EXT_shader_texture_lod)
    {
        extBehavior[TExtension::EXT_shader_texture_lod] = EBhUndefined;
    }
    if (resources.EXT_shader_framebuffer_fetch)
    {
        extBehavior[TExtension::EXT_shader_framebuffer_fetch] = EBhUndefined;
    }
    if (resources.NV_shader_framebuffer_fetch)
    {
        extBehavior[TExtension::NV_shader_framebuffer_fetch] = EBhUndefined;
    }
    if (resources.ARM_shader_framebuffer_fetch)
    {
        extBehavior[TExtension::ARM_shader_framebuffer_fetch] = EBhUndefined;
    }
    if (resources.OVR_multiview)
    {
        extBehavior[TExtension::OVR_multiview] = EBhUndefined;
    }
    if (resources.EXT_YUV_target)
    {
        extBehavior[TExtension::EXT_YUV_target] = EBhUndefined;
    }
    if (resources.OES_geometry_shader)
    {
        extBehavior[TExtension::OES_geometry_shader] = EBhUndefined;
        extBehavior[TExtension::EXT_geometry_shader] = EBhUndefined;
    }
}

void ResetExtensionBehavior(TExtensionBehavior &extBehavior)
{
    for (auto &ext : extBehavior)
    {
        if (ext.first == TExtension::ARB_texture_rectangle)
        {
            ext.second = EBhEnable;
        }
        else
        {
            ext.second = EBhUndefined;
        }
    }
}

}  // namespace sh
