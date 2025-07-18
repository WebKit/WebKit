//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetWgpu.h:
//    Defines the class interface for RenderTargetWgpu.
//

#ifndef LIBANGLE_RENDERER_WGPU_RENDERTARGETWGPU_H_
#define LIBANGLE_RENDERER_WGPU_RENDERTARGETWGPU_H_

#include <stdint.h>
#include <webgpu/webgpu.h>

#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/wgpu/wgpu_helpers.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{
class RenderTargetWgpu final : public FramebufferAttachmentRenderTarget
{
  public:
    RenderTargetWgpu();
    ~RenderTargetWgpu() override;

    // Used in std::vector initialization.
    RenderTargetWgpu(RenderTargetWgpu &&other);

    void set(webgpu::ImageHelper *image,
             const webgpu::TextureViewHandle &texture,
             const webgpu::LevelIndex level,
             uint32_t layer,
             WGPUTextureFormat format);
    void reset();

    angle::Result flushImageStagedUpdates(ContextWgpu *contextWgpu,
                                          webgpu::ClearValuesArray *deferredClears,
                                          uint32_t deferredClearIndex);

    webgpu::TextureViewHandle getTextureView() { return mTextureView; }
    webgpu::ImageHelper *getImage() { return mImage; }
    webgpu::LevelIndex getLevelIndex() const { return mLevelIndex; }
    uint32_t getLayer() const { return mLayerIndex; }
    gl::LevelIndex getGlLevel() const { return mImage->toGlLevel(mLevelIndex); }

  private:
    webgpu::ImageHelper *mImage = nullptr;
    // TODO(liza): move TextureView into ImageHelper.
    webgpu::TextureViewHandle mTextureView;
    webgpu::LevelIndex mLevelIndex{0};
    uint32_t mLayerIndex               = 0;
    const WGPUTextureFormat *mFormat   = nullptr;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_RENDERTARGETWGPU_H_
