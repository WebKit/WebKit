//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureVk.h:
//    Defines the class interface for TextureVk, implementing TextureImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_TEXTUREVK_H_
#define LIBANGLE_RENDERER_VULKAN_TEXTUREVK_H_

#include "libANGLE/renderer/TextureImpl.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/SamplerVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{

enum class ImageMipLevels
{
    EnabledLevels = 0,
    FullMipChain  = 1,

    InvalidEnum = 2,
};

// vkCmdCopyBufferToImage buffer offset multiple
constexpr VkDeviceSize kBufferOffsetMultiple = 4;

class TextureVk : public TextureImpl, public angle::ObserverInterface
{
  public:
    TextureVk(const gl::TextureState &state, RendererVk *renderer);
    ~TextureVk() override;
    void onDestroy(const gl::Context *context) override;

    angle::Result setImage(const gl::Context *context,
                           const gl::ImageIndex &index,
                           GLenum internalFormat,
                           const gl::Extents &size,
                           GLenum format,
                           GLenum type,
                           const gl::PixelUnpackState &unpack,
                           gl::Buffer *unpackBuffer,
                           const uint8_t *pixels) override;
    angle::Result setSubImage(const gl::Context *context,
                              const gl::ImageIndex &index,
                              const gl::Box &area,
                              GLenum format,
                              GLenum type,
                              const gl::PixelUnpackState &unpack,
                              gl::Buffer *unpackBuffer,
                              const uint8_t *pixels) override;

