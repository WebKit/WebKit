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
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"

namespace rx
{

DisplayVk::DisplayVk(const egl::DisplayState &state) : DisplayImpl(state), mRenderer(nullptr)
{
}

DisplayVk::~DisplayVk()
{
}

egl::Error DisplayVk::initialize(egl::Display *display)
{
    ASSERT(!mRenderer && display != nullptr);
    mRenderer.reset(new RendererVk());
    return mRenderer->initialize(display->getAttributeMap()).toEGL(EGL_NOT_INITIALIZED);
}

void DisplayVk::terminate()
{
    mRenderer.reset(nullptr);
}

egl::Error DisplayVk::makeCurrent(egl::Surface * /*drawSurface*/,
                                  egl::Surface * /*readSurface*/,
                                  gl::Context * /*context*/)
{
    return egl::Error(EGL_SUCCESS);
}

egl::ConfigSet DisplayVk::generateConfigs()
{
    // TODO(jmadill): Multiple configs, pbuffers, and proper checking of config attribs.
    egl::Config singleton;
    singleton.renderTargetFormat    = GL_RGBA8;
    singleton.depthStencilFormat    = GL_NONE;
    singleton.bufferSize            = 32;
    singleton.redSize               = 8;
    singleton.greenSize             = 8;
    singleton.blueSize              = 8;
    singleton.alphaSize             = 8;
    singleton.alphaMaskSize         = 0;
    singleton.bindToTextureRGB      = EGL_FALSE;
    singleton.bindToTextureRGBA     = EGL_FALSE;
    singleton.colorBufferType       = EGL_RGB_BUFFER;
    singleton.configCaveat          = EGL_NONE;
    singleton.conformant            = 0;
    singleton.depthSize             = 0;
    singleton.stencilSize           = 0;
    singleton.level                 = 0;
    singleton.matchNativePixmap     = EGL_NONE;
    singleton.maxPBufferWidth       = 0;
    singleton.maxPBufferHeight      = 0;
    singleton.maxPBufferPixels      = 0;
    singleton.maxSwapInterval       = 1;
    singleton.minSwapInterval       = 1;
    singleton.nativeRenderable      = EGL_TRUE;
    singleton.nativeVisualID        = 0;
    singleton.nativeVisualType      = EGL_NONE;
    singleton.renderableType        = EGL_OPENGL_ES2_BIT;
    singleton.sampleBuffers         = 0;
    singleton.samples               = 0;
    singleton.surfaceType           = EGL_WINDOW_BIT;
    singleton.optimalOrientation    = 0;
    singleton.transparentType       = EGL_NONE;
    singleton.transparentRedValue   = 0;
    singleton.transparentGreenValue = 0;
    singleton.transparentBlueValue  = 0;
    singleton.colorComponentType    = EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;

    egl::ConfigSet configSet;
    configSet.add(singleton);
    return configSet;
}

bool DisplayVk::testDeviceLost()
{
    // TODO(jmadill): Figure out how to do device lost in Vulkan.
    return false;
}

egl::Error DisplayVk::restoreLostDevice()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

bool DisplayVk::isValidNativeWindow(EGLNativeWindowType window) const
{
    // TODO(jmadill): Cross-platform this.
    return (IsWindow(window) == TRUE);
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

egl::Error DisplayVk::getDevice(DeviceImpl **device)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayVk::waitClient() const
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error DisplayVk::waitNative(EGLint engine,
                                 egl::Surface *drawSurface,
                                 egl::Surface *readSurface) const
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

SurfaceImpl *DisplayVk::createWindowSurface(const egl::SurfaceState &state,
                                            EGLNativeWindowType window,
                                            const egl::AttributeMap &attribs)
{
    EGLint width  = attribs.getAsInt(EGL_WIDTH, 0);
    EGLint height = attribs.getAsInt(EGL_HEIGHT, 0);

    return new WindowSurfaceVk(state, window, width, height);
}

SurfaceImpl *DisplayVk::createPbufferSurface(const egl::SurfaceState &state,
                                             const egl::AttributeMap &attribs)
{
    ASSERT(mRenderer);

    EGLint width  = attribs.getAsInt(EGL_WIDTH, 0);
    EGLint height = attribs.getAsInt(EGL_HEIGHT, 0);

    return new OffscreenSurfaceVk(state, width, height);
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

ImageImpl *DisplayVk::createImage(EGLenum target,
                                  egl::ImageSibling *buffer,
                                  const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<ImageImpl *>(0);
}

ContextImpl *DisplayVk::createContext(const gl::ContextState &state)
{
    return new ContextVk(state, mRenderer.get());
}

StreamProducerImpl *DisplayVk::createStreamProducerD3DTextureNV12(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<StreamProducerImpl *>(0);
}

gl::Version DisplayVk::getMaxSupportedESVersion() const
{
    UNIMPLEMENTED();
    return gl::Version(0, 0);
}

void DisplayVk::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
}

void DisplayVk::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;
}

}  // namespace rx
