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
              UniqueSerial imageSiblingSerial,
              gl::LevelIndex levelIndexGL,
              uint32_t layerIndex,
              uint32_t layerCount,
              RenderTargetTransience transience);

    vk::ImageOrBufferViewSubresourceSerial getDrawSubresourceSerial() const;
    vk::ImageOrBufferViewSubresourceSerial getResolveSubresourceSerial() const;

    // Note: RenderTargets should be called in order, with the depth/stencil onRender last.
    void onColorDraw(ContextVk *contextVk,
                     uint32_t framebufferLayerCount,
                     vk::PackedAttachmentIndex index);
    void onColorResolve(ContextVk *contextVk, uint32_t framebufferLayerCount);
    void onDepthStencilDraw(ContextVk *contextVk, uint32_t framebufferLayerCount);

    vk::ImageHelper &getImageForRenderPass();
    const vk::ImageHelper &getImageForRenderPass() const;

    vk::ImageHelper &getResolveImageForRenderPass();
    const vk::ImageHelper &getResolveImageForRenderPass() const;

    vk::ImageHelper &getImageForCopy() const;
    vk::ImageHelper &getImageForWrite() const;

    // For cube maps we use single-level single-layer 2D array views.
    angle::Result getImageView(vk::Context *contextVk, const vk::ImageView **imageViewOut) const;
    angle::Result getImageViewWithColorspace(vk::Context *context,
                                             gl::SrgbWriteControlMode srgbWriteContrlMode,
                                             const vk::ImageView **imageViewOut) const;
    angle::Result getResolveImageView(vk::Context *context,
                                      const vk::ImageView **imageViewOut) const;

    // For 3D textures, the 2D view created for render target is invalid to read from.  The
    // following will return a view to the whole image (for all types, including 3D and 2DArray).
    angle::Result getCopyImageView(vk::Context *context, const vk::ImageView **imageViewOut) const;

    angle::FormatID getImageActualFormatID() const;
    const angle::Format &getImageActualFormat() const;
    angle::FormatID getImageIntendedFormatID() const;
    const angle::Format &getImageIntendedFormat() const;

    gl::Extents getExtents() const;
    gl::Extents getRotatedExtents() const;
    gl::LevelIndex getLevelIndex() const { return mLevelIndexGL; }
    uint32_t getLayerIndex() const { return mLayerIndex; }
    uint32_t getLayerCount() const { return mLayerCount; }

    gl::ImageIndex getImageIndexForClear(uint32_t layerCount) const;

    // Special mutator for Surface RenderTargets. Allows the Framebuffer to keep a single
    // RenderTargetVk pointer.
    void updateSwapchainImage(vk::ImageHelper *image,
                              vk::ImageViewHelper *imageViews,
                              vk::ImageHelper *resolveImage,
                              vk::ImageViewHelper *resolveImageViews);

    angle::Result flushStagedUpdates(ContextVk *contextVk,
                                     vk::ClearValuesArray *deferredClears,
                                     uint32_t deferredClearIndex,
                                     uint32_t framebufferLayerCount);

    bool hasDefinedContent() const;
    bool hasDefinedStencilContent() const;
    // Mark content as undefined so that certain optimizations are possible such as using DONT_CARE
    // as loadOp of the render target in the next renderpass.  If |preferToKeepContentsDefinedOut|
    // is set to true, it's preferred to ignore the invalidation due to image format and device
    // architecture properties.
    void invalidateEntireContent(ContextVk *contextVk, bool *preferToKeepContentsDefinedOut);
    void invalidateEntireStencilContent(ContextVk *contextVk, bool *preferToKeepContentsDefinedOut);

    // See the description of mTransience for details of how the following two can interact.
    bool hasResolveAttachment() const { return mResolveImage != nullptr && !isEntirelyTransient(); }
    bool isImageTransient() const { return mTransience != RenderTargetTransience::Default; }
    bool isEntirelyTransient() const
    {
        return mTransience == RenderTargetTransience::EntirelyTransient;
    }

    void onNewFramebuffer(const vk::SharedFramebufferCacheKey &sharedFramebufferCacheKey)
    {
        ASSERT(!mFramebufferCacheManager.containsKey(sharedFramebufferCacheKey));
        mFramebufferCacheManager.addKey(sharedFramebufferCacheKey);
    }
    void release(ContextVk *contextVk) { mFramebufferCacheManager.releaseKeys(contextVk); }
    void destroy(RendererVk *renderer) { mFramebufferCacheManager.destroyKeys(renderer); }

  private:
    void reset();

    angle::Result getImageViewImpl(vk::Context *context,
                                   const vk::ImageHelper &image,
                                   gl::SrgbWriteControlMode mode,
                                   vk::ImageViewHelper *imageViews,
                                   const vk::ImageView **imageViewOut) const;

    vk::ImageOrBufferViewSubresourceSerial getSubresourceSerialImpl(
        vk::ImageViewHelper *imageViews) const;

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

    UniqueSerial mImageSiblingSerial;

    // Which subresource of the image is used as render target.  For single-layer render targets,
    // |mLayerIndex| will contain the layer index and |mLayerCount| will be 1.  For layered render
    // targets, |mLayerIndex| will be 0 and |mLayerCount| will be the number of layers in the image
    // (or level depth, if image is 3D).  Note that blit and other functions that read or write to
    // the render target always use layer 0, so this works out for users of |getLayerIndex()|.
    gl::LevelIndex mLevelIndexGL;
    uint32_t mLayerIndex;
    uint32_t mLayerCount;

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

    // Track references to the cached Framebuffer object that created out of this object
    vk::FramebufferCacheManager mFramebufferCacheManager;
};

// A vector of rendertargets
using RenderTargetVector = std::vector<RenderTargetVk>;
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERTARGETVK_H_
