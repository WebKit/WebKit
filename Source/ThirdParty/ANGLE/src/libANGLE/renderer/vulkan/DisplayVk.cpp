//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVk.cpp:
//    Implements the class methods for DisplayVk.
//

#include "libANGLE/renderer/vulkan/DisplayVk.h"

#include "common/debug.h"
#include "common/system_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DeviceVk.h"
#include "libANGLE/renderer/vulkan/ImageVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"
#include "libANGLE/renderer/vulkan/SyncVk.h"
#include "libANGLE/renderer/vulkan/VkImageImageSiblingVk.h"
#include "libANGLE/trace.h"

namespace rx
{
// Time interval in seconds that we should try to prune default buffer pools.
constexpr double kTimeElapsedForPruneDefaultBufferPool = 0.25;

DisplayVk::DisplayVk(const egl::DisplayState &state)
    : DisplayImpl(state),
      vk::Context(new RendererVk()),
      mScratchBuffer(1000u),
      mSavedError({VK_SUCCESS, "", "", 0})
{}

DisplayVk::~DisplayVk()
{
    delete mRenderer;
}

egl::Error DisplayVk::initialize(egl::Display *display)
{
    ASSERT(mRenderer != nullptr && display != nullptr);
    angle::Result result = mRenderer->initialize(this, display, getWSIExtension(), getWSILayer());
    ANGLE_TRY(angle::ToEGL(result, this, EGL_NOT_INITIALIZED));
    return egl::NoError();
}

void DisplayVk::terminate()
{
    mRenderer->reloadVolkIfNeeded();

    ASSERT(mRenderer);
    mRenderer->onDestroy(this);
}

egl::Error DisplayVk::makeCurrent(egl::Display * /*display*/,
                                  egl::Surface * /*drawSurface*/,
                                  egl::Surface * /*readSurface*/,
                                  gl::Context * /*context*/)
{
    // Ensure the appropriate global DebugAnnotator is used
    ASSERT(mRenderer);
    mRenderer->setGlobalDebugAnnotator();

    return egl::NoError();
}

bool DisplayVk::testDeviceLost()
{
    return mRenderer->isDeviceLost();
}

egl::Error DisplayVk::restoreLostDevice(const egl::Display *display)
{
    // A vulkan device cannot be restored, the entire renderer would have to be re-created along
    // with any other EGL objects that reference it.
    return egl::EglBadDisplay();
}

std::string DisplayVk::getRendererDescription()
{
    if (mRenderer)
    {
        return mRenderer->getRendererDescription();
    }
    return std::string();
}

std::string DisplayVk::getVendorString()
{
    if (mRenderer)
    {
        return mRenderer->getVendorString();
    }
    return std::string();
}

std::string DisplayVk::getVersionString(bool includeFullVersion)
{
    if (mRenderer)
    {
        return mRenderer->getVersionString(includeFullVersion);
    }
    return std::string();
}

DeviceImpl *DisplayVk::createDevice()
{
    return new DeviceVk();
}

egl::Error DisplayVk::waitClient(const gl::Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "DisplayVk::waitClient");
    ContextVk *contextVk = vk::GetImpl(context);
    return angle::ToEGL(contextVk->finishImpl(RenderPassClosureReason::EGLWaitClient), this,
                        EGL_BAD_ACCESS);
}

egl::Error DisplayVk::waitNative(const gl::Context *context, EGLint engine)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "DisplayVk::waitNative");
    return angle::ResultToEGL(waitNativeImpl());
}

angle::Result DisplayVk::waitNativeImpl()
{
    return angle::Result::Continue;
}

SurfaceImpl *DisplayVk::createWindowSurface(const egl::SurfaceState &state,
                                            EGLNativeWindowType window,
                                            const egl::AttributeMap &attribs)
{
    return createWindowSurfaceVk(state, window);
}

SurfaceImpl *DisplayVk::createPbufferSurface(const egl::SurfaceState &state,
                                             const egl::AttributeMap &attribs)
{
    ASSERT(mRenderer);
    return new OffscreenSurfaceVk(state, mRenderer);
}

SurfaceImpl *DisplayVk::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                      EGLenum buftype,
                                                      EGLClientBuffer clientBuffer,
                                                      const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

