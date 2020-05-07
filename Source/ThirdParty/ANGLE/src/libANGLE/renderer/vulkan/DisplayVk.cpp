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
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/ImageVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"
#include "libANGLE/renderer/vulkan/SyncVk.h"
#include "libANGLE/trace.h"

namespace rx
{

DisplayVk::DisplayVk(const egl::DisplayState &state)
    : DisplayImpl(state), vk::Context(new RendererVk()), mScratchBuffer(1000u)
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

egl::Error DisplayVk::makeCurrent(egl::Surface * /*drawSurface*/,
                                  egl::Surface * /*readSurface*/,
                                  gl::Context * /*context*/)
{
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

std::string DisplayVk::getVendorString() const
{
    std::string vendorString = "Google Inc.";
    if (mRenderer)
    {
        vendorString += " " + mRenderer->getVendorString();
    }

    return vendorString;
}

DeviceImpl *DisplayVk::createDevice()
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::Error DisplayVk::waitClient(const gl::Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "DisplayVk::waitClient");
    ContextVk *contextVk = vk::GetImpl(context);
    return angle::ToEGL(contextVk->finishImpl(), this, EGL_BAD_ACCESS);
}

egl::Error DisplayVk::waitNative(const gl::Context *context, EGLint engine)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
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
    return new OffscreenSurfaceVk(state);
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

rx::ContextImpl *DisplayVk::createContext(const gl::State &state,
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

void DisplayVk::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->createContextRobustness      = true;
    outExtensions->surfaceOrientation           = true;
    outExtensions->displayTextureShareGroup     = true;
    outExtensions->robustResourceInitialization = true;

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
    outExtensions->glColorspace = getRenderer()->getFeatures().supportsSwapchainColorspace.enabled;

#if defined(ANGLE_PLATFORM_ANDROID)
    outExtensions->framebufferTargetANDROID = true;
#endif  // defined(ANGLE_PLATFORM_ANDROID)

    outExtensions->contextPriority = true;
    outExtensions->noConfigContext = true;

#if defined(ANGLE_PLATFORM_GGP)
    outExtensions->ggpStreamDescriptor = true;
    outExtensions->swapWithFrameToken  = true;
#endif  // defined(ANGLE_PLATFORM_GGP)
}

void DisplayVk::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;
}

const char *DisplayVk::getWSILayer() const
{
    return nullptr;
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

    std::stringstream errorStream;
    errorStream << "Internal Vulkan error: " << VulkanResultString(result) << ", in " << file
                << ", " << function << ":" << line << ".";
    mStoredErrorString = errorStream.str();

    if (result == VK_ERROR_DEVICE_LOST)
    {
        WARN() << mStoredErrorString;
        mRenderer->notifyDeviceLost();
    }
}

// TODO(jmadill): Remove this. http://anglebug.com/3041
egl::Error DisplayVk::getEGLError(EGLint errorCode)
{
    return egl::Error(errorCode, 0, std::move(mStoredErrorString));
}

void DisplayVk::populateFeatureList(angle::FeatureList *features)
{
    mRenderer->getFeatures().populateFeatureList(features);
}

}  // namespace rx
