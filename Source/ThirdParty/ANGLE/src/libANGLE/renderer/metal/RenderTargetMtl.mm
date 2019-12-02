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
      mLevelIndex(other.mLevelIndex),
      mLayerIndex(other.mLayerIndex)
{}

void RenderTargetMtl::set(const mtl::TextureRef &texture,
                          size_t level,
                          size_t layer,
                          const mtl::Format &format)
{
    mTexture    = texture;
    mLevelIndex = level;
    mLayerIndex = layer;
    mFormat     = &format;
}

void RenderTargetMtl::set(const mtl::TextureRef &texture)
{
    mTexture = texture;
}

void RenderTargetMtl::reset()
{
    mTexture.reset();
    mLevelIndex = 0;
    mLayerIndex = 0;
    mFormat     = nullptr;
}

void RenderTargetMtl::toRenderPassAttachmentDesc(mtl::RenderPassAttachmentDesc *rpaDescOut) const
{
    rpaDescOut->texture = mTexture;
    rpaDescOut->level   = static_cast<uint32_t>(mLevelIndex);
    rpaDescOut->slice   = static_cast<uint32_t>(mLayerIndex);
}
}