SurfaceImpl *DisplayVk::createPixmapSurface(const egl::SurfaceState &state,
                                            NativePixmapType nativePixmap,
                                            const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

ImageImpl *DisplayVk::createImage(const egl::ImageState &state,
                                  const gl::Context *context,
                                  EGLenum target,
                                  const egl::AttributeMap &attribs)
{
    return new ImageVk(state, context);
}

ShareGroupImpl *DisplayVk::createShareGroup()
{
    return new ShareGroupVk();
}

ContextImpl *DisplayVk::createContext(const gl::State &state,
                                      gl::ErrorSet *errorSet,
                                      const egl::Config *configuration,
                                      const gl::Context *shareContext,
                                      const egl::AttributeMap &attribs)
{
    return new ContextVk(state, errorSet, mRenderer);
}

StreamProducerImpl *DisplayVk::createStreamProducerD3DTexture(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<StreamProducerImpl *>(0);
}

EGLSyncImpl *DisplayVk::createSync(const egl::AttributeMap &attribs)
{
    return new EGLSyncVk(attribs);
}

gl::Version DisplayVk::getMaxSupportedESVersion() const
{
    return mRenderer->getMaxSupportedESVersion();
}

gl::Version DisplayVk::getMaxConformantESVersion() const
{
    return mRenderer->getMaxConformantESVersion();
}

egl::Error DisplayVk::validateImageClientBuffer(const gl::Context *context,
                                                EGLenum target,
                                                EGLClientBuffer clientBuffer,
                                                const egl::AttributeMap &attribs) const
{
    switch (target)
    {
        case EGL_VULKAN_IMAGE_ANGLE:
        {
            VkImage *vkImage = reinterpret_cast<VkImage *>(clientBuffer);
            if (!vkImage || *vkImage == VK_NULL_HANDLE)
            {
                return egl::EglBadParameter() << "clientBuffer is invalid.";
            }

            GLenum internalFormat =
                static_cast<GLenum>(attribs.get(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_NONE));
            switch (internalFormat)
            {
                case GL_RGBA:
                case GL_BGRA_EXT:
                case GL_RGB:
                case GL_RED_EXT:
                case GL_RG_EXT:
                case GL_RGB10_A2_EXT:
                case GL_R16_EXT:
                case GL_RG16_EXT:
                case GL_NONE:
                    break;
                default:
                    return egl::EglBadParameter() << "Invalid EGLImage texture internal format: 0x"
                                                  << std::hex << internalFormat;
            }

            uint64_t hi = static_cast<uint64_t>(attribs.get(EGL_VULKAN_IMAGE_CREATE_INFO_HI_ANGLE));
            uint64_t lo = static_cast<uint64_t>(attribs.get(EGL_VULKAN_IMAGE_CREATE_INFO_LO_ANGLE));
            uint64_t info = ((hi & 0xffffffff) << 32) | (lo & 0xffffffff);
            if (reinterpret_cast<const VkImageCreateInfo *>(info)->sType !=
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO)
            {
                return egl::EglBadParameter()
                       << "EGL_VULKAN_IMAGE_CREATE_INFO_HI_ANGLE and "
                          "EGL_VULKAN_IMAGE_CREATE_INFO_LO_ANGLE are not pointing to a "
                          "valid VkImageCreateInfo structure.";
            }

            return egl::NoError();
        }
        default:
            return DisplayImpl::validateImageClientBuffer(context, target, clientBuffer, attribs);
    }
}

ExternalImageSiblingImpl *DisplayVk::createExternalImageSibling(const gl::Context *context,
                                                                EGLenum target,
                                                                EGLClientBuffer buffer,
                                                                const egl::AttributeMap &attribs)
{
    switch (target)
    {
        case EGL_VULKAN_IMAGE_ANGLE:
            return new VkImageImageSiblingVk(buffer, attribs);
        default:
            return DisplayImpl::createExternalImageSibling(context, target, buffer, attribs);
    }
}

void DisplayVk::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->createContextRobustness    = getRenderer()->getNativeExtensions().robustnessEXT;
    outExtensions->surfaceOrientation         = true;
    outExtensions->displayTextureShareGroup   = true;
    outExtensions->displaySemaphoreShareGroup = true;
    outExtensions->robustResourceInitializationANGLE = true;

    // The Vulkan implementation will always say that EGL_KHR_swap_buffers_with_damage is supported.
    // When the Vulkan driver supports VK_KHR_incremental_present, it will use it.  Otherwise, it
    // will ignore the hint and do a regular swap.
    outExtensions->swapBuffersWithDamage = true;

    outExtensions->fenceSync = true;
    outExtensions->waitSync  = true;

    outExtensions->image                 = true;
    outExtensions->imageBase             = true;
    outExtensions->imagePixmap           = false;  // ANGLE does not support pixmaps
    outExtensions->glTexture2DImage      = true;
    outExtensions->glTextureCubemapImage = true;
    outExtensions->glTexture3DImage      = false;
    outExtensions->glRenderbufferImage   = true;
    outExtensions->imageNativeBuffer =
        getRenderer()->getFeatures().supportsAndroidHardwareBuffer.enabled;
    outExtensions->surfacelessContext = true;
    outExtensions->glColorspace       = true;
    outExtensions->imageGlColorspace =
        outExtensions->glColorspace && getRenderer()->getFeatures().supportsImageFormatList.enabled;

#if defined(ANGLE_PLATFORM_ANDROID)
    outExtensions->getNativeClientBufferANDROID = true;
    outExtensions->framebufferTargetANDROID     = true;
#endif  // defined(ANGLE_PLATFORM_ANDROID)

    // EGL_EXT_image_dma_buf_import is only exposed if EGL_EXT_image_dma_buf_import_modifiers can
    // also be exposed.  The Vulkan extensions that support these EGL extensions are not split in
    // the same way; both Vulkan extensions are needed for EGL_EXT_image_dma_buf_import, and with
    // both Vulkan extensions, EGL_EXT_image_dma_buf_import_modifiers is also supportable.
    outExtensions->imageDmaBufImportEXT =
        getRenderer()->getFeatures().supportsExternalMemoryDmaBufAndModifiers.enabled;
    outExtensions->imageDmaBufImportModifiersEXT = outExtensions->imageDmaBufImportEXT;

    // Disable context priority when non-zero memory init is enabled. This enforces a queue order.
    outExtensions->contextPriority = !getRenderer()->getFeatures().allocateNonZeroMemory.enabled;
    outExtensions->noConfigContext = true;

#if defined(ANGLE_PLATFORM_ANDROID)
    outExtensions->nativeFenceSyncANDROID =
        getRenderer()->getFeatures().supportsAndroidNativeFenceSync.enabled;
#endif  // defined(ANGLE_PLATFORM_ANDROID)

#if defined(ANGLE_PLATFORM_GGP)
    outExtensions->ggpStreamDescriptor = true;
    outExtensions->swapWithFrameToken  = getRenderer()->getFeatures().supportsGGPFrameToken.enabled;
#endif  // defined(ANGLE_PLATFORM_GGP)

    outExtensions->bufferAgeEXT = true;

    outExtensions->protectedContentEXT =
        (getRenderer()->getFeatures().supportsProtectedMemory.enabled &&
         getRenderer()->getFeatures().supportsSurfaceProtectedSwapchains.enabled);

    outExtensions->createSurfaceSwapIntervalANGLE = true;

    outExtensions->mutableRenderBufferKHR =
        getRenderer()->getFeatures().supportsSharedPresentableImageExtension.enabled;

    outExtensions->vulkanImageANGLE = true;

    outExtensions->lockSurface3KHR =
        getRenderer()->getFeatures().supportsLockSurfaceExtension.enabled;

    outExtensions->partialUpdateKHR = true;
}

