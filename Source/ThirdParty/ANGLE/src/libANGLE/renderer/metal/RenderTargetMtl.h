//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetMtl.h:
//    Defines the class interface for RenderTargetMtl.
//

#ifndef LIBANGLE_RENDERER_METAL_RENDERTARGETMTL_H_
#define LIBANGLE_RENDERER_METAL_RENDERTARGETMTL_H_

#import <Metal/Metal.h>

#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/renderer/metal/mtl_format_utils.h"
#include "libANGLE/renderer/metal/mtl_resources.h"
#include "libANGLE/renderer/metal/mtl_state_cache.h"

namespace rx
{

// This is a very light-weight class that does not own to the resources it points to.
// It's meant only to copy across some information from a FramebufferAttachment to the
// business rendering logic.
class RenderTargetMtl final : public FramebufferAttachmentRenderTarget
{
  public:
    RenderTargetMtl();
    ~RenderTargetMtl() override;

    // Used in std::vector initialization.
    RenderTargetMtl(RenderTargetMtl &&other);

    void set(const mtl::TextureRef &texture,
             const mtl::MipmapNativeLevel &level,
             uint32_t layer,
             const mtl::Format &format);
    void setWithImplicitMSTexture(const mtl::TextureRef &texture,
                                  const mtl::TextureRef &implicitMSTexture,
                                  const mtl::MipmapNativeLevel &level,
                                  uint32_t layer,
                                  const mtl::Format &format);
    void setTexture(const mtl::TextureRef &texture);
    void setImplicitMSTexture(const mtl::TextureRef &implicitMSTexture);
    void duplicateFrom(const RenderTargetMtl &src);
    void reset();

    mtl::TextureRef getTexture() const { return mTextureRenderTargetInfo->getTextureRef(); }
    mtl::TextureRef getImplicitMSTexture() const { return mTextureRenderTargetInfo->getImplicitMSTextureRef(); }
    const mtl::MipmapNativeLevel &getLevelIndex() const { return mTextureRenderTargetInfo->level; }
    uint32_t getLayerIndex() const { return mTextureRenderTargetInfo->sliceOrDepth; }
    uint32_t getRenderSamples() const;
    const mtl::Format *getFormat() const { return mFormat; }

    void toRenderPassAttachmentDesc(mtl::RenderPassAttachmentDesc *rpaDescOut) const;

  private:
// This is shared pointer to avoid crashing when texture deleted after bound to a frame buffer.
    std::shared_ptr<mtl::RenderPassAttachmentTextureTargetDesc> mTextureRenderTargetInfo;
    const mtl::Format *mFormat = nullptr;
};
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_RENDERTARGETMTL_H */
