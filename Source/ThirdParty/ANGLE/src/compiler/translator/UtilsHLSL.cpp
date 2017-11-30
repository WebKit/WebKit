//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UtilsHLSL.cpp:
//   Utility methods for GLSL to HLSL translation.
//

#include "compiler/translator/UtilsHLSL.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/StructureHLSL.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

TString SamplerString(const TBasicType type)
{
    if (IsShadowSampler(type))
    {
        return "SamplerComparisonState";
    }
    else
    {
        return "SamplerState";
    }
}

TString SamplerString(HLSLTextureGroup type)
{
    if (type >= HLSL_COMPARISON_SAMPLER_GROUP_BEGIN && type <= HLSL_COMPARISON_SAMPLER_GROUP_END)
    {
        return "SamplerComparisonState";
    }
    else
    {
        return "SamplerState";
    }
}

HLSLTextureGroup TextureGroup(const TBasicType type, TLayoutImageInternalFormat imageInternalFormat)

{
    switch (type)
    {
        case EbtSampler2D:
            return HLSL_TEXTURE_2D;
        case EbtSamplerCube:
            return HLSL_TEXTURE_CUBE;
        case EbtSamplerExternalOES:
            return HLSL_TEXTURE_2D;
        case EbtSampler2DArray:
            return HLSL_TEXTURE_2D_ARRAY;
        case EbtSampler3D:
            return HLSL_TEXTURE_3D;
        case EbtSampler2DMS:
            return HLSL_TEXTURE_2D_MS;
        case EbtISampler2D:
            return HLSL_TEXTURE_2D_INT4;
        case EbtISampler3D:
            return HLSL_TEXTURE_3D_INT4;
        case EbtISamplerCube:
            return HLSL_TEXTURE_2D_ARRAY_INT4;
        case EbtISampler2DArray:
            return HLSL_TEXTURE_2D_ARRAY_INT4;
        case EbtISampler2DMS:
            return HLSL_TEXTURE_2D_MS_INT4;
        case EbtUSampler2D:
            return HLSL_TEXTURE_2D_UINT4;
        case EbtUSampler3D:
            return HLSL_TEXTURE_3D_UINT4;
        case EbtUSamplerCube:
            return HLSL_TEXTURE_2D_ARRAY_UINT4;
        case EbtUSampler2DArray:
            return HLSL_TEXTURE_2D_ARRAY_UINT4;
        case EbtUSampler2DMS:
            return HLSL_TEXTURE_2D_MS_UINT4;
        case EbtSampler2DShadow:
            return HLSL_TEXTURE_2D_COMPARISON;
        case EbtSamplerCubeShadow:
            return HLSL_TEXTURE_CUBE_COMPARISON;
        case EbtSampler2DArrayShadow:
            return HLSL_TEXTURE_2D_ARRAY_COMPARISON;
        case EbtImage2D:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32F:
                case EiifRGBA16F:
                case EiifR32F:
                    return HLSL_TEXTURE_2D;
                case EiifRGBA8:
                    return HLSL_TEXTURE_2D_UNORM;
                case EiifRGBA8_SNORM:
                    return HLSL_TEXTURE_2D_SNORM;
                default:
                    UNREACHABLE();
            }
        }
        case EbtIImage2D:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32I:
                case EiifRGBA16I:
                case EiifRGBA8I:
                case EiifR32I:
                    return HLSL_TEXTURE_2D_INT4;
                default:
                    UNREACHABLE();
            }
        }
        case EbtUImage2D:
        {
            switch (imageInternalFormat)
            {

                case EiifRGBA32UI:
                case EiifRGBA16UI:
                case EiifRGBA8UI:
                case EiifR32UI:
                    return HLSL_TEXTURE_2D_UINT4;
                default:
                    UNREACHABLE();
            }
        }
        case EbtImage3D:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32F:
                case EiifRGBA16F:
                case EiifR32F:
                    return HLSL_TEXTURE_3D;
                case EiifRGBA8:
                    return HLSL_TEXTURE_3D_UNORM;
                case EiifRGBA8_SNORM:
                    return HLSL_TEXTURE_3D_SNORM;
                default:
                    UNREACHABLE();
            }
        }
        case EbtIImage3D:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32I:
                case EiifRGBA16I:
                case EiifRGBA8I:
                case EiifR32I:
                    return HLSL_TEXTURE_3D_INT4;
                default:
                    UNREACHABLE();
            }
        }
        case EbtUImage3D:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32UI:
                case EiifRGBA16UI:
                case EiifRGBA8UI:
                case EiifR32UI:
                    return HLSL_TEXTURE_3D_UINT4;
                default:
                    UNREACHABLE();
            }
        }
        case EbtImage2DArray:
        case EbtImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32F:
                case EiifRGBA16F:
                case EiifR32F:
                    return HLSL_TEXTURE_2D_ARRAY;
                case EiifRGBA8:
                    return HLSL_TEXTURE_2D_ARRAY_UNORN;
                case EiifRGBA8_SNORM:
                    return HLSL_TEXTURE_2D_ARRAY_SNORM;
                default:
                    UNREACHABLE();
            }
        }
        case EbtIImage2DArray:
        case EbtIImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32I:
                case EiifRGBA16I:
                case EiifRGBA8I:
                case EiifR32I:
                    return HLSL_TEXTURE_2D_ARRAY_INT4;
                default:
                    UNREACHABLE();
            }
        }
        case EbtUImage2DArray:
        case EbtUImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32UI:
                case EiifRGBA16UI:
                case EiifRGBA8UI:
                case EiifR32UI:
                    return HLSL_TEXTURE_2D_ARRAY_UINT4;
                default:
                    UNREACHABLE();
            }
        }
        default:
            UNREACHABLE();
    }
    return HLSL_TEXTURE_UNKNOWN;
}

