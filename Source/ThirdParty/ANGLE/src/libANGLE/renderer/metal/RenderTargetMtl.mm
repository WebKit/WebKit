//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetMtl.mm:
//    Implements the class methods for RenderTargetMtl.
//

#include "libANGLE/renderer/metal/RenderTargetMtl.h"

namespace rx
{
namespace
{
mtl::TextureRef LinearColorViewIfNeeded(const mtl::TextureRef &texture,
                                        gl::SrgbWriteControlMode srgbWriteControlMode)
{
    if (srgbWriteControlMode != gl::SrgbWriteControlMode::Linear || !texture ||
        !texture->hasLinearColorView())
    {
        return texture;
    }
    return texture->getLinearColorView();
}
}  // namespace

RenderTargetMtl::RenderTargetMtl() {}

RenderTargetMtl::~RenderTargetMtl()
{
    reset();
}

void RenderTargetMtl::set(const mtl::TextureRef &texture,
                          const mtl::MipmapNativeLevel &level,
                          uint32_t layer,
                          const mtl::Format &format)
{
    setWithImplicitMSTexture(texture, nullptr, level, layer, format);
}

void RenderTargetMtl::setWithImplicitMSTexture(const mtl::TextureRef &texture,
                                               const mtl::TextureRef &implicitMSTexture,
                                               const mtl::MipmapNativeLevel &level,
                                               uint32_t layer,
                                               const mtl::Format &format)
{
    mTexture           = texture;
    mImplicitMSTexture = implicitMSTexture;
    mLevelIndex        = level;
    mLayerIndex        = layer;
    mFormat            = format;
}

void RenderTargetMtl::setTexture(const mtl::TextureRef &texture)
{
    mTexture = texture;
}

void RenderTargetMtl::setImplicitMSTexture(const mtl::TextureRef &implicitMSTexture)
{
    mImplicitMSTexture = implicitMSTexture;
}

void RenderTargetMtl::duplicateFrom(const RenderTargetMtl &src)
{
    setWithImplicitMSTexture(src.getTexture(), src.getImplicitMSTexture(), src.getLevelIndex(),
                             src.getLayerIndex(), src.getFormat());
}

void RenderTargetMtl::reset()
{
    mTexture.reset();
    mImplicitMSTexture.reset();
    mLevelIndex = mtl::kZeroNativeMipLevel;
    mLayerIndex = 0;
    mFormat     = mtl::Format();
}

uint32_t RenderTargetMtl::getRenderSamples() const
{
    mtl::TextureRef implicitMSTex = getImplicitMSTexture();
    mtl::TextureRef tex           = getTexture();
    return implicitMSTex ? implicitMSTex->samples() : (tex ? tex->samples() : 1);
}

void RenderTargetMtl::toRenderPassAttachmentDesc(
    mtl::RenderPassAttachmentDesc *rpaDescOut,
    gl::SrgbWriteControlMode srgbWriteControlMode) const
{
    mtl::TextureRef implicitMSTex = getImplicitMSTexture();
    mtl::TextureRef tex           = getTexture();
    if (implicitMSTex)
    {
        rpaDescOut->texture        = LinearColorViewIfNeeded(implicitMSTex, srgbWriteControlMode);
        rpaDescOut->resolveTexture = LinearColorViewIfNeeded(tex, srgbWriteControlMode);
        rpaDescOut->resolveLevel   = mLevelIndex;
        rpaDescOut->resolveSliceOrDepth = mLayerIndex;
    }
    else
    {
        rpaDescOut->texture      = LinearColorViewIfNeeded(tex, srgbWriteControlMode);
        rpaDescOut->level        = mLevelIndex;
        rpaDescOut->sliceOrDepth = mLayerIndex;
    }
    rpaDescOut->blendable = mFormat.getCaps().blendable;
}

#if ANGLE_WEBKIT_EXPLICIT_RESOLVE_TARGET_ENABLED
void RenderTargetMtl::toRenderPassResolveAttachmentDesc(
    mtl::RenderPassAttachmentDesc *rpaDescOut,
    gl::SrgbWriteControlMode srgbWriteControlMode) const
{
    ASSERT(!getImplicitMSTexture());
    ASSERT(getRenderSamples() == 1);
    rpaDescOut->resolveTexture      = LinearColorViewIfNeeded(getTexture(), srgbWriteControlMode);
    rpaDescOut->resolveLevel        = mLevelIndex;
    rpaDescOut->resolveSliceOrDepth = mLayerIndex;
}
#endif
}  // namespace rx
