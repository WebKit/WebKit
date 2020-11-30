//
// Copyright 2019 The ANGLE Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// blit.metal: Implements blitting texture content to current frame buffer.

#include "common.h"

using namespace rx::mtl_shader;

constant bool kPremultiplyAlpha [[function_constant(1)]];
constant bool kUnmultiplyAlpha [[function_constant(2)]];
constant int kSourceTextureType [[function_constant(3)]];   // Source texture type.
constant bool kSourceTextureType2D      = kSourceTextureType == kTextureType2D;
constant bool kSourceTextureType2DArray = kSourceTextureType == kTextureType2DArray;
constant bool kSourceTextureType2DMS    = kSourceTextureType == kTextureType2DMultisample;
constant bool kSourceTextureTypeCube    = kSourceTextureType == kTextureTypeCube;
constant bool kSourceTextureType3D      = kSourceTextureType == kTextureType3D;

struct BlitParams
{
    // 0: lower left, 1: lower right, 2: upper left
    float2 srcTexCoords[3];
    int srcLevel;   // Source texture level.
    int srcLayer;   // Source texture layer.

    bool dstFlipViewportX;
    bool dstFlipViewportY;
    bool dstLuminance;  // destination texture is luminance. Unused by depth & stencil blitting.
};

struct BlitVSOut
{
    float4 position [[position]];
    float2 texCoords [[user(locn1)]];
};

vertex BlitVSOut blitVS(unsigned int vid [[vertex_id]], constant BlitParams &options [[buffer(0)]])
{
    BlitVSOut output;
    output.position  = float4(gCorners[vid], 0.0, 1.0);
    output.texCoords = options.srcTexCoords[vid];

    if (options.dstFlipViewportX)
    {
        output.position.x = -output.position.x;
    }
    if (!options.dstFlipViewportY)
    {
        // If viewport is not flipped, we have to flip Y in normalized device coordinates.
        // Since NDC has Y is opposite direction of viewport coodrinates.
        output.position.y = -output.position.y;
    }

    return output;
}

static inline float3 cubeTexcoords(float2 texcoords, int face)
{
    texcoords = 2.0 * texcoords - 1.0;
    switch (face)
    {
        case 0:
            return float3(1.0, -texcoords.y, -texcoords.x);
        case 1:
            return float3(-1.0, -texcoords.y, texcoords.x);
        case 2:
            return float3(texcoords.x, 1.0, texcoords.y);
        case 3:
            return float3(texcoords.x, -1.0, -texcoords.y);
        case 4:
            return float3(texcoords.x, -texcoords.y, 1.0);
        case 5:
            return float3(-texcoords.x, -texcoords.y, -1.0);
    }
    return float3(texcoords, 0);
}

template <typename T>
static inline vec<T, 4> blitSampleTextureMS(texture2d_ms<T> srcTexture, float2 texCoords)
{
    uint2 dimens(srcTexture.get_width(), srcTexture.get_height());
    uint2 coords = uint2(texCoords * float2(dimens));

    uint samples = srcTexture.get_num_samples();

    vec<T, 4> output(0);

    for (uint sample = 0; sample < samples; ++sample)
    {
        output += srcTexture.read(coords, sample);
    }

    output = output / samples;

    return output;
}

template <typename T>
static inline vec<T, 4> blitSampleTexture3D(texture3d<T> srcTexture,
                                            sampler textureSampler,
                                            float2 texCoords,
                                            constant BlitParams &options)
{
    uint depth   = srcTexture.get_depth(options.srcLevel);
    float zCoord = (float(options.srcLayer) + 0.5) / float(depth);

    return srcTexture.sample(textureSampler, float3(texCoords, zCoord), level(options.srcLevel));
}

// clang-format off
#define BLIT_COLOR_FS_PARAMS(TYPE)                                                               \
    BlitVSOut input [[stage_in]],                                                                \
    texture2d<TYPE> srcTexture2d [[texture(0), function_constant(kSourceTextureType2D)]],        \
    texture2d_array<TYPE> srcTexture2dArray                                                      \
    [[texture(0), function_constant(kSourceTextureType2DArray)]],                                \
    texture2d_ms<TYPE> srcTexture2dMS [[texture(0), function_constant(kSourceTextureType2DMS)]], \
    texturecube<TYPE> srcTextureCube [[texture(0), function_constant(kSourceTextureTypeCube)]],  \
    texture3d<TYPE> srcTexture3d [[texture(0), function_constant(kSourceTextureType3D)]],        \
    sampler textureSampler [[sampler(0)]],                                                       \
    constant BlitParams &options [[buffer(0)]]
// clang-format on

#define FORWARD_BLIT_COLOR_FS_PARAMS                                                      \
    input, srcTexture2d, srcTexture2dArray, srcTexture2dMS, srcTextureCube, srcTexture3d, \
        textureSampler, options

template <typename T>
static inline vec<T, 4> blitReadTexture(BLIT_COLOR_FS_PARAMS(T))
{
    vec<T, 4> output;

    switch (kSourceTextureType)
    {
        case kTextureType2D:
            output = srcTexture2d.sample(textureSampler, input.texCoords, level(options.srcLevel));
            break;
        case kTextureType2DArray:
            output = srcTexture2dArray.sample(textureSampler, input.texCoords, options.srcLayer,
                                              level(options.srcLevel));
            break;
        case kTextureType2DMultisample:
            output = blitSampleTextureMS(srcTexture2dMS, input.texCoords);
            break;
        case kTextureTypeCube:
            output = srcTextureCube.sample(textureSampler,
                                           cubeTexcoords(input.texCoords, options.srcLayer),
                                           level(options.srcLevel));
            break;
        case kTextureType3D:
            output = blitSampleTexture3D(srcTexture3d, textureSampler, input.texCoords, options);
            break;
    }

    if (kPremultiplyAlpha)
    {
        output.xyz *= output.a;
    }
    else if (kUnmultiplyAlpha)
    {
        if (output.a != 0.0)
        {
            output.xyz /= output.a;
        }
    }

    if (options.dstLuminance)
    {
        output.g = output.b = output.r;
    }

    return output;
}

fragment float4 blitFS(BLIT_COLOR_FS_PARAMS(float))
{
    return blitReadTexture(FORWARD_BLIT_COLOR_FS_PARAMS);
}