TString TextureString(const HLSLTextureGroup textureGroup)
{
    switch (textureGroup)
    {
        case HLSL_TEXTURE_2D:
            return "Texture2D<float4>";
        case HLSL_TEXTURE_CUBE:
            return "TextureCube<float4>";
        case HLSL_TEXTURE_2D_ARRAY:
            return "Texture2DArray<float4>";
        case HLSL_TEXTURE_3D:
            return "Texture3D<float4>";
        case HLSL_TEXTURE_2D_UNORM:
            return "Texture2D<unorm float4>";
        case HLSL_TEXTURE_CUBE_UNORM:
            return "TextureCube<unorm float4>";
        case HLSL_TEXTURE_2D_ARRAY_UNORN:
            return "Texture2DArray<unorm float4>";
        case HLSL_TEXTURE_3D_UNORM:
            return "Texture3D<unorm float4>";
        case HLSL_TEXTURE_2D_SNORM:
            return "Texture2D<snorm float4>";
        case HLSL_TEXTURE_CUBE_SNORM:
            return "TextureCube<snorm float4>";
        case HLSL_TEXTURE_2D_ARRAY_SNORM:
            return "Texture2DArray<snorm float4>";
        case HLSL_TEXTURE_3D_SNORM:
            return "Texture3D<snorm float4>";
        case HLSL_TEXTURE_2D_MS:
            return "Texture2DMS<float4>";
        case HLSL_TEXTURE_2D_INT4:
            return "Texture2D<int4>";
        case HLSL_TEXTURE_3D_INT4:
            return "Texture3D<int4>";
        case HLSL_TEXTURE_2D_ARRAY_INT4:
            return "Texture2DArray<int4>";
        case HLSL_TEXTURE_2D_MS_INT4:
            return "Texture2DMS<int4>";
        case HLSL_TEXTURE_2D_UINT4:
            return "Texture2D<uint4>";
        case HLSL_TEXTURE_3D_UINT4:
            return "Texture3D<uint4>";
        case HLSL_TEXTURE_2D_ARRAY_UINT4:
            return "Texture2DArray<uint4>";
        case HLSL_TEXTURE_2D_MS_UINT4:
            return "Texture2DMS<uint4>";
        case HLSL_TEXTURE_2D_COMPARISON:
            return "Texture2D";
        case HLSL_TEXTURE_CUBE_COMPARISON:
            return "TextureCube";
        case HLSL_TEXTURE_2D_ARRAY_COMPARISON:
            return "Texture2DArray";
        default:
            UNREACHABLE();
    }

    return "<unknown read texture type>";
}

