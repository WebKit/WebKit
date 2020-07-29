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
RenderTargetMtl::RenderTargetMtl() {}

RenderTargetMtl::~RenderTargetMtl()
{
    reset();
}

RenderTargetMtl::RenderTargetMtl(RenderTargetMtl &&other)
    : mTexture(std::move(other.mTexture)),
      mImplicitMSTexture(std::move(other.mImplicitMSTexture)),
      mLevelIndex(other.mLevelIndex),
      mLayerIndex(other.mLayerIndex)
{}

void RenderTargetMtl::set(const mtl::TextureRef &texture,
                          uint32_t level,
                          uint32_t layer,
                          const mtl::Format &format)
{
    set(texture, nullptr, level, layer, format);
}

void RenderTargetMtl::set(const mtl::TextureRef &texture,
                          const mtl::TextureRef &implicitMSTexture,
                          uint32_t level,
                          uint32_t layer,
                          const mtl::Format &format)
{
    mTexture           = texture;
    mImplicitMSTexture = implicitMSTexture;
    mLevelIndex        = level;
    mLayerIndex        = layer;
    mFormat            = &format;
}

void RenderTargetMtl::setTexture(const mtl::TextureRef &texture)
{
    mTexture = texture;
}

void RenderTargetMtl::setImplicitMSTexture(const mtl::TextureRef &implicitMSTexture)
{
    mImplicitMSTexture = implicitMSTexture;
}

void RenderTargetMtl::reset()
{
    mTexture.reset();
    mImplicitMSTexture.reset();
    mLevelIndex = 0;
    mLayerIndex = 0;
    mFormat     = nullptr;
}

uint32_t RenderTargetMtl::getRenderSamples() const
{
    return mImplicitMSTexture ? mImplicitMSTexture->samples()
                              : (mTexture ? mTexture->samples() : 1);
}
void RenderTargetMtl::toRenderPassAttachmentDesc(mtl::RenderPassAttachmentDesc *rpaDescOut) const
{
    rpaDescOut->texture           = mTexture;
    rpaDescOut->implicitMSTexture = mImplicitMSTexture;
    rpaDescOut->level             = mLevelIndex;
    rpaDescOut->sliceOrDepth      = mLayerIndex;
}
}
