//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVk.h:
//    Defines the class interface for DisplayVk, implementing DisplayImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_DISPLAYVK_H_
#define LIBANGLE_RENDERER_VULKAN_DISPLAYVK_H_

#include "common/MemoryBuffer.h"
#include "libANGLE/renderer/DisplayImpl.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{
constexpr VkDeviceSize kMaxTotalEmptyBufferBytes = 16 * 1024 * 1024;

class RendererVk;
using ContextVkSet = std::set<ContextVk *>;

class TextureUpload
{
  public:
    TextureUpload() { mPrevUploadedMutableTexture = nullptr; }
    ~TextureUpload() { resetPrevTexture(); }
    angle::Result onMutableTextureUpload(ContextVk *contextVk, TextureVk *newTexture);
    void onTextureRelease(TextureVk *textureVk);
    void resetPrevTexture() { mPrevUploadedMutableTexture = nullptr; }

  private:
    // Keep track of the previously stored texture. Used to flush mutable textures.
    TextureVk *mPrevUploadedMutableTexture;
};

class ShareGroupVk : public ShareGroupImpl
{
  public:
    ShareGroupVk();
    void onDestroy(const egl::Display *display) override;

    FramebufferCache &getFramebufferCache() { return mFramebufferCache; }

    // PipelineLayoutCache and DescriptorSetLayoutCache can be shared between multiple threads
    // accessing them via shared contexts. The ShareGroup locks around gl entrypoints ensuring
    // synchronous update to the caches.
    PipelineLayoutCache &getPipelineLayoutCache() { return mPipelineLayoutCache; }
    DescriptorSetLayoutCache &getDescriptorSetLayoutCache() { return mDescriptorSetLayoutCache; }
    const ContextVkSet &getContexts() const { return mContexts; }
    vk::MetaDescriptorPool &getMetaDescriptorPool(DescriptorSetIndex descriptorSetIndex)
    {
        return mMetaDescriptorPools[descriptorSetIndex];
    }

    void releaseResourceUseLists(const Serial &submitSerial);
    void acquireResourceUseList(vk::ResourceUseList &&resourceUseList)
    {
        mResourceUseLists.emplace_back(std::move(resourceUseList));
    }

    // Used to flush the mutable textures more often.
    angle::Result onMutableTextureUpload(ContextVk *contextVk, TextureVk *newTexture);

    vk::BufferPool *getDefaultBufferPool(RendererVk *renderer,
                                         VkDeviceSize size,
                                         uint32_t memoryTypeIndex);
    void pruneDefaultBufferPools(RendererVk *renderer);
    bool isDueForBufferPoolPrune(RendererVk *renderer);

    void calculateTotalBufferCount(size_t *bufferCount, VkDeviceSize *totalSize) const;
    void logBufferPools() const;

    void addContext(ContextVk *contextVk);
    void removeContext(ContextVk *contextVk);

    void onTextureRelease(TextureVk *textureVk);

  private:
    // VkFramebuffer caches
    FramebufferCache mFramebufferCache;

    void resetPrevTexture() { mTextureUpload.resetPrevTexture(); }

    // ANGLE uses a PipelineLayout cache to store compatible pipeline layouts.
    PipelineLayoutCache mPipelineLayoutCache;

    // DescriptorSetLayouts are also managed in a cache.
    DescriptorSetLayoutCache mDescriptorSetLayoutCache;

    // Descriptor set caches
    vk::DescriptorSetArray<vk::MetaDescriptorPool> mMetaDescriptorPools;

    // The list of contexts within the share group
    ContextVkSet mContexts;

    // List of resources currently used that need to be freed when any ContextVk in this
    // ShareGroupVk submits the next command.
    std::vector<vk::ResourceUseList> mResourceUseLists;

    // The per shared group buffer pools that all buffers should sub-allocate from.
    vk::BufferPoolPointerArray mDefaultBufferPools;

    // The pool dedicated for small allocations that uses faster buddy algorithm
    std::unique_ptr<vk::BufferPool> mSmallBufferPool;
    static constexpr VkDeviceSize kMaxSizeToUseSmallBufferPool = 256;

    // The system time when last pruneEmptyBuffer gets called.
    double mLastPruneTime;

    // Texture update manager used to flush uploaded mutable textures.
    TextureUpload mTextureUpload;

    // If true, it is expected that a BufferBlock may still in used by textures that outlived
    // ShareGroup. The non-empty BufferBlock will be put into RendererVk's orphan list instead.
    bool mOrphanNonEmptyBufferBlock;
};