TString TextureString(const TBasicType type, TLayoutImageInternalFormat imageInternalFormat)
{
    return TextureString(TextureGroup(type, imageInternalFormat));
}

TString TextureGroupSuffix(const HLSLTextureGroup type)
{
    switch (type)
    {
        case HLSL_TEXTURE_2D:
            return "2D";
        case HLSL_TEXTURE_CUBE:
            return "Cube";
        case HLSL_TEXTURE_2D_ARRAY:
            return "2DArray";
        case HLSL_TEXTURE_3D:
            return "3D";
        case HLSL_TEXTURE_2D_UNORM:
            return "2D_unorm_float4_";
        case HLSL_TEXTURE_CUBE_UNORM:
            return "Cube_unorm_float4_";
        case HLSL_TEXTURE_2D_ARRAY_UNORN:
            return "2DArray_unorm_float4_";
        case HLSL_TEXTURE_3D_UNORM:
            return "3D_unorm_float4_";
        case HLSL_TEXTURE_2D_SNORM:
            return "2D_snorm_float4_";
        case HLSL_TEXTURE_CUBE_SNORM:
            return "Cube_snorm_float4_";
        case HLSL_TEXTURE_2D_ARRAY_SNORM:
            return "2DArray_snorm_float4_";
        case HLSL_TEXTURE_3D_SNORM:
            return "3D_snorm_float4_";
        case HLSL_TEXTURE_2D_MS:
            return "2DMS";
        case HLSL_TEXTURE_2D_INT4:
            return "2D_int4_";
        case HLSL_TEXTURE_3D_INT4:
            return "3D_int4_";
        case HLSL_TEXTURE_2D_ARRAY_INT4:
            return "2DArray_int4_";
        case HLSL_TEXTURE_2D_MS_INT4:
            return "2DMS_int4_";
        case HLSL_TEXTURE_2D_UINT4:
            return "2D_uint4_";
        case HLSL_TEXTURE_3D_UINT4:
            return "3D_uint4_";
        case HLSL_TEXTURE_2D_ARRAY_UINT4:
            return "2DArray_uint4_";
        case HLSL_TEXTURE_2D_MS_UINT4:
            return "2DMS_uint4_";
        case HLSL_TEXTURE_2D_COMPARISON:
            return "2D_comparison";
        case HLSL_TEXTURE_CUBE_COMPARISON:
            return "Cube_comparison";
        case HLSL_TEXTURE_2D_ARRAY_COMPARISON:
            return "2DArray_comparison";
        default:
            UNREACHABLE();
    }

    return "<unknown texture type>";
}

TString TextureGroupSuffix(const TBasicType type, TLayoutImageInternalFormat imageInternalFormat)
{
    return TextureGroupSuffix(TextureGroup(type, imageInternalFormat));
}

TString TextureTypeSuffix(const TBasicType type, TLayoutImageInternalFormat imageInternalFormat)
{
    switch (type)
    {
        case EbtISamplerCube:
            return "Cube_int4_";
        case EbtUSamplerCube:
            return "Cube_uint4_";
        case EbtSamplerExternalOES:
            return "_External";
        case EbtImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32F:
                case EiifRGBA16F:
                case EiifR32F:
                    return "Cube_float4_";
                case EiifRGBA8:
                    return "Cube_unorm_float4_";
                case EiifRGBA8_SNORM:
                    return "Cube_snorm_float4_";
                default:
                    UNREACHABLE();
            }
        }
        case EbtIImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32I:
                case EiifRGBA16I:
                case EiifRGBA8I:
                case EiifR32I:
                    return "Cube_int4_";
                default:
                    UNREACHABLE();
            }
        }
        case EbtUImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32UI:
                case EiifRGBA16UI:
                case EiifRGBA8UI:
                case EiifR32UI:
                    return "Cube_uint4_";
                default:
                    UNREACHABLE();
            }
        }
        default:
            // All other types are identified by their group suffix
            return TextureGroupSuffix(type, imageInternalFormat);
    }
}

