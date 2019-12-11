//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BlitResolve.frag: Blit color or depth/stencil images, or resolve multisampled ones.

#version 450 core

#define MAKE_SRC_RESOURCE(prefix, type) prefix ## type

#if BlitColorFloat

#define IsBlitColor 1
#define COLOR_SRC_RESOURCE(type) type
#define ColorType vec4

#elif BlitColorInt

#define IsBlitColor 1
#define COLOR_SRC_RESOURCE(type) MAKE_SRC_RESOURCE(i, type)
#define ColorType ivec4

#elif BlitColorUint

#define IsBlitColor 1
#define COLOR_SRC_RESOURCE(type) MAKE_SRC_RESOURCE(u, type)
#define ColorType uvec4

#elif BlitDepth

#define IsBlitDepth 1

#elif BlitStencil

#define IsBlitStencil 1

#elif BlitDepthStencil

#define IsBlitDepth 1
#define IsBlitStencil 1

#else

#error "Not all resolve targets are accounted for"

#endif

#if IsBlitColor && (IsBlitDepth || IsBlitStencil)
#error "The shader doesn't blit color and depth/stencil at the same time."
#endif

#if IsResolve
#extension GL_EXT_samplerless_texture_functions : require
#endif
#if IsBlitStencil
#extension GL_ARB_shader_stencil_export : require
#endif

#define MAKE_SRC_RESOURCE(prefix, type) prefix ## type

#define DEPTH_SRC_RESOURCE(type) type
#define STENCIL_SRC_RESOURCE(type) MAKE_SRC_RESOURCE(u, type)

#if IsResolve

#define CoordType ivec2
#if SrcIsArray
#define SRC_RESOURCE_NAME texture2DMSArray
#define TEXEL_FETCH(src, coord, sample) texelFetch(src, ivec3(coord, params.srcLayer), sample)
#else
#define SRC_RESOURCE_NAME texture2DMS
#define TEXEL_FETCH(src, coord, sample) texelFetch(src, coord, sample)
#endif

#define COLOR_TEXEL_FETCH(src, coord, sample) TEXEL_FETCH(src, coord, sample)
#define DEPTH_TEXEL_FETCH(src, coord, sample) TEXEL_FETCH(src, coord, sample)
#define STENCIL_TEXEL_FETCH(src, coord, sample) TEXEL_FETCH(src, coord, sample)

#else

#define CoordType vec2
#if SrcIsArray
#define SRC_RESOURCE_NAME texture2DArray
#define SRC_SAMPLER_NAME sampler2DArray
#define TEXEL_FETCH(src, coord, sample) texture(src, vec3(coord * params.invSrcExtent, params.srcLayer))
#else
#define SRC_RESOURCE_NAME texture2D
#define SRC_SAMPLER_NAME sampler2D
#define TEXEL_FETCH(src, coord, sample) texture(src, coord * params.invSrcExtent)
#endif

#define COLOR_TEXEL_FETCH(src, coord, sample) TEXEL_FETCH(COLOR_SRC_RESOURCE(SRC_SAMPLER_NAME)(src, blitSampler), coord, sample)
#define DEPTH_TEXEL_FETCH(src, coord, sample) TEXEL_FETCH(DEPTH_SRC_RESOURCE(SRC_SAMPLER_NAME)(src, blitSampler), coord, sample)
#define STENCIL_TEXEL_FETCH(src, coord, sample) TEXEL_FETCH(STENCIL_SRC_RESOURCE(SRC_SAMPLER_NAME)(src, blitSampler), coord, sample)

#endif  // IsResolve

layout(push_constant) uniform PushConstants {
    // Translation from source to destination coordinates.
    CoordType offset;
    vec2 stretch;
    vec2 invSrcExtent;
    int srcLayer;
    int samples;
    float invSamples;
    // Mask to output only to color attachments that are actually present.
    int outputMask;
    // Flip control.
    bool flipX;
    bool flipY;
} params;

#if IsBlitColor
layout(set = 0, binding = 0) uniform COLOR_SRC_RESOURCE(SRC_RESOURCE_NAME) color;

