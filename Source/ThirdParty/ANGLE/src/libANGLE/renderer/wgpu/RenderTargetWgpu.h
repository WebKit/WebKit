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

#include <dawn/webgpu_cpp.h>
#include <stdint.h>

#include "libANGLE/FramebufferAttachment.h"
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

    void set(const wgpu::TextureView &texture,
             const webgpu::LevelIndex level,
             uint32_t layer,
             const wgpu::TextureFormat &format);
    void setTexture(const wgpu::TextureView &texture);
    void reset();

    wgpu::TextureView getTexture() { return mTexture; }

  private:
    wgpu::TextureView mTexture;
    webgpu::LevelIndex mLevelIndex{0};
    uint32_t mLayerIndex               = 0;
    const wgpu::TextureFormat *mFormat = nullptr;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_RENDERTARGETWGPU_H_
