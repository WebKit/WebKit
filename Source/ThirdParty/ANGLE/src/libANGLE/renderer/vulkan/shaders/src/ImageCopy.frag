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

#if SrcIs2D
#define SRC_RESOURCE_NAME texture2D
#elif SrcIs2DArray
#define SRC_RESOURCE_NAME texture2DArray
#elif SrcIs3D
#define SRC_RESOURCE_NAME texture3D
#else
#error "Not all source types are accounted for"
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
layout(location = 0) out DestType dst;

layout(push_constant) uniform PushConstants {
    // Translation from source to destination coordinates.
    ivec2 srcOffset;
    ivec2 dstOffset;
    int srcMip;
    int srcLayer;
    // Whether x and/or y need to be flipped
    bool flipX;
    bool flipY;
    // Premultiplied alpha conversions
    bool premultiplyAlpha;
    bool unmultiplyAlpha;
    // Whether destination is emulated luminance/alpha.
    bool dstHasLuminance;
    bool dstIsAlpha;
    // Whether source or destination are sRGB.  They are brought to linear space for alpha
    // premultiply/unmultiply, as well as to ensure the copy doesn't change values due to sRGB
    // transformation.
    bool srcIsSRGB;
    bool dstIsSRGB;
    // Bits 0~3 tell whether R,G,B or A exist in destination, but as a result of format emulation.
    // Bit 0 is ignored, because R is always present.  For B and G, the result is set to 0 and for
    // A, the result is set to 1.
    int dstDefaultChannelsMask;
    bool rotateXY;
} params;

#if SrcIsFloat
float linearToSRGB(float linear)
{
    // sRGB transform: y = sRGB(x) where x is linear and y is the sRGB encoding:
    //
    //    x <= 0.0031308: y = x * 12.92
    //    o.w.          : y = 1.055 * x^(1/2.4) - 0.055
    if (linear <= 0.0031308)
    {
        return linear * 12.92;
    }
    else
    {
        return pow(linear, (1.0f / 2.4f)) * 1.055f - 0.055f;
    }
}
#endif

#if DestIsFloat
float sRGBToLinear(float sRGB)
{
    // sRGB inverse transform: x = sRGB^(-1)(y) where x is linear and y is the sRGB encoding:
    //
    //    y <= 0.04045: x = y / 12.92
    //    o.w.          : x = ((y + 0.055) / 1.055)^(2.4)
    if (sRGB <= 0.04045)
    {
        return sRGB / 12.92;
    }
    else
    {
        return pow((sRGB + 0.055f) / 1.055f, 2.4f);
    }
}
#endif

void main()
{
    ivec2 dstSubImageCoords = ivec2(gl_FragCoord.xy) - params.dstOffset;

    ivec2 srcSubImageCoords = dstSubImageCoords;

    // If flipping X and/or Y, srcOffset would contain the opposite x and/or y coordinate, so we
    // can simply reverse the direction in which x and/or y grows.
    if (params.flipX)
    {
        srcSubImageCoords.x = -srcSubImageCoords.x;
    }
    if (params.flipY)
    {
        srcSubImageCoords.y = -srcSubImageCoords.y;
    }
    if (params.rotateXY)
    {
        srcSubImageCoords.xy = srcSubImageCoords.yx;
    }

#if SrcIs2D
    SrcType srcValue = texelFetch(src, params.srcOffset + srcSubImageCoords, params.srcMip);
#elif SrcIs2DArray || SrcIs3D
    SrcType srcValue = texelFetch(src, ivec3(params.srcOffset + srcSubImageCoords, params.srcLayer), params.srcMip);
#else
#error "Not all source types are accounted for"
#endif

    // Note: sRGB formats are unorm, so SrcIsFloat must be necessarily set
#if SrcIsFloat
    if (params.srcIsSRGB)
    {
        // If src is sRGB, then texelFetch has performed an sRGB->linear transformation.  We need to
        // undo that to get back to the original values in the texture.  This is done to avoid
        // creating a non-sRGB view of the texture, which would require recreating it with the
        // VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT flag.

        srcValue.r = linearToSRGB(srcValue.r);
        srcValue.g = linearToSRGB(srcValue.g);
        srcValue.b = linearToSRGB(srcValue.b);
    }
#endif

    if (params.premultiplyAlpha)
    {
        srcValue.rgb *= srcValue.a;
    }
    else if (params.unmultiplyAlpha && srcValue.a > 0)
    {
        srcValue.rgb /= srcValue.a;
    }

#if SrcIsFloat && !DestIsFloat
    srcValue *= 255.0;
#endif

    // Convert value to destination type.
    DestType dstValue = DestType(srcValue);

#if !SrcIsFloat && DestIsFloat
    dstValue /= 255.0;
#endif

    // Note: sRGB formats are unorm, so DestIsFloat must be necessarily set
#if DestIsFloat
    if (params.dstIsSRGB)
    {
        // If dst is sRGB, then export will perform a linear->sRGB transformation.  We need to
        // preemptively undo that so the values will be exported unchanged.This is done to avoid
        // creating a non-sRGB view of the texture, which would require recreating it with the
        // VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT flag.

        dstValue.r = sRGBToLinear(dstValue.r);
        dstValue.g = sRGBToLinear(dstValue.g);
        dstValue.b = sRGBToLinear(dstValue.b);
    }
#endif

    // If dst is luminance/alpha, it's implemented with R or RG.  Do the appropriate swizzle.
    if (params.dstHasLuminance)
    {
        dstValue.rg = dstValue.ra;
    }
    else if (params.dstIsAlpha)
    {
        dstValue.r = dstValue.a;
    }
    else
    {
        int defaultChannelsMask = params.dstDefaultChannelsMask;
        if ((defaultChannelsMask & 2) != 0)
        {
            dstValue.g = 0;
        }
        if ((defaultChannelsMask & 4) != 0)
        {
            dstValue.b = 0;
        }
        if ((defaultChannelsMask & 8) != 0)
        {
            dstValue.a = 1;
        }
    }

    dst = dstValue;
}