layout(location = 0) out ColorType colorOut0;
layout(location = 1) out ColorType colorOut1;
layout(location = 2) out ColorType colorOut2;
layout(location = 3) out ColorType colorOut3;
layout(location = 4) out ColorType colorOut4;
layout(location = 5) out ColorType colorOut5;
layout(location = 6) out ColorType colorOut6;
layout(location = 7) out ColorType colorOut7;
#endif
#if IsBlitDepth
layout(set = 0, binding = 0) uniform DEPTH_SRC_RESOURCE(SRC_RESOURCE_NAME) depth;
#endif
#if IsBlitStencil
layout(set = 0, binding = 1) uniform STENCIL_SRC_RESOURCE(SRC_RESOURCE_NAME) stencil;
#endif

#if !IsResolve
layout(set = 0, binding = 2) uniform sampler blitSampler;
#endif

void main()
{
    // Assume only one direction; x.  We are blitting from source to destination either flipped or
    // not, with a stretch factor of T.  If resolving, T == 1.  Note that T here is:
    //
    //     T = SrcWidth / DstWidth
    //
    // Assume the blit offset in source is S and in destination D.  If flipping, S has the
    // coordinates of the opposite size of the rectangle.  In this shader, we have the fragment
    // coordinates, X, which is a point in the destination buffer.  We need to map this to the
    // source buffer to know where to sample from.
    //
    // If there's no flipping:
    //
    //     S Y             D    X
    //     +-x----+   ->   +----x-----------+
    //
    //        Y = S + (X - D) * T
    //     => Y = TX - (DT - S)
    //
    // If there's flipping:
    //
    //          Y S        D    X
    //     +----x-+   ->   +----x-----------+
    //
    //        Y = S - (X - D) * T
    //     => Y = -(TX - (DT + S))
    //
    // The above can be implemented as:
    //
    //     !Flip: Y = TX - O      where O = DT-S
    //      Flip: Y = -(TX - O)   where O = DT+S
    //
    // Note that T is params.stretch and O is params.offset.

    // X
    CoordType srcImageCoords = CoordType(gl_FragCoord.xy);  // X
#if !IsResolve
    srcImageCoords *= params.stretch;                       // TX
#endif
    srcImageCoords -= params.offset;                        // TX - O

    // If flipping, negate the coordinates.
    if (params.flipX)
        srcImageCoords.x = -srcImageCoords.x;
    if (params.flipY)
        srcImageCoords.y = -srcImageCoords.y;

#if IsBlitColor
#if IsResolve
    ColorType colorValue = ColorType(0, 0, 0, 1);
    for (int i = 0; i < params.samples; ++i)
    {
        colorValue += COLOR_TEXEL_FETCH(color, srcImageCoords, i);
    }
#if IsFloat
    colorValue *= params.invSamples;
#else
    colorValue = ColorType(round(colorValue * params.invSamples));
#endif

#else
    ColorType colorValue = COLOR_TEXEL_FETCH(color, srcImageCoords, 0);
#endif

    // Note: not exporting to render targets that are not present optimizes the number of export
    // instructions, which would have otherwise been a likely bottleneck.
    if ((params.outputMask & (1 << 0)) != 0)
    {
        colorOut0 = colorValue;
    }
    if ((params.outputMask & (1 << 1)) != 0)
    {
        colorOut1 = colorValue;
    }
    if ((params.outputMask & (1 << 2)) != 0)
    {
        colorOut2 = colorValue;
    }
    if ((params.outputMask & (1 << 3)) != 0)
    {
        colorOut3 = colorValue;
    }
    if ((params.outputMask & (1 << 4)) != 0)
    {
        colorOut4 = colorValue;
    }
    if ((params.outputMask & (1 << 5)) != 0)
    {
        colorOut5 = colorValue;
    }
    if ((params.outputMask & (1 << 6)) != 0)
    {
        colorOut6 = colorValue;
    }
    if ((params.outputMask & (1 << 7)) != 0)
    {
        colorOut7 = colorValue;
    }
#endif  // IsBlitColor

    // Note: always resolve depth/stencil using sample 0.  GLES3 gives us freedom in choosing how
    // to resolve depth/stencil images.

#if IsBlitDepth
    gl_FragDepth = DEPTH_TEXEL_FETCH(depth, srcImageCoords, 0).x;
#endif  // IsBlitDepth

#if IsBlitStencil
    gl_FragStencilRefARB = int(STENCIL_TEXEL_FETCH(stencil, srcImageCoords, 0).x);
#endif  // IsBlitStencil
}