void DisplayVk::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;
    outCaps->stencil8    = getRenderer()->getNativeExtensions().textureStencil8OES;
}

const char *DisplayVk::getWSILayer() const
{
    return nullptr;
}

bool DisplayVk::isUsingSwapchain() const
{
    return true;
}

bool DisplayVk::getScratchBuffer(size_t requstedSizeBytes,
                                 angle::MemoryBuffer **scratchBufferOut) const
{
    return mScratchBuffer.get(requstedSizeBytes, scratchBufferOut);
}

void DisplayVk::handleError(VkResult result,
                            const char *file,
                            const char *function,
                            unsigned int line)
{
    ASSERT(result != VK_SUCCESS);

    mSavedError.errorCode = result;
    mSavedError.file      = file;
    mSavedError.function  = function;
    mSavedError.line      = line;

    if (result == VK_ERROR_DEVICE_LOST)
    {
        WARN() << "Internal Vulkan error (" << result << "): " << VulkanResultString(result)
               << ", in " << file << ", " << function << ":" << line << ".";
        mRenderer->notifyDeviceLost();
    }
}

// TODO(jmadill): Remove this. http://anglebug.com/3041
egl::Error DisplayVk::getEGLError(EGLint errorCode)
{
    std::stringstream errorStream;
    errorStream << "Internal Vulkan error (" << mSavedError.errorCode
                << "): " << VulkanResultString(mSavedError.errorCode) << ", in " << mSavedError.file
                << ", " << mSavedError.function << ":" << mSavedError.line << ".";
    std::string errorString = errorStream.str();

    return egl::Error(errorCode, 0, std::move(errorString));
}

