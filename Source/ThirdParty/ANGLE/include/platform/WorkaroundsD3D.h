//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WorkaroundsD3D.h: Workarounds for D3D driver bugs and other issues.

#ifndef ANGLE_PLATFORM_WORKAROUNDSD3D_H_
#define ANGLE_PLATFORM_WORKAROUNDSD3D_H_

// TODO(jmadill,zmo,geofflang): make a workarounds library that can operate
// independent of ANGLE's renderer. Workarounds should also be accessible
// outside of the Renderer.

namespace angle
{
struct CompilerWorkaroundsD3D
{
    bool skipOptimization   = false;
    bool useMaxOptimization = false;

    // IEEE strictness needs to be enabled for NANs to work.
    bool enableIEEEStrictness = false;
};

struct WorkaroundsD3D
{
    // On some systems, having extra rendertargets than necessary slows down the shader.
    // We can fix this by optimizing those out of the shader. At the same time, we can
    // work around a bug on some nVidia drivers that they ignore "null" render targets
    // in D3D11, by compacting the active color attachments list to omit null entries.
    bool mrtPerfWorkaround = false;

    bool setDataFasterThanImageUpload = false;

    // Some renderers can't disable mipmaps on a mipmapped texture (i.e. solely sample from level
    // zero, and ignore the other levels). D3D11 Feature Level 10+ does this by setting MaxLOD to
    // 0.0f in the Sampler state. D3D9 sets D3DSAMP_MIPFILTER to D3DTEXF_NONE. There is no
    // equivalent to this in D3D11 Feature Level 9_3. This causes problems when (for example) an
    // application creates a mipmapped texture2D, but sets GL_TEXTURE_MIN_FILTER to GL_NEAREST
    // (i.e disables mipmaps). To work around this, D3D11 FL9_3 has to create two copies of the
    // texture. The textures' level zeros are identical, but only one texture has mips.
    bool zeroMaxLodWorkaround = false;

    // Some renderers do not support Geometry Shaders so the Geometry Shader-based PointSprite
    // emulation will not work. To work around this, D3D11 FL9_3 has to use a different pointsprite
    // emulation that is implemented using instanced quads.
    bool useInstancedPointSpriteEmulation = false;

    // A bug fixed in NVIDIA driver version 347.88 < x <= 368.81 triggers a TDR when using
    // CopySubresourceRegion from a staging texture to a depth/stencil in D3D11. The workaround
    // is to use UpdateSubresource to trigger an extra copy. We disable this workaround on newer
    // NVIDIA driver versions because of a second driver bug present with the workaround enabled.
    // (See: http://anglebug.com/1452)
    bool depthStencilBlitExtraCopy = false;

    // The HLSL optimizer has a bug with optimizing "pow" in certain integer-valued expressions.
    // We can work around this by expanding the pow into a series of multiplies if we're running
    // under the affected compiler.
    bool expandIntegerPowExpressions = false;

    // NVIDIA drivers sometimes write out-of-order results to StreamOut buffers when transform
    // feedback is used to repeatedly write to the same buffer positions.
    bool flushAfterEndingTransformFeedback = false;

    // Some drivers (NVIDIA) do not take into account the base level of the texture in the results
    // of the HLSL GetDimensions builtin.
    bool getDimensionsIgnoresBaseLevel = false;

    // On some Intel drivers, HLSL's function texture.Load returns 0 when the parameter Location
    // is negative, even if the sum of Offset and Location is in range. This may cause errors when
    // translating GLSL's function texelFetchOffset into texture.Load, as it is valid for
    // texelFetchOffset to use negative texture coordinates as its parameter P when the sum of P
    // and Offset is in range. To work around this, we translate texelFetchOffset into texelFetch
    // by adding Offset directly to Location before reading the texture.
    bool preAddTexelFetchOffsets = false;

    // On some AMD drivers, 1x1 and 2x2 mips of depth/stencil textures aren't sampled correctly.
    // We can work around this bug by doing an internal blit to a temporary single-channel texture
    // before we sample.
    bool emulateTinyStencilTextures = false;

    // In Intel driver, the data with format DXGI_FORMAT_B5G6R5_UNORM will be parsed incorrectly.
    // This workaroud will disable B5G6R5 support when it's Intel driver. By default, it will use
    // R8G8B8A8 format. This bug is fixed in version 4539 on Intel drivers.
    bool disableB5G6R5Support = false;

    // On some Intel drivers, evaluating unary minus operator on integer may get wrong answer in
    // vertex shaders. To work around this bug, we translate -(int) into ~(int)+1.
    bool rewriteUnaryMinusOperator = false;

    // On some Intel drivers, using isnan() on highp float will get wrong answer. To work around
    // this bug, we use an expression to emulate function isnan().
    // Tracking bug: https://crbug.com/650547
    // This driver bug is fixed in 21.20.16.4542.
    bool emulateIsnanFloat = false;

    // On some Intel drivers, using clear() may not take effect. One of such situation is to clear
    // a target with width or height < 16. To work around this bug, we call clear() twice on these
    // platforms. Tracking bug: https://crbug.com/655534
    bool callClearTwiceOnSmallTarget = false;

    // On some Intel drivers, copying from staging storage to constant buffer storage does not
    // seem to work. Work around this by keeping system memory storage as a canonical reference
    // for buffer data.
    // D3D11-only workaround. See http://crbug.com/593024.
    bool useSystemMemoryForConstantBuffers = false;
};

}  // namespace angle

#endif  // ANGLE_PLATFORM_WORKAROUNDSD3D_H_