HLSLRWTextureGroup RWTextureGroup(const TBasicType type,
                                  TLayoutImageInternalFormat imageInternalFormat)

{
    switch (type)
    {
        case EbtImage2D:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32F:
                case EiifRGBA16F:
                case EiifR32F:
                    return HLSL_RWTEXTURE_2D_FLOAT4;
                case EiifRGBA8:
                    return HLSL_RWTEXTURE_2D_UNORM;
                case EiifRGBA8_SNORM:
                    return HLSL_RWTEXTURE_2D_SNORM;
                default:
                    UNREACHABLE();
            }
        }
        case EbtIImage2D:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32I:
                case EiifRGBA16I:
                case EiifRGBA8I:
                case EiifR32I:
                    return HLSL_RWTEXTURE_2D_INT4;
                default:
                    UNREACHABLE();
            }
        }
        case EbtUImage2D:
        {
            switch (imageInternalFormat)
            {

                case EiifRGBA32UI:
                case EiifRGBA16UI:
                case EiifRGBA8UI:
                case EiifR32UI:
                    return HLSL_RWTEXTURE_2D_UINT4;
                default:
                    UNREACHABLE();
            }
        }
        case EbtImage3D:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32F:
                case EiifRGBA16F:
                case EiifR32F:
                    return HLSL_RWTEXTURE_3D_FLOAT4;
                case EiifRGBA8:
                    return HLSL_RWTEXTURE_3D_UNORM;
                case EiifRGBA8_SNORM:
                    return HLSL_RWTEXTURE_3D_SNORM;
                default:
                    UNREACHABLE();
            }
        }
        case EbtIImage3D:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32I:
                case EiifRGBA16I:
                case EiifRGBA8I:
                case EiifR32I:
                    return HLSL_RWTEXTURE_3D_INT4;
                default:
                    UNREACHABLE();
            }
        }
        case EbtUImage3D:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32UI:
                case EiifRGBA16UI:
                case EiifRGBA8UI:
                case EiifR32UI:
                    return HLSL_RWTEXTURE_3D_UINT4;
                default:
                    UNREACHABLE();
            }
        }
        case EbtImage2DArray:
        case EbtImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32F:
                case EiifRGBA16F:
                case EiifR32F:
                    return HLSL_RWTEXTURE_2D_ARRAY_FLOAT4;
                case EiifRGBA8:
                    return HLSL_RWTEXTURE_2D_ARRAY_UNORN;
                case EiifRGBA8_SNORM:
                    return HLSL_RWTEXTURE_2D_ARRAY_SNORM;
                default:
                    UNREACHABLE();
            }
        }
        case EbtIImage2DArray:
        case EbtIImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32I:
                case EiifRGBA16I:
                case EiifRGBA8I:
                case EiifR32I:
                    return HLSL_RWTEXTURE_2D_ARRAY_INT4;
                default:
                    UNREACHABLE();
            }
        }
        case EbtUImage2DArray:
        case EbtUImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32UI:
                case EiifRGBA16UI:
                case EiifRGBA8UI:
                case EiifR32UI:
                    return HLSL_RWTEXTURE_2D_ARRAY_UINT4;
                default:
                    UNREACHABLE();
            }
        }
        default:
            UNREACHABLE();
    }
    return HLSL_RWTEXTURE_UNKNOWN;
}

