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
RenderTargetMtl::RenderTargetMtl() :
  mTextureRenderTargetInfo(std::make_shared<mtl::RenderPassAttachmentTextureTargetDesc>()),
  mFormat(nullptr)
{}

RenderTargetMtl::~RenderTargetMtl()
{
    reset();
}

RenderTargetMtl::RenderTargetMtl(RenderTargetMtl &&other)
    : mTextureRenderTargetInfo(std::move(other.mTextureRenderTargetInfo))
{}

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
    mTextureRenderTargetInfo->texture           = texture;
    mTextureRenderTargetInfo->implicitMSTexture = implicitMSTexture;
    mTextureRenderTargetInfo->level        = level;
    mTextureRenderTargetInfo->sliceOrDepth        = layer;
    mFormat            = &format;
}

void RenderTargetMtl::setTexture(const mtl::TextureRef &texture)
{
    mTextureRenderTargetInfo->texture = texture;
}

void RenderTargetMtl::setImplicitMSTexture(const mtl::TextureRef &implicitMSTexture)
{
    mTextureRenderTargetInfo->implicitMSTexture = implicitMSTexture;
}

void RenderTargetMtl::duplicateFrom(const RenderTargetMtl &src)
{
    setWithImplicitMSTexture(src.getTexture(), src.getImplicitMSTexture(), src.getLevelIndex(),
                             src.getLayerIndex(), *src.getFormat());
}

void RenderTargetMtl::reset()
{
    mTextureRenderTargetInfo->texture.reset();
    mTextureRenderTargetInfo->implicitMSTexture.reset();
    mTextureRenderTargetInfo->level        = mtl::kZeroNativeMipLevel;
    mTextureRenderTargetInfo->sliceOrDepth = 0;
    mFormat                                = nullptr;
}

uint32_t RenderTargetMtl::getRenderSamples() const
{
    return mTextureRenderTargetInfo->getRenderSamples();
}
void RenderTargetMtl::toRenderPassAttachmentDesc(mtl::RenderPassAttachmentDesc *rpaDescOut) const
{
    rpaDescOut->renderTarget = mTextureRenderTargetInfo;
}
}