class DisplayVk : public DisplayImpl, public vk::Context
{
  public:
    DisplayVk(const egl::DisplayState &state);
    ~DisplayVk() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    egl::Error makeCurrent(egl::Display *display,
                           egl::Surface *drawSurface,
                           egl::Surface *readSurface,
                           gl::Context *context) override;

    bool testDeviceLost() override;
    egl::Error restoreLostDevice(const egl::Display *display) override;

    std::string getRendererDescription() override;
    std::string getVendorString() override;
    std::string getVersionString(bool includeFullVersion) override;

    DeviceImpl *createDevice() override;

    egl::Error waitClient(const gl::Context *context) override;
    egl::Error waitNative(const gl::Context *context, EGLint engine) override;

    SurfaceImpl *createWindowSurface(const egl::SurfaceState &state,
                                     EGLNativeWindowType window,
                                     const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferSurface(const egl::SurfaceState &state,
                                      const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                               EGLenum buftype,
                                               EGLClientBuffer clientBuffer,
                                               const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPixmapSurface(const egl::SurfaceState &state,
                                     NativePixmapType nativePixmap,
                                     const egl::AttributeMap &attribs) override;

    ImageImpl *createImage(const egl::ImageState &state,
                           const gl::Context *context,
                           EGLenum target,
                           const egl::AttributeMap &attribs) override;

    ContextImpl *createContext(const gl::State &state,
                               gl::ErrorSet *errorSet,
                               const egl::Config *configuration,
                               const gl::Context *shareContext,
                               const egl::AttributeMap &attribs) override;

    StreamProducerImpl *createStreamProducerD3DTexture(egl::Stream::ConsumerType consumerType,
                                                       const egl::AttributeMap &attribs) override;

    EGLSyncImpl *createSync(const egl::AttributeMap &attribs) override;

    gl::Version getMaxSupportedESVersion() const override;
    gl::Version getMaxConformantESVersion() const override;
    Optional<gl::Version> getMaxSupportedDesktopVersion() const override;

    egl::Error validateImageClientBuffer(const gl::Context *context,
                                         EGLenum target,
                                         EGLClientBuffer clientBuffer,
                                         const egl::AttributeMap &attribs) const override;
    ExternalImageSiblingImpl *createExternalImageSibling(const gl::Context *context,
                                                         EGLenum target,
                                                         EGLClientBuffer buffer,
                                                         const egl::AttributeMap &attribs) override;
    virtual const char *getWSIExtension() const = 0;
    virtual const char *getWSILayer() const;
    virtual bool isUsingSwapchain() const;

    // Determine if a config with given formats and sample counts is supported.  This callback may
    // modify the config to add or remove platform specific attributes such as nativeVisualID.  If
    // the config is not supported by the window system, it removes the EGL_WINDOW_BIT from
    // surfaceType, which would still allow the config to be used for pbuffers.
    virtual void checkConfigSupport(egl::Config *config) = 0;

    [[nodiscard]] bool getScratchBuffer(size_t requestedSizeBytes,
                                        angle::MemoryBuffer **scratchBufferOut) const;
    angle::ScratchBuffer *getScratchBuffer() const { return &mScratchBuffer; }

    void handleError(VkResult result,
                     const char *file,
                     const char *function,
                     unsigned int line) override;

    // TODO(jmadill): Remove this once refactor is done. http://anglebug.com/3041
    egl::Error getEGLError(EGLint errorCode);

    void initializeFrontendFeatures(angle::FrontendFeatures *features) const override;

    void populateFeatureList(angle::FeatureList *features) override;

    ShareGroupImpl *createShareGroup() override;

    bool isConfigFormatSupported(VkFormat format) const;
    bool isSurfaceFormatColorspacePairSupported(VkSurfaceKHR surface,
                                                VkFormat format,
                                                VkColorSpaceKHR colorspace) const;

  protected:
    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;

  private:
    virtual SurfaceImpl *createWindowSurfaceVk(const egl::SurfaceState &state,
                                               EGLNativeWindowType window) = 0;
    void generateCaps(egl::Caps *outCaps) const override;

    virtual angle::Result waitNativeImpl();

    bool isColorspaceSupported(VkColorSpaceKHR colorspace) const;
    void initSupportedSurfaceFormatColorspaces();

    mutable angle::ScratchBuffer mScratchBuffer;

    vk::Error mSavedError;

    // Map of supported colorspace and associated surface format set.
    angle::HashMap<VkColorSpaceKHR, std::unordered_set<VkFormat>> mSupportedColorspaceFormatsMap;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_DISPLAYVK_H_