    angle::Result setCompressedImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     GLenum internalFormat,
                                     const gl::Extents &size,
                                     const gl::PixelUnpackState &unpack,
                                     size_t imageSize,
                                     const uint8_t *pixels) override;
    angle::Result setCompressedSubImage(const gl::Context *context,
                                        const gl::ImageIndex &index,
                                        const gl::Box &area,
                                        GLenum format,
                                        const gl::PixelUnpackState &unpack,
                                        size_t imageSize,
                                        const uint8_t *pixels) override;

    angle::Result copyImage(const gl::Context *context,
                            const gl::ImageIndex &index,
                            const gl::Rectangle &sourceArea,
                            GLenum internalFormat,
                            gl::Framebuffer *source) override;
    angle::Result copySubImage(const gl::Context *context,
                               const gl::ImageIndex &index,
                               const gl::Offset &destOffset,
                               const gl::Rectangle &sourceArea,
                               gl::Framebuffer *source) override;

    angle::Result copyTexture(const gl::Context *context,
                              const gl::ImageIndex &index,
                              GLenum internalFormat,
                              GLenum type,
                              GLint sourceLevelGL,
                              bool unpackFlipY,
                              bool unpackPremultiplyAlpha,
                              bool unpackUnmultiplyAlpha,
                              const gl::Texture *source) override;
    angle::Result copySubTexture(const gl::Context *context,
                                 const gl::ImageIndex &index,
                                 const gl::Offset &destOffset,
                                 GLint sourceLevelGL,
                                 const gl::Box &sourceBox,
                                 bool unpackFlipY,
                                 bool unpackPremultiplyAlpha,
                                 bool unpackUnmultiplyAlpha,
                                 const gl::Texture *source) override;

    angle::Result copyRenderbufferSubData(const gl::Context *context,
                                          const gl::Renderbuffer *srcBuffer,
                                          GLint srcLevel,
                                          GLint srcX,
                                          GLint srcY,
                                          GLint srcZ,
                                          GLint dstLevel,
                                          GLint dstX,
                                          GLint dstY,
                                          GLint dstZ,
                                          GLsizei srcWidth,
                                          GLsizei srcHeight,
                                          GLsizei srcDepth) override;

    angle::Result copyTextureSubData(const gl::Context *context,
                                     const gl::Texture *srcTexture,
                                     GLint srcLevel,
                                     GLint srcX,
                                     GLint srcY,
                                     GLint srcZ,
                                     GLint dstLevel,
                                     GLint dstX,
                                     GLint dstY,
                                     GLint dstZ,
                                     GLsizei srcWidth,
                                     GLsizei srcHeight,
                                     GLsizei srcDepth) override;

    angle::Result copyCompressedTexture(const gl::Context *context,
                                        const gl::Texture *source) override;

    angle::Result setStorage(const gl::Context *context,
                             gl::TextureType type,
                             size_t levels,
                             GLenum internalFormat,
                             const gl::Extents &size) override;

    angle::Result setStorageExternalMemory(const gl::Context *context,
                                           gl::TextureType type,
                                           size_t levels,
                                           GLenum internalFormat,
                                           const gl::Extents &size,
                                           gl::MemoryObject *memoryObject,
                                           GLuint64 offset,
                                           GLbitfield createFlags,
                                           GLbitfield usageFlags) override;

    angle::Result setEGLImageTarget(const gl::Context *context,
                                    gl::TextureType type,
                                    egl::Image *image) override;

    angle::Result setImageExternal(const gl::Context *context,
                                   gl::TextureType type,
                                   egl::Stream *stream,
                                   const egl::Stream::GLTextureDescription &desc) override;

    angle::Result generateMipmap(const gl::Context *context) override;

    angle::Result setBaseLevel(const gl::Context *context, GLuint baseLevel) override;

    angle::Result bindTexImage(const gl::Context *context, egl::Surface *surface) override;
    angle::Result releaseTexImage(const gl::Context *context) override;

    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            GLsizei samples,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

    angle::Result syncState(const gl::Context *context,
                            const gl::Texture::DirtyBits &dirtyBits,
                            gl::Command source) override;

    angle::Result setStorageMultisample(const gl::Context *context,
                                        gl::TextureType type,
                                        GLsizei samples,
                                        GLint internalformat,
                                        const gl::Extents &size,
                                        bool fixedSampleLocations) override;

    angle::Result initializeContents(const gl::Context *context,
                                     const gl::ImageIndex &imageIndex) override;

    const vk::ImageHelper &getImage() const
    {
        ASSERT(mImage && mImage->valid());
        return *mImage;
    }

    vk::ImageHelper &getImage()
    {
        ASSERT(mImage && mImage->valid());
        return *mImage;
    }

    void retainImageViews(vk::ResourceUseList *resourceUseList)
    {
        getImageViews().retain(resourceUseList);
    }

    void releaseOwnershipOfImage(const gl::Context *context);

    const vk::ImageView &getReadImageViewAndRecordUse(ContextVk *contextVk,
                                                      GLenum srgbDecode,
                                                      bool texelFetchStaticUse) const;

    // A special view for cube maps as a 2D array, used with shaders that do texelFetch() and for
    // seamful cube map emulation.
    const vk::ImageView &getFetchImageViewAndRecordUse(ContextVk *contextVk,
                                                       GLenum srgbDecode,
                                                       bool texelFetchStaticUse) const;

    // A special view used for texture copies that shouldn't perform swizzle.
    const vk::ImageView &getCopyImageViewAndRecordUse(ContextVk *contextVk) const;
    angle::Result getStorageImageView(ContextVk *contextVk,
                                      const gl::ImageUnit &binding,
                                      const vk::ImageView **imageViewOut);

    const vk::SamplerHelper &getSampler() const
    {
        ASSERT(mSampler.valid());
        return mSampler.get();
    }

    // Normally, initialize the image with enabled mipmap level counts.
    angle::Result ensureImageInitialized(ContextVk *contextVk, ImageMipLevels mipLevels);

    vk::ImageViewSubresourceSerial getImageViewSubresourceSerial(
        const gl::SamplerState &samplerState) const;

    void overrideStagingBufferSizeForTesting(size_t initialSizeForTesting)
    {
        mStagingBufferInitialSize = initialSizeForTesting;
    }

    GLenum getColorReadFormat(const gl::Context *context) override;
    GLenum getColorReadType(const gl::Context *context) override;

    angle::Result getTexImage(const gl::Context *context,
                              const gl::PixelPackState &packState,
                              gl::Buffer *packBuffer,
                              gl::TextureTarget target,
                              GLint level,
                              GLenum format,
                              GLenum type,
                              void *pixels) override;

    ANGLE_INLINE bool hasBeenBoundAsImage() const { return mState.hasBeenBoundAsImage(); }

    bool isSRGBOverrideEnabled() const
    {
        return mState.getSRGBOverride() != gl::SrgbOverride::Default;
    }

    angle::Result ensureMutable(ContextVk *contextVk);

  private:
    // Transform an image index from the frontend into one that can be used on the backing
    // ImageHelper, taking into account mipmap or cube face offsets
    gl::ImageIndex getNativeImageIndex(const gl::ImageIndex &inputImageIndex) const;
    gl::LevelIndex getNativeImageLevel(gl::LevelIndex frontendLevel) const;
    uint32_t getNativeImageLayer(uint32_t frontendLayer) const;

    void releaseAndDeleteImage(ContextVk *contextVk);
    angle::Result ensureImageAllocated(ContextVk *contextVk, const vk::Format &format);
    void setImageHelper(ContextVk *contextVk,
                        vk::ImageHelper *imageHelper,
                        gl::TextureType imageType,
                        const vk::Format &format,
                        uint32_t imageLevelOffset,
                        uint32_t imageLayerOffset,
                        gl::LevelIndex imageBaseLevel,
                        bool selfOwned);
    void updateImageHelper(ContextVk *contextVk, size_t imageCopyBufferAlignment);
    vk::ImageViewHelper &getImageViews()
    {
        return mMultisampledImageViews[gl::RenderToTextureImageIndex::Default];
    }
    const vk::ImageViewHelper &getImageViews() const
    {
        return mMultisampledImageViews[gl::RenderToTextureImageIndex::Default];
    }

    // Redefine a mip level of the texture.  If the new size and format don't match the allocated
    // image, the image may be released.  When redefining a mip of a multi-level image, updates are
    // forced to be staged, as another mip of the image may be bound to a framebuffer.  For example,
    // assume texture has two mips, and framebuffer is bound to mip 0.  Redefining mip 1 to an
    // incompatible size shouldn't affect the framebuffer, especially if the redefinition comes from
    // something like glCopyTexSubImage2D() (which simultaneously is reading from said framebuffer,
    // i.e. mip 0 of the texture).
    angle::Result redefineLevel(const gl::Context *context,
                                const gl::ImageIndex &index,
                                const vk::Format &format,
                                const gl::Extents &size);

    angle::Result setImageImpl(const gl::Context *context,
                               const gl::ImageIndex &index,
                               const gl::InternalFormat &formatInfo,
                               const gl::Extents &size,
                               GLenum type,
                               const gl::PixelUnpackState &unpack,
                               gl::Buffer *unpackBuffer,
                               const uint8_t *pixels);
    angle::Result setSubImageImpl(const gl::Context *context,
                                  const gl::ImageIndex &index,
                                  const gl::Box &area,
                                  const gl::InternalFormat &formatInfo,
                                  GLenum type,
                                  const gl::PixelUnpackState &unpack,
                                  gl::Buffer *unpackBuffer,
                                  const uint8_t *pixels,
                                  const vk::Format &vkFormat);

    angle::Result copyImageDataToBufferAndGetData(ContextVk *contextVk,
                                                  gl::LevelIndex sourceLevelGL,
                                                  uint32_t layerCount,
                                                  const gl::Box &sourceArea,
                                                  uint8_t **outDataPtr);

    angle::Result copyBufferDataToImage(ContextVk *contextVk,
                                        vk::BufferHelper *srcBuffer,
                                        const gl::ImageIndex index,
                                        uint32_t rowLength,
                                        uint32_t imageHeight,
                                        const gl::Box &sourceArea,
                                        size_t offset,
                                        VkImageAspectFlags aspectFlags);

    // Called from syncState to prepare the image for mipmap generation.
    void prepareForGenerateMipmap(ContextVk *contextVk);

    // Generate mipmaps from level 0 into the rest of the mips.  This requires the image to have
    // STORAGE usage.
    angle::Result generateMipmapsWithCompute(ContextVk *contextVk);

    angle::Result generateMipmapsWithCPU(const gl::Context *context);

    angle::Result generateMipmapLevelsWithCPU(ContextVk *contextVk,
                                              const angle::Format &sourceFormat,
                                              GLuint layer,
                                              gl::LevelIndex firstMipLevel,
                                              gl::LevelIndex maxMipLevel,
                                              const size_t sourceWidth,
                                              const size_t sourceHeight,
                                              const size_t sourceDepth,
                                              const size_t sourceRowPitch,
                                              const size_t sourceDepthPitch,
                                              uint8_t *sourceData);

    angle::Result copySubImageImpl(const gl::Context *context,
                                   const gl::ImageIndex &index,
                                   const gl::Offset &destOffset,
                                   const gl::Rectangle &sourceArea,
                                   const gl::InternalFormat &internalFormat,
                                   gl::Framebuffer *source);

    angle::Result copySubTextureImpl(ContextVk *contextVk,
                                     const gl::ImageIndex &index,
                                     const gl::Offset &destOffset,
                                     const gl::InternalFormat &destFormat,
                                     gl::LevelIndex sourceLevelGL,
                                     const gl::Box &sourceBox,
                                     bool unpackFlipY,
                                     bool unpackPremultiplyAlpha,
                                     bool unpackUnmultiplyAlpha,
                                     TextureVk *source);

    angle::Result copySubImageImplWithTransfer(ContextVk *contextVk,
                                               const gl::ImageIndex &index,
                                               const gl::Offset &destOffset,
                                               const vk::Format &destFormat,
                                               gl::LevelIndex sourceLevelGL,
                                               size_t sourceLayer,
                                               const gl::Box &sourceBox,
                                               vk::ImageHelper *srcImage);

    angle::Result copySubImageImplWithDraw(ContextVk *contextVk,
                                           const gl::ImageIndex &index,
                                           const gl::Offset &destOffset,
                                           const vk::Format &destFormat,
                                           gl::LevelIndex sourceLevelGL,
                                           const gl::Box &sourceBox,
                                           bool isSrcFlipY,
                                           bool unpackFlipY,
                                           bool unpackPremultiplyAlpha,
                                           bool unpackUnmultiplyAlpha,
                                           vk::ImageHelper *srcImage,
                                           const vk::ImageView *srcView,
                                           SurfaceRotation srcFramebufferRotation);

    angle::Result initImage(ContextVk *contextVk,
                            const vk::Format &format,
                            const bool sized,
                            const gl::Extents &extents,
                            const uint32_t levelCount);
    void releaseImage(ContextVk *contextVk);
    void releaseStagingBuffer(ContextVk *contextVk);
    uint32_t getMipLevelCount(ImageMipLevels mipLevels) const;
    uint32_t getMaxLevelCount() const;
    angle::Result copyAndStageImageData(ContextVk *contextVk,
                                        gl::LevelIndex previousBaseLevel,
                                        vk::ImageHelper *srcImage,
                                        vk::ImageHelper *dstImage);
    angle::Result initImageViews(ContextVk *contextVk,
                                 const vk::Format &format,
                                 const bool sized,
                                 uint32_t levelCount,
                                 uint32_t layerCount);
    angle::Result initRenderTargets(ContextVk *contextVk,
                                    GLuint layerCount,
                                    gl::LevelIndex levelIndexGL,
                                    gl::RenderToTextureImageIndex renderToTextureIndex);
    angle::Result getLevelLayerImageView(ContextVk *contextVk,
                                         gl::LevelIndex levelGL,
                                         size_t layer,
                                         const vk::ImageView **imageViewOut);

    // Flush image's staged updates for all levels and layers.
    angle::Result flushImageStagedUpdates(ContextVk *contextVk);

    const gl::InternalFormat &getImplementationSizedFormat(const gl::Context *context) const;
    const vk::Format &getBaseLevelFormat(RendererVk *renderer) const;
    // Queues a flush of any modified image attributes. The image will be reallocated with its new
    // attributes at the next opportunity.
    angle::Result respecifyImageStorage(ContextVk *contextVk);
    angle::Result respecifyImageStorageAndLevels(ContextVk *contextVk,
                                                 gl::LevelIndex previousBaseLevelGL,
                                                 gl::LevelIndex baseLevelGL,
                                                 gl::LevelIndex maxLevelGL);

    // Update base and max levels, and re-create image if needed.
    angle::Result updateBaseMaxLevels(ContextVk *contextVk,
                                      gl::LevelIndex baseLevelGL,
                                      gl::LevelIndex maxLevelGL);

    bool isFastUnpackPossible(const vk::Format &vkFormat, size_t offset) const;

    bool shouldUpdateBeStaged(gl::LevelIndex textureLevelIndexGL) const;

    // We monitor the staging buffer and set dirty bits if the staging buffer changes. Note that we
    // support changes in the staging buffer even outside the TextureVk class.
    void onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message) override;

    ANGLE_INLINE VkImageTiling getTilingMode()
    {
        return (mImage->valid()) ? mImage->getTilingMode() : VK_IMAGE_TILING_OPTIMAL;
    }

    angle::Result refreshImageViews(ContextVk *contextVk);
    bool shouldDecodeSRGB(ContextVk *contextVk, GLenum srgbDecode, bool texelFetchStaticUse) const;
    void initImageUsageFlags(ContextVk *contextVk, const vk::Format &format);

    bool mOwnsImage;
    bool mRequiresMutableStorage;

    gl::TextureType mImageNativeType;

    // The layer offset to apply when converting from a frontend texture layer to a texture layer in
    // mImage. Used when this texture sources a cube map face or 3D texture layer from an EGL image.
    uint32_t mImageLayerOffset;

    // The level offset to apply when converting from a frontend texture level to texture level in
    // mImage.
    uint32_t mImageLevelOffset;

    // If multisampled rendering to texture, an intermediate multisampled image is created for use
    // as renderpass color attachment.  An array of images and image views are used based on the
    // number of samples used with multisampled rendering to texture.  Index 0 corresponds to the
    // non-multisampled-render-to-texture usage of the texture.

    // - index 0: Unused.  See description of |mImage|.
    // - index N: intermediate multisampled image used for multisampled rendering to texture with
    //   1 << N samples
    gl::RenderToTextureImageMap<vk::ImageHelper> mMultisampledImages;

    // |ImageViewHelper| contains all the current views for the Texture. The views are always owned
    // by the Texture and are not shared like |mImage|. They also have different lifetimes and can
    // be reallocated independently of |mImage| on state changes.
    //
    // - index 0: views for the texture's image (regardless of |mOwnsImage|).
    // - index N: views for mMultisampledImages[N]
    gl::RenderToTextureImageMap<vk::ImageViewHelper> mMultisampledImageViews;

    // Render targets stored as array of vector of vectors
    //
    // - First dimension: index N contains render targets with views from mMultisampledImageViews[N]
    // - Second dimension: level
    // - Third dimension: layer
    gl::RenderToTextureImageMap<std::vector<RenderTargetVector>> mRenderTargets;

    // |mImage| wraps a VkImage and VkDeviceMemory that represents the gl::Texture. |mOwnsImage|
    // indicates that |TextureVk| owns the image. Otherwise it is a weak pointer shared with another
    // class. Due to this sharing, for example through EGL images, the image must always be
    // dynamically allocated as the texture can release ownership for example and it can be
    // transferred to another |TextureVk|.
    vk::ImageHelper *mImage;

    // |mSampler| contains the relevant Vulkan sampler states representing the OpenGL Texture
    // sampling states for the Texture.
    vk::SamplerBinding mSampler;

    // Overridden in some tests.
    size_t mStagingBufferInitialSize;

    // The created vkImage usage flag.
    VkImageUsageFlags mImageUsageFlags;

    // Additional image create flags
    VkImageCreateFlags mImageCreateFlags;

    // If an image level is incompatibly redefined, the image lives through the call that did this
    // (i.e. set and copy levels), because the image may be used by the framebuffer in the very same
    // call.  As a result, updates to this redefined level are staged (in both the call that
    // redefines it, and any future calls such as subimage updates).  This bitset flags redefined
    // levels so that their updates will be force-staged until image is recreated.
    //
    // In common cases with mipmapped textures, the base/max level would need adjusting as the
    // texture is no longer mip-complete.  However, if every level is redefined such that at the end
    // the image becomes mip-complete again, no reinitialization of the image is done.  This bitset
    // is additionally used to ensure the image is recreated in the next syncState, if not already.
    gl::TexLevelMask mRedefinedLevels;

    angle::ObserverBinding mImageObserverBinding;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_TEXTUREVK_H_