TString RWTextureString(const HLSLRWTextureGroup RWTextureGroup)
{
    switch (RWTextureGroup)
    {
        case HLSL_RWTEXTURE_2D_FLOAT4:
            return "RWTexture2D<float4>";
        case HLSL_RWTEXTURE_2D_ARRAY_FLOAT4:
            return "RWTexture2DArray<float4>";
        case HLSL_RWTEXTURE_3D_FLOAT4:
            return "RWTexture3D<float4>";
        case HLSL_RWTEXTURE_2D_UNORM:
            return "RWTexture2D<unorm float4>";
        case HLSL_RWTEXTURE_2D_ARRAY_UNORN:
            return "RWTexture2DArray<unorm float4>";
        case HLSL_RWTEXTURE_3D_UNORM:
            return "RWTexture3D<unorm float4>";
        case HLSL_RWTEXTURE_2D_SNORM:
            return "RWTexture2D<snorm float4>";
        case HLSL_RWTEXTURE_2D_ARRAY_SNORM:
            return "RWTexture2DArray<snorm float4>";
        case HLSL_RWTEXTURE_3D_SNORM:
            return "RWTexture3D<snorm float4>";
        case HLSL_RWTEXTURE_2D_UINT4:
            return "RWTexture2D<uint4>";
        case HLSL_RWTEXTURE_2D_ARRAY_UINT4:
            return "RWTexture2DArray<uint4>";
        case HLSL_RWTEXTURE_3D_UINT4:
            return "RWTexture3D<uint4>";
        case HLSL_RWTEXTURE_2D_INT4:
            return "RWTexture2D<int4>";
        case HLSL_RWTEXTURE_2D_ARRAY_INT4:
            return "RWTexture2DArray<int4>";
        case HLSL_RWTEXTURE_3D_INT4:
            return "RWTexture3D<int4>";
        default:
            UNREACHABLE();
    }

    return "<unknown read and write texture type>";
}

TString RWTextureString(const TBasicType type, TLayoutImageInternalFormat imageInternalFormat)
{
    return RWTextureString(RWTextureGroup(type, imageInternalFormat));
}

TString RWTextureGroupSuffix(const HLSLRWTextureGroup type)
{
    switch (type)
    {
        case HLSL_RWTEXTURE_2D_FLOAT4:
            return "RW2D_float4_";
        case HLSL_RWTEXTURE_2D_ARRAY_FLOAT4:
            return "RW2DArray_float4_";
        case HLSL_RWTEXTURE_3D_FLOAT4:
            return "RW3D_float4_";
        case HLSL_RWTEXTURE_2D_UNORM:
            return "RW2D_unorm_float4_";
        case HLSL_RWTEXTURE_2D_ARRAY_UNORN:
            return "RW2DArray_unorm_float4_";
        case HLSL_RWTEXTURE_3D_UNORM:
            return "RW3D_unorm_float4_";
        case HLSL_RWTEXTURE_2D_SNORM:
            return "RW2D_snorm_float4_";
        case HLSL_RWTEXTURE_2D_ARRAY_SNORM:
            return "RW2DArray_snorm_float4_";
        case HLSL_RWTEXTURE_3D_SNORM:
            return "RW3D_snorm_float4_";
        case HLSL_RWTEXTURE_2D_UINT4:
            return "RW2D_uint4_";
        case HLSL_RWTEXTURE_2D_ARRAY_UINT4:
            return "RW2DArray_uint4_";
        case HLSL_RWTEXTURE_3D_UINT4:
            return "RW3D_uint4_";
        case HLSL_RWTEXTURE_2D_INT4:
            return "RW2D_int4_";
        case HLSL_RWTEXTURE_2D_ARRAY_INT4:
            return "RW2DArray_int4_";
        case HLSL_RWTEXTURE_3D_INT4:
            return "RW3D_int4_";
        default:
            UNREACHABLE();
    }

    return "<unknown read and write resource>";
}

