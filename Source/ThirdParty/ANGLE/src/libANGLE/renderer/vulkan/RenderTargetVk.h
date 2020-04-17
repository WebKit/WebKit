//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetVk:
//   Wrapper around a Vulkan renderable resource, using an ImageView.
//

#ifndef LIBANGLE_RENDERER_VULKAN_RENDERTARGETVK_H_
#define LIBANGLE_RENDERER_VULKAN_RENDERTARGETVK_H_

#include "volk.h"

#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
namespace vk
{
struct Format;
class FramebufferHelper;
class ImageHelper;
class ImageView;
class CommandGraphResource;
class RenderPassDesc;
}  // namespace vk

class ContextVk;
class TextureVk;

// This is a very light-weight class that does not own to the resources it points to.
// It's meant only to copy across some information from a FramebufferAttachment to the
// business rendering logic. It stores Images and ImageViews by pointer for performance.
class RenderTargetVk final : public FramebufferAttachmentRenderTarget
{
  public:
    RenderTargetVk();
    ~RenderTargetVk() override;

    // Used in std::vector initialization.
    RenderTargetVk(RenderTargetVk &&other);

    void init(vk::ImageHelper *image,
              vk::ImageViewHelper *imageViews,
              uint32_t levelIndex,
              uint32_t layerIndex);
    void reset();

    // Note: RenderTargets should be called in order, with the depth/stencil onRender last.
    angle::Result onColorDraw(ContextVk *contextVk,
                              vk::FramebufferHelper *framebufferVk,
                              vk::CommandBuffer *commandBuffer);
    angle::Result onDepthStencilDraw(ContextVk *contextVk,
                                     vk::FramebufferHelper *framebufferVk,
                                     vk::CommandBuffer *commandBuffer);

    vk::ImageHelper &getImage();
    const vk::ImageHelper &getImage() const;

    // getImageForRead will also transition the resource to the given layout.
    vk::ImageHelper *getImageForRead(ContextVk *contextVk,
                                     vk::CommandGraphResource *readingResource,
                                     vk::ImageLayout layout,
                                     vk::CommandBuffer *commandBuffer);
    vk::ImageHelper *getImageForWrite(ContextVk *contextVk,
                                      vk::CommandGraphResource *writingResource) const;

    // For cube maps we use single-level single-layer 2D array views.
    angle::Result getImageView(ContextVk *contextVk, const vk::ImageView **imageViewOut) const;

    const vk::Format &getImageFormat() const;
    gl::Extents getExtents() const;
    uint32_t getLevelIndex() const { return mLevelIndex; }
    uint32_t getLayerIndex() const { return mLayerIndex; }

    // Special mutator for Surface RenderTargets. Allows the Framebuffer to keep a single
    // RenderTargetVk pointer.
    void updateSwapchainImage(vk::ImageHelper *image, vk::ImageViewHelper *imageViews);

    angle::Result flushStagedUpdates(ContextVk *contextVk);

    void onImageViewAccess(ContextVk *contextVk) const;

  private:
    vk::ImageHelper *mImage;
    vk::ImageViewHelper *mImageViews;
    uint32_t mLevelIndex;
    uint32_t mLayerIndex;
};

// A vector of rendertargets
using RenderTargetVector = std::vector<RenderTargetVk>;

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERTARGETVK_H_
