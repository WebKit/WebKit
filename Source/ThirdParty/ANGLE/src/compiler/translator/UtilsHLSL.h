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

// Unique combinations of HLSL Texture type and HLSL Sampler type.
enum HLSLTextureSamplerGroup
{
    // Regular samplers
    HLSL_TEXTURE_2D,
    HLSL_TEXTURE_MIN = HLSL_TEXTURE_2D,

    HLSL_TEXTURE_CUBE,
    HLSL_TEXTURE_2D_ARRAY,
    HLSL_TEXTURE_3D,
    HLSL_TEXTURE_2D_INT4,
    HLSL_TEXTURE_3D_INT4,
    HLSL_TEXTURE_2D_ARRAY_INT4,
    HLSL_TEXTURE_2D_UINT4,
    HLSL_TEXTURE_3D_UINT4,
    HLSL_TEXTURE_2D_ARRAY_UINT4,

    // Comparison samplers

    HLSL_TEXTURE_2D_COMPARISON,
    HLSL_TEXTURE_CUBE_COMPARISON,
    HLSL_TEXTURE_2D_ARRAY_COMPARISON,

    HLSL_COMPARISON_SAMPLER_GROUP_BEGIN = HLSL_TEXTURE_2D_COMPARISON,
    HLSL_COMPARISON_SAMPLER_GROUP_END   = HLSL_TEXTURE_2D_ARRAY_COMPARISON,

    HLSL_TEXTURE_UNKNOWN,
    HLSL_TEXTURE_MAX = HLSL_TEXTURE_UNKNOWN
};

HLSLTextureSamplerGroup TextureGroup(const TBasicType type);
TString TextureString(const HLSLTextureSamplerGroup type);
TString TextureString(const TBasicType type);
TString TextureGroupSuffix(const HLSLTextureSamplerGroup type);
TString TextureGroupSuffix(const TBasicType type);
TString TextureTypeSuffix(const TBasicType type);
TString SamplerString(const TBasicType type);
TString SamplerString(HLSLTextureSamplerGroup type);
// Prepends an underscore to avoid naming clashes
TString Decorate(const TString &string);
TString DecorateIfNeeded(const TName &name);
// Decorates and also unmangles the function name
TString DecorateFunctionIfNeeded(const TName &name);
TString DecorateUniform(const TName &name, const TType &type);
TString DecorateField(const TString &string, const TStructure &structure);
TString DecoratePrivate(const TString &privateText);
TString TypeString(const TType &type);
TString StructNameString(const TStructure &structure);
TString QualifiedStructNameString(const TStructure &structure, bool useHLSLRowMajorPacking,
                                  bool useStd140Packing);
TString InterpolationString(TQualifier qualifier);
TString QualifierString(TQualifier qualifier);
// Parameters may need to be included in function names to disambiguate between overloaded
// functions.
TString DisambiguateFunctionName(const TIntermSequence *parameters);
}

#endif // COMPILER_TRANSLATOR_UTILSHLSL_H_
