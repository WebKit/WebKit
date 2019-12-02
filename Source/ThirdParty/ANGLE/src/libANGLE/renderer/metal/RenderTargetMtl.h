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

    void set(const mtl::TextureRef &texture, size_t level, size_t layer, const mtl::Format &format);
    void set(const mtl::TextureRef &texture);
    void reset();

    const mtl::TextureRef &getTexture() const { return mTexture; }
    size_t getLevelIndex() const { return mLevelIndex; }
    size_t getLayerIndex() const { return mLayerIndex; }
    const mtl::Format *getFormat() const { return mFormat; }

    void toRenderPassAttachmentDesc(mtl::RenderPassAttachmentDesc *rpaDescOut) const;

  private:
    mtl::TextureRef mTexture;
    size_t mLevelIndex         = 0;
    size_t mLayerIndex         = 0;
    const mtl::Format *mFormat = nullptr;
};
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_RENDERTARGETMTL_H */
