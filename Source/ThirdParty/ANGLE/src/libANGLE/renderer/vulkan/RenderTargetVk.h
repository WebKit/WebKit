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

enum class RenderTargetTransience
{
    // Regular render targets that load and store from the image.
    Default,
    // Multisampled-render-to-texture textures, where the implicit multisampled image is transient,
    // but the resolved image is persistent.
    MultisampledTransient,
    // Multisampled-render-to-texture depth/stencil textures.
    EntirelyTransient,
};

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
              vk::ImageHelper *resolveImage,
              vk::ImageViewHelper *resolveImageViews,
              gl::LevelIndex levelIndexGL,
              uint32_t layerIndex,
              RenderTargetTransience transience);
    void reset();

    vk::ImageViewSubresourceSerial getDrawSubresourceSerial() const;
    vk::ImageViewSubresourceSerial getResolveSubresourceSerial() const;

    // Note: RenderTargets should be called in order, with the depth/stencil onRender last.
    void onColorDraw(ContextVk *contextVk);
    void onDepthStencilDraw(ContextVk *contextVk);

    vk::ImageHelper &getImageForRenderPass();
    const vk::ImageHelper &getImageForRenderPass() const;

    vk::ImageHelper &getResolveImageForRenderPass();
    const vk::ImageHelper &getResolveImageForRenderPass() const;

    vk::ImageHelper &getImageForCopy() const;
    vk::ImageHelper &getImageForWrite() const;

    // For cube maps we use single-level single-layer 2D array views.
    angle::Result getImageView(ContextVk *contextVk, const vk::ImageView **imageViewOut) const;
    angle::Result getResolveImageView(ContextVk *contextVk,
                                      const vk::ImageView **imageViewOut) const;

    // For 3D textures, the 2D view created for render target is invalid to read from.  The
    // following will return a view to the whole image (for all types, including 3D and 2DArray).
    angle::Result getAndRetainCopyImageView(ContextVk *contextVk,
                                            const vk::ImageView **imageViewOut) const;

    const vk::Format &getImageFormat() const;
    gl::Extents getExtents() const;
    gl::LevelIndex getLevelIndex() const { return mLevelIndexGL; }
    uint32_t getLayerIndex() const { return mLayerIndex; }

    gl::ImageIndex getImageIndex() const;

    // Special mutator for Surface RenderTargets. Allows the Framebuffer to keep a single
    // RenderTargetVk pointer.
    void updateSwapchainImage(vk::ImageHelper *image,
                              vk::ImageViewHelper *imageViews,
                              vk::ImageHelper *resolveImage,
                              vk::ImageViewHelper *resolveImageViews);

    angle::Result flushStagedUpdates(ContextVk *contextVk,
                                     vk::ClearValuesArray *deferredClears,
                                     uint32_t deferredClearIndex);

    void retainImageViews(ContextVk *contextVk) const;

    bool hasDefinedContent() const;
    bool hasDefinedStencilContent() const;
    // Mark content as undefined so that certain optimizations are possible such as using DONT_CARE
    // as loadOp of the render target in the next renderpass.
    void invalidateEntireContent(ContextVk *contextVk);
    void invalidateEntireStencilContent(ContextVk *contextVk);
    void restoreEntireContent();
    void restoreEntireStencilContent();

    // See the description of mTransience for details of how the following two can interact.
    bool hasResolveAttachment() const { return mResolveImage != nullptr && !isEntirelyTransient(); }
    bool isImageTransient() const { return mTransience != RenderTargetTransience::Default; }
    bool isEntirelyTransient() const
    {
        return mTransience == RenderTargetTransience::EntirelyTransient;
    }

  private:
    angle::Result getImageViewImpl(ContextVk *contextVk,
                                   const vk::ImageHelper &image,
                                   vk::ImageViewHelper *imageViews,
                                   const vk::ImageView **imageViewOut) const;

    vk::ImageViewSubresourceSerial getSubresourceSerialImpl(vk::ImageViewHelper *imageViews) const;

    bool isResolveImageOwnerOfData() const;
    vk::ImageHelper *getOwnerOfData() const;

    // The color or depth/stencil attachment of the framebuffer and its view.
    vk::ImageHelper *mImage;
    vk::ImageViewHelper *mImageViews;

    // If present, this is the corresponding resolve attachment and its view.  This is used to
    // implement GL_EXT_multisampled_render_to_texture, so while the rendering is done on mImage
    // during the renderpass, the resolved image is the one that actually holds the data.  This
    // means that data uploads and blit are done on this image, copies are done out of this image
    // etc.  This means that if there is no clear, and hasDefined*Content(), the contents of
    // mResolveImage must be copied to mImage since the loadOp of the attachment must be set to
    // LOAD.
    vk::ImageHelper *mResolveImage;
    vk::ImageViewHelper *mResolveImageViews;

    // Which subresource of the image is used as render target.
    gl::LevelIndex mLevelIndexGL;
    uint32_t mLayerIndex;

    // If resolve attachment exists, |mTransience| could be *Transient if the multisampled results
    // need to be discarded.
    //
    // - GL_EXT_multisampled_render_to_texture[2]: this is |MultisampledTransient| for render
    //   targets created from color textures, as well as color or depth/stencil renderbuffers.
    // - GL_EXT_multisampled_render_to_texture2: this is |EntirelyTransient| for depth/stencil
    //   textures per this extension, even though a resolve attachment is not even provided.
    //
    // Based on the above, we have:
    //
    //                     mResolveImage == nullptr
    //                        Normal rendering
    // Default                   No resolve
    //                         storeOp = STORE
    //                      Owner of data: mImage
    //
    //      ---------------------------------------------
    //
    //                     mResolveImage != nullptr
    //               GL_EXT_multisampled_render_to_texture
    // Multisampled               Resolve
    // Transient             storeOp = DONT_CARE
    //                     resolve storeOp = STORE
    //                   Owner of data: mResolveImage
    //
    //      ---------------------------------------------
    //
    //                     mResolveImage != nullptr
    //               GL_EXT_multisampled_render_to_texture2
    // Entirely                  No Resolve
    // Transient             storeOp = DONT_CARE
    //                   Owner of data: mResolveImage
    //
    // In the above, storeOp of the resolve attachment is always STORE.  If |Default|, storeOp is
    // affected by a framebuffer invalidate call.  Note that even though |EntirelyTransient| has a
    // resolve attachment, it is not used.  The only purpose of |mResolveImage| is to store deferred
    // clears.
    RenderTargetTransience mTransience;
};

// A vector of rendertargets
using RenderTargetVector = std::vector<RenderTargetVk>;
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERTARGETVK_H_
