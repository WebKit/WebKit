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

#include "common/vulkan/vk_headers.h"
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
class Resource;
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
              uint32_t levelIndexGL,
              uint32_t layerIndex);
    void reset();
    // This returns the serial from underlying ImageViewHelper, first assigning one if required
    ImageViewSerial getAssignImageViewSerial(ContextVk *contextVk) const;

    // Note: RenderTargets should be called in order, with the depth/stencil onRender last.
    angle::Result onColorDraw(ContextVk *contextVk);
    angle::Result onDepthStencilDraw(ContextVk *contextVk);

    vk::ImageHelper &getImageForRenderPass();
    const vk::ImageHelper &getImageForRenderPass() const;

    vk::ImageHelper &getImageForCopy() const;
    vk::ImageHelper &getImageForWrite() const;

    // For cube maps we use single-level single-layer 2D array views.
    angle::Result getImageView(ContextVk *contextVk, const vk::ImageView **imageViewOut) const;

    // For 3D textures, the 2D view created for render target is invalid to read from.  The
    // following will return a view to the whole image (for all types, including 3D and 2DArray).
    angle::Result getAndRetainCopyImageView(ContextVk *contextVk,
                                            const vk::ImageView **imageViewOut) const;

    const vk::Format &getImageFormat() const;
    gl::Extents getExtents() const;
    uint32_t getLevelIndex() const { return mLevelIndexGL; }
    uint32_t getLayerIndex() const { return mLayerIndex; }

    gl::ImageIndex getImageIndex() const;

    // Special mutator for Surface RenderTargets. Allows the Framebuffer to keep a single
    // RenderTargetVk pointer.
    void updateSwapchainImage(vk::ImageHelper *image, vk::ImageViewHelper *imageViews);

    angle::Result flushStagedUpdates(ContextVk *contextVk,
                                     vk::ClearValuesArray *deferredClears,
                                     uint32_t deferredClearIndex);

    void retainImageViews(ContextVk *contextVk) const;

    bool hasDefinedContent() const { return mContentDefined; }
    // Mark content as undefined so that certain optimizations are possible such as using DONT_CARE
    // as loadOp of the render target in the next renderpass.
    void invalidateEntireContent() { mContentDefined = false; }

  private:
    vk::ImageHelper *mImage;
    vk::ImageViewHelper *mImageViews;
    uint32_t mLevelIndexGL;
    uint32_t mLayerIndex;

    // Whether the render target has been invalidated.  If so, DONT_CARE is used instead of LOAD for
    // loadOp of this attachment.
    bool mContentDefined;
};

// A vector of rendertargets
using RenderTargetVector = std::vector<RenderTargetVk>;
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERTARGETVK_H_