TString RWTextureGroupSuffix(const TBasicType type, TLayoutImageInternalFormat imageInternalFormat)
{
    return RWTextureGroupSuffix(RWTextureGroup(type, imageInternalFormat));
}

TString RWTextureTypeSuffix(const TBasicType type, TLayoutImageInternalFormat imageInternalFormat)
{
    switch (type)
    {
        case EbtImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32F:
                case EiifRGBA16F:
                case EiifR32F:
                    return "RWCube_float4_";
                case EiifRGBA8:
                    return "RWCube_unorm_float4_";
                case EiifRGBA8_SNORM:
                    return "RWCube_unorm_float4_";
                default:
                    UNREACHABLE();
            }
        }
        case EbtIImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32I:
                case EiifRGBA16I:
                case EiifRGBA8I:
                case EiifR32I:
                    return "RWCube_int4_";
                default:
                    UNREACHABLE();
            }
        }
        case EbtUImageCube:
        {
            switch (imageInternalFormat)
            {
                case EiifRGBA32UI:
                case EiifRGBA16UI:
                case EiifRGBA8UI:
                case EiifR32UI:
                    return "RWCube_uint4_";
                default:
                    UNREACHABLE();
            }
        }
        default:
            // All other types are identified by their group suffix
            return TextureGroupSuffix(type, imageInternalFormat);
    }
}

TString DecorateField(const TString &string, const TStructure &structure)
{
    if (structure.name().compare(0, 3, "gl_") != 0)
    {
        return Decorate(string);
    }

    return string;
}

TString DecoratePrivate(const TString &privateText)
{
    return "dx_" + privateText;
}

TString Decorate(const TString &string)
{
    if (string.compare(0, 3, "gl_") != 0)
    {
        return "_" + string;
    }

    return string;
}

TString DecorateVariableIfNeeded(const TName &name)
{
    if (name.isInternal())
    {
        // The name should not have a prefix reserved for user-defined variables or functions.
        ASSERT(name.getString().compare(0, 2, "f_") != 0);
        ASSERT(name.getString().compare(0, 1, "_") != 0);
        return name.getString();
    }
    else
    {
        return Decorate(name.getString());
    }
}

TString DecorateFunctionIfNeeded(const TName &name)
{
    if (name.isInternal())
    {
        // The name should not have a prefix reserved for user-defined variables or functions.
        ASSERT(name.getString().compare(0, 2, "f_") != 0);
        ASSERT(name.getString().compare(0, 1, "_") != 0);
        return name.getString();
    }
    ASSERT(name.getString().compare(0, 3, "gl_") != 0);
    // Add an additional f prefix to functions so that they're always disambiguated from variables.
    // This is necessary in the corner case where a variable declaration hides a function that it
    // uses in its initializer.
    return "f_" + name.getString();
}

TString TypeString(const TType &type)
{
    const TStructure *structure = type.getStruct();
    if (structure)
    {
        const TString &typeName = structure->name();
        if (typeName != "")
        {
            return StructNameString(*structure);
        }
        else  // Nameless structure, define in place
        {
            return StructureHLSL::defineNameless(*structure);
        }
    }
    else if (type.isMatrix())
    {
        int cols = type.getCols();
        int rows = type.getRows();
        return "float" + str(cols) + "x" + str(rows);
    }
    else
    {
        switch (type.getBasicType())
        {
            case EbtFloat:
                switch (type.getNominalSize())
                {
                    case 1:
                        return "float";
                    case 2:
                        return "float2";
                    case 3:
                        return "float3";
                    case 4:
                        return "float4";
                }
            case EbtInt:
                switch (type.getNominalSize())
                {
                    case 1:
                        return "int";
                    case 2:
                        return "int2";
                    case 3:
                        return "int3";
                    case 4:
                        return "int4";
                }
            case EbtUInt:
                switch (type.getNominalSize())
                {
                    case 1:
                        return "uint";
                    case 2:
                        return "uint2";
                    case 3:
                        return "uint3";
                    case 4:
                        return "uint4";
                }
            case EbtBool:
                switch (type.getNominalSize())
                {
                    case 1:
                        return "bool";
                    case 2:
                        return "bool2";
                    case 3:
                        return "bool3";
                    case 4:
                        return "bool4";
                }
            case EbtVoid:
                return "void";
            case EbtSampler2D:
            case EbtISampler2D:
            case EbtUSampler2D:
            case EbtSampler2DArray:
            case EbtISampler2DArray:
            case EbtUSampler2DArray:
                return "sampler2D";
            case EbtSamplerCube:
            case EbtISamplerCube:
            case EbtUSamplerCube:
                return "samplerCUBE";
            case EbtSamplerExternalOES:
                return "sampler2D";
            case EbtAtomicCounter:
                return "atomic_uint";
            default:
                break;
        }
    }

    UNREACHABLE();
    return "<unknown type>";
}

