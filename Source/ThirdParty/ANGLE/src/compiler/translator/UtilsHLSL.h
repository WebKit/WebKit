//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UtilsHLSL.h:
//   Utility methods for GLSL to HLSL translation.
//

#ifndef COMPILER_TRANSLATOR_UTILSHLSL_H_
#define COMPILER_TRANSLATOR_UTILSHLSL_H_

#include <vector>
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/Types.h"

#include "angle_gl.h"

class TName;

namespace sh
{

// HLSL Texture type for GLSL sampler type and readonly image type.
enum HLSLTextureGroup
{
    // read resources
    HLSL_TEXTURE_2D,
    HLSL_TEXTURE_MIN = HLSL_TEXTURE_2D,

    HLSL_TEXTURE_CUBE,
    HLSL_TEXTURE_2D_ARRAY,
    HLSL_TEXTURE_3D,
    HLSL_TEXTURE_2D_UNORM,
    HLSL_TEXTURE_CUBE_UNORM,
    HLSL_TEXTURE_2D_ARRAY_UNORN,
    HLSL_TEXTURE_3D_UNORM,
    HLSL_TEXTURE_2D_SNORM,
    HLSL_TEXTURE_CUBE_SNORM,
    HLSL_TEXTURE_2D_ARRAY_SNORM,
    HLSL_TEXTURE_3D_SNORM,
    HLSL_TEXTURE_2D_MS,
    HLSL_TEXTURE_2D_INT4,
    HLSL_TEXTURE_3D_INT4,
    HLSL_TEXTURE_2D_ARRAY_INT4,
    HLSL_TEXTURE_2D_MS_INT4,
    HLSL_TEXTURE_2D_UINT4,
    HLSL_TEXTURE_3D_UINT4,
    HLSL_TEXTURE_2D_ARRAY_UINT4,
    HLSL_TEXTURE_2D_MS_UINT4,

    // Comparison samplers

    HLSL_TEXTURE_2D_COMPARISON,
    HLSL_TEXTURE_CUBE_COMPARISON,
    HLSL_TEXTURE_2D_ARRAY_COMPARISON,

    HLSL_COMPARISON_SAMPLER_GROUP_BEGIN = HLSL_TEXTURE_2D_COMPARISON,
    HLSL_COMPARISON_SAMPLER_GROUP_END   = HLSL_TEXTURE_2D_ARRAY_COMPARISON,

    HLSL_TEXTURE_UNKNOWN,
    HLSL_TEXTURE_MAX = HLSL_TEXTURE_UNKNOWN
};

// HLSL RWTexture type for GLSL read and write image type.
enum HLSLRWTextureGroup
{
    // read/write resource
    HLSL_RWTEXTURE_2D_FLOAT4,
    HLSL_RWTEXTURE_MIN = HLSL_RWTEXTURE_2D_FLOAT4,
    HLSL_RWTEXTURE_2D_ARRAY_FLOAT4,
    HLSL_RWTEXTURE_3D_FLOAT4,
    HLSL_RWTEXTURE_2D_UNORM,
    HLSL_RWTEXTURE_2D_ARRAY_UNORN,
    HLSL_RWTEXTURE_3D_UNORM,
    HLSL_RWTEXTURE_2D_SNORM,
    HLSL_RWTEXTURE_2D_ARRAY_SNORM,
    HLSL_RWTEXTURE_3D_SNORM,
    HLSL_RWTEXTURE_2D_UINT4,
    HLSL_RWTEXTURE_2D_ARRAY_UINT4,
    HLSL_RWTEXTURE_3D_UINT4,
    HLSL_RWTEXTURE_2D_INT4,
    HLSL_RWTEXTURE_2D_ARRAY_INT4,
    HLSL_RWTEXTURE_3D_INT4,

    HLSL_RWTEXTURE_UNKNOWN,
    HLSL_RWTEXTURE_MAX = HLSL_RWTEXTURE_UNKNOWN
};

HLSLTextureGroup TextureGroup(const TBasicType type,
                              TLayoutImageInternalFormat imageInternalFormat = EiifUnspecified);
TString TextureString(const HLSLTextureGroup textureGroup);
TString TextureString(const TBasicType type,
                      TLayoutImageInternalFormat imageInternalFormat = EiifUnspecified);
TString TextureGroupSuffix(const HLSLTextureGroup type);
TString TextureGroupSuffix(const TBasicType type,
                           TLayoutImageInternalFormat imageInternalFormat = EiifUnspecified);
TString TextureTypeSuffix(const TBasicType type,
                          TLayoutImageInternalFormat imageInternalFormat = EiifUnspecified);
HLSLRWTextureGroup RWTextureGroup(const TBasicType type,
                                  TLayoutImageInternalFormat imageInternalFormat);
TString RWTextureString(const HLSLRWTextureGroup textureGroup);
TString RWTextureString(const TBasicType type, TLayoutImageInternalFormat imageInternalFormat);
TString RWTextureGroupSuffix(const HLSLRWTextureGroup type);
TString RWTextureGroupSuffix(const TBasicType type, TLayoutImageInternalFormat imageInternalFormat);
TString RWTextureTypeSuffix(const TBasicType type, TLayoutImageInternalFormat imageInternalFormat);

TString SamplerString(const TBasicType type);
TString SamplerString(HLSLTextureGroup type);

// Adds a prefix to user-defined names to avoid naming clashes.
TString Decorate(const TString &string);
TString DecorateVariableIfNeeded(const TName &name);
TString DecorateFunctionIfNeeded(const TName &name);
TString DecorateField(const TString &string, const TStructure &structure);
TString DecoratePrivate(const TString &privateText);
TString TypeString(const TType &type);
TString StructNameString(const TStructure &structure);
TString QualifiedStructNameString(const TStructure &structure,
                                  bool useHLSLRowMajorPacking,
                                  bool useStd140Packing);
TString InterpolationString(TQualifier qualifier);
TString QualifierString(TQualifier qualifier);
// Parameters may need to be included in function names to disambiguate between overloaded
// functions.
TString DisambiguateFunctionName(const TIntermSequence *parameters);
}

#endif  // COMPILER_TRANSLATOR_UTILSHLSL_H_
