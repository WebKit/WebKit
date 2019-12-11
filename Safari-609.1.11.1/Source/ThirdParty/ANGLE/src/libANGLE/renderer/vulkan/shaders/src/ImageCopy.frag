//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImageCopy.frag: Copy parts of an image to another.

#version 450 core

#extension GL_EXT_samplerless_texture_functions : require

#define MAKE_SRC_RESOURCE(prefix, type) prefix ## type

#if SrcIsFloat
#define SRC_RESOURCE(type) type
#define SrcType vec4
#elif SrcIsSint
#define SRC_RESOURCE(type) MAKE_SRC_RESOURCE(i, type)
#define SrcType ivec4
#elif SrcIsUint
#define SRC_RESOURCE(type) MAKE_SRC_RESOURCE(u, type)
#define SrcType uvec4
#else
#error "Not all source formats are accounted for"
#endif

#if SrcIsArray
#define SRC_RESOURCE_NAME texture2DArray
#else
#define SRC_RESOURCE_NAME texture2D
#endif

#if DestIsFloat
#define DestType vec4
#elif DestIsSint
#define DestType ivec4
#elif DestIsUint
#define DestType uvec4
#else
#error "Not all destination formats are accounted for"
#endif

layout(set = 0, binding = 0) uniform SRC_RESOURCE(SRC_RESOURCE_NAME) src;
layout(location = 0) out DestType dest;

layout(push_constant) uniform PushConstants {
    // Translation from source to destination coordinates.
    ivec2 srcOffset;
    ivec2 destOffset;
    int srcMip;
    int srcLayer;
    // Whether y needs to be flipped
    bool flipY;
    // Premultiplied alpha conversions
    bool premultiplyAlpha;
    bool unmultiplyAlpha;
    // Whether destination is emulated luminance/alpha.
    bool destHasLuminance;
    bool destIsAlpha;
    // Bits 0~3 tell whether R,G,B or A exist in destination, but as a result of format emulation.
    // Bit 0 is ignored, because R is always present.  For B and G, the result is set to 0 and for
    // A, the result is set to 1.
    int destDefaultChannelsMask;
} params;

void main()
{
    ivec2 destSubImageCoords = ivec2(gl_FragCoord.xy) - params.destOffset;

    ivec2 srcSubImageCoords = destSubImageCoords;

    // If flipping Y, srcOffset would contain the opposite y coordinate, so we can
    // simply reverse the direction in which y grows.
    if (params.flipY)
        srcSubImageCoords.y = -srcSubImageCoords.y;

#if SrcIsArray
    SrcType srcValue = texelFetch(src, ivec3(params.srcOffset + srcSubImageCoords, params.srcLayer), params.srcMip);
#else
    SrcType srcValue = texelFetch(src, params.srcOffset + srcSubImageCoords, params.srcMip);
#endif

    if (params.premultiplyAlpha)
    {
        srcValue.rgb *= srcValue.a;
    }
    else if (params.unmultiplyAlpha && srcValue.a > 0)
    {
        srcValue.rgb /= srcValue.a;
    }

    // Convert value to destination type.
    DestType destValue = DestType(srcValue);

    // If dest is luminance/alpha, it's implemented with R or RG.  Do the appropriate swizzle.
    if (params.destHasLuminance)
    {
        destValue.rg = destValue.ra;
    }
    else if (params.destIsAlpha)
    {
        destValue.r = destValue.a;
    }
    else
    {
        int defaultChannelsMask = params.destDefaultChannelsMask;
        if ((defaultChannelsMask & 2) != 0)
        {
            destValue.g = 0;
        }
        if ((defaultChannelsMask & 4) != 0)
        {
            destValue.b = 0;
        }
        if ((defaultChannelsMask & 8) != 0)
        {
            destValue.a = 1;
        }
    }

    dest = destValue;
}