void DisplayVk::populateFeatureList(angle::FeatureList *features)
{
    mRenderer->getFeatures().populateFeatureList(features);
}

ShareGroupVk::ShareGroupVk()
{
    mLastPruneTime = angle::GetCurrentSystemTime();
}

void ShareGroupVk::onDestroy(const egl::Display *display)
{
    RendererVk *renderer = vk::GetImpl(display)->getRenderer();

    for (std::unique_ptr<vk::BufferPool> &pool : mDefaultBufferPools)
    {
        if (pool)
        {
            pool->destroy(renderer);
        }
    }

    if (mSmallBufferPool)
    {
        mSmallBufferPool->destroy(renderer);
    }

    mPipelineLayoutCache.destroy(renderer);
    mDescriptorSetLayoutCache.destroy(renderer);

    ASSERT(mResourceUseLists.empty());
}

void ShareGroupVk::releaseResourceUseLists(const Serial &submitSerial)
{
    if (!mResourceUseLists.empty())
    {
        for (vk::ResourceUseList &it : mResourceUseLists)
        {
            it.releaseResourceUsesAndUpdateSerials(submitSerial);
        }
        mResourceUseLists.clear();
    }
}

vk::BufferPool *ShareGroupVk::getDefaultBufferPool(RendererVk *renderer,
                                                   VkDeviceSize size,
                                                   uint32_t memoryTypeIndex)
{
    if (size <= kMaxSizeToUseSmallBufferPool &&
        memoryTypeIndex ==
            renderer->getVertexConversionBufferMemoryTypeIndex(vk::MemoryHostVisibility::Visible))
    {
        if (!mSmallBufferPool)
        {
            const vk::Allocator &allocator = renderer->getAllocator();
            VkBufferUsageFlags usageFlags  = GetDefaultBufferUsageFlags(renderer);

            VkMemoryPropertyFlags memoryPropertyFlags;
            allocator.getMemoryTypeProperties(memoryTypeIndex, &memoryPropertyFlags);

            std::unique_ptr<vk::BufferPool> pool = std::make_unique<vk::BufferPool>();
            pool->initWithFlags(renderer, vma::VirtualBlockCreateFlagBits::BUDDY, usageFlags, 0,
                                memoryTypeIndex, memoryPropertyFlags);
            mSmallBufferPool = std::move(pool);
        }
        return mSmallBufferPool.get();
    }
    else if (!mDefaultBufferPools[memoryTypeIndex])
    {
        const vk::Allocator &allocator = renderer->getAllocator();
        VkBufferUsageFlags usageFlags  = GetDefaultBufferUsageFlags(renderer);

        VkMemoryPropertyFlags memoryPropertyFlags;
        allocator.getMemoryTypeProperties(memoryTypeIndex, &memoryPropertyFlags);

        std::unique_ptr<vk::BufferPool> pool = std::make_unique<vk::BufferPool>();
        pool->initWithFlags(renderer, vma::VirtualBlockCreateFlagBits::GENERAL, usageFlags, 0,
                            memoryTypeIndex, memoryPropertyFlags);
        mDefaultBufferPools[memoryTypeIndex] = std::move(pool);
    }

    return mDefaultBufferPools[memoryTypeIndex].get();
}

void ShareGroupVk::pruneDefaultBufferPools(RendererVk *renderer)
{
    mLastPruneTime = angle::GetCurrentSystemTime();

    for (std::unique_ptr<vk::BufferPool> &pool : mDefaultBufferPools)
    {
        if (pool)
        {
            pool->pruneEmptyBuffers(renderer);
        }
    }
    if (mSmallBufferPool)
    {
        mSmallBufferPool->pruneEmptyBuffers(renderer);
    }
}

bool ShareGroupVk::isDueForBufferPoolPrune()
{
    double timeElapsed = angle::GetCurrentSystemTime() - mLastPruneTime;
    return timeElapsed > kTimeElapsedForPruneDefaultBufferPool;
}
}  // namespace rx
