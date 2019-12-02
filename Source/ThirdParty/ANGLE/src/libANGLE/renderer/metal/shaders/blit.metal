//
// Copyright 2019 The ANGLE Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// blit.metal: Implements blitting texture content to current frame buffer.

#include "common.h"


struct BlitParams
{
    // 0: lower left, 1: lower right, 2: upper left, 3: upper right
    float2 srcTexCoords[4];
    int srcLevel;
    bool srcLuminance; // source texture is luminance texture
    bool dstFlipViewportY;
    bool dstLuminance; // destination texture is luminance;
};

struct BlitVSOut
{
    float4 position [[position]];
    float2 texCoords [[user(locn1)]];
};

vertex BlitVSOut blitVS(unsigned int vid [[ vertex_id ]],
                         constant BlitParams &options [[buffer(0)]])
{
    BlitVSOut output;
    output.position = float4(gCorners[vid], 0.0, 1.0);
    output.texCoords = options.srcTexCoords[gTexcoordsIndices[vid]];

    if (!options.dstFlipViewportY)
    {
        // If viewport is not flipped, we have to flip Y in normalized device coordinates.
        // Since NDC has Y is opposite direction of viewport coodrinates.
        output.position.y = -output.position.y;
    }

    return output;
}

float4 blitSampleTexture(texture2d<float> srcTexture,
                     float2 texCoords,
                     constant BlitParams &options)
{
    constexpr sampler textureSampler(mag_filter::linear,
                                     min_filter::linear);
    float4 output = srcTexture.sample(textureSampler, texCoords, level(options.srcLevel));

    if (options.srcLuminance)
    {
        output.gb = float2(output.r, output.r);
    }

    return output;
}

float4 blitOutput(float4 color, constant BlitParams &options)
{
    float4 ret = color;

    if (options.dstLuminance)
    {
        ret.r = ret.g = ret.b = color.r;
    }

    return ret;
}

fragment float4 blitFS(BlitVSOut input [[stage_in]],
                       texture2d<float> srcTexture [[texture(0)]],
                       constant BlitParams &options [[buffer(0)]])
{
    return blitOutput(blitSampleTexture(srcTexture, input.texCoords, options), options);
}

fragment float4 blitPremultiplyAlphaFS(BlitVSOut input [[stage_in]],
                                       texture2d<float> srcTexture [[texture(0)]],
                                       constant BlitParams &options [[buffer(0)]])
{
    float4 output = blitSampleTexture(srcTexture, input.texCoords, options);
    output.xyz *= output.a;
    return blitOutput(output, options);
}

fragment float4 blitUnmultiplyAlphaFS(BlitVSOut input [[stage_in]],
                                      texture2d<float> srcTexture [[texture(0)]],
                                      constant BlitParams &options [[buffer(0)]])
{
    float4 output = blitSampleTexture(srcTexture, input.texCoords, options);
    if (output.a != 0.0)
    {
        output.xyz *= 1.0 / output.a;
    }
    return blitOutput(output, options);
}