TString StructNameString(const TStructure &structure)
{
    if (structure.name().empty())
    {
        return "";
    }

    // For structures at global scope we use a consistent
    // translation so that we can link between shader stages.
    if (structure.atGlobalScope())
    {
        return Decorate(structure.name());
    }

    return "ss" + str(structure.uniqueId()) + "_" + structure.name();
}

TString QualifiedStructNameString(const TStructure &structure,
                                  bool useHLSLRowMajorPacking,
                                  bool useStd140Packing)
{
    if (structure.name() == "")
    {
        return "";
    }

    TString prefix = "";

    // Structs packed with row-major matrices in HLSL are prefixed with "rm"
    // GLSL column-major maps to HLSL row-major, and the converse is true

    if (useStd140Packing)
    {
        prefix += "std_";
    }

    if (useHLSLRowMajorPacking)
    {
        prefix += "rm_";
    }

    return prefix + StructNameString(structure);
}

TString InterpolationString(TQualifier qualifier)
{
    switch (qualifier)
    {
        case EvqVaryingIn:
            return "";
        case EvqFragmentIn:
            return "";
        case EvqSmoothIn:
            return "linear";
        case EvqFlatIn:
            return "nointerpolation";
        case EvqCentroidIn:
            return "centroid";
        case EvqVaryingOut:
            return "";
        case EvqVertexOut:
            return "";
        case EvqSmoothOut:
            return "linear";
        case EvqFlatOut:
            return "nointerpolation";
        case EvqCentroidOut:
            return "centroid";
        default:
            UNREACHABLE();
    }

    return "";
}

TString QualifierString(TQualifier qualifier)
{
    switch (qualifier)
    {
        case EvqIn:
            return "in";
        case EvqOut:
            return "inout";  // 'out' results in an HLSL error if not all fields are written, for
                             // GLSL it's undefined
        case EvqInOut:
            return "inout";
        case EvqConstReadOnly:
            return "const";
        default:
            UNREACHABLE();
    }

    return "";
}

TString DisambiguateFunctionName(const TIntermSequence *parameters)
{
    TString disambiguatingString;
    for (auto parameter : *parameters)
    {
        const TType &paramType = parameter->getAsTyped()->getType();
        // Parameter types are only added to function names if they are ambiguous according to the
        // native HLSL compiler. Other parameter types are not added to function names to avoid
        // making function names longer.
        if (paramType.getObjectSize() == 4 && paramType.getBasicType() == EbtFloat)
        {
            // Disambiguation is needed for float2x2 and float4 parameters. These are the only
            // built-in types that HLSL thinks are identical. float2x3 and float3x2 are different
            // types, for example.
            disambiguatingString += "_" + TypeString(paramType);
        }
        else if (paramType.getBasicType() == EbtStruct)
        {
            // Disambiguation is needed for struct parameters, since HLSL thinks that structs with
            // the same fields but a different name are identical.
            ASSERT(paramType.getStruct()->name() != "");
            disambiguatingString += "_" + TypeString(paramType);
        }
    }
    return disambiguatingString;
}

}  // namespace sh
