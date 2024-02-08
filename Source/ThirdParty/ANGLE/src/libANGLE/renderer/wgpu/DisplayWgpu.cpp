//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayWgpu.cpp:
//    Implements the class methods for DisplayWgpu.
//

#include "libANGLE/renderer/wgpu/DisplayWgpu.h"

#include "common/debug.h"

#include "libANGLE/Display.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/DeviceWgpu.h"
#include "libANGLE/renderer/wgpu/ImageWgpu.h"
#include "libANGLE/renderer/wgpu/SurfaceWgpu.h"

namespace rx
{

DisplayWgpu::DisplayWgpu(const egl::DisplayState &state) : DisplayImpl(state) {}

DisplayWgpu::~DisplayWgpu() {}

egl::Error DisplayWgpu::initialize(egl::Display *display)
{
    return egl::NoError();
}

void DisplayWgpu::terminate() {}

egl::Error DisplayWgpu::makeCurrent(egl::Display *display,
                                    egl::Surface *drawSurface,
                                    egl::Surface *readSurface,
                                    gl::Context *context)
{
    // Ensure that the correct global DebugAnnotator is installed when the end2end tests change
    // the ANGLE back-end (done frequently).
    display->setGlobalDebugAnnotator();

    return egl::NoError();
}

egl::ConfigSet DisplayWgpu::generateConfigs()
{
    egl::Config config;
    config.renderTargetFormat    = GL_RGBA8;
    config.depthStencilFormat    = GL_DEPTH24_STENCIL8;
    config.bufferSize            = 32;
    config.redSize               = 8;
    config.greenSize             = 8;
    config.blueSize              = 8;
    config.alphaSize             = 8;
    config.alphaMaskSize         = 0;
    config.bindToTextureRGB      = EGL_TRUE;
    config.bindToTextureRGBA     = EGL_TRUE;
    config.colorBufferType       = EGL_RGB_BUFFER;
    config.configCaveat          = EGL_NONE;
    config.conformant            = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;
    config.depthSize             = 24;
    config.level                 = 0;
    config.matchNativePixmap     = EGL_NONE;
    config.maxPBufferWidth       = 0;
    config.maxPBufferHeight      = 0;
    config.maxPBufferPixels      = 0;
    config.maxSwapInterval       = 1;
    config.minSwapInterval       = 1;
    config.nativeRenderable      = EGL_TRUE;
    config.nativeVisualID        = 0;
    config.nativeVisualType      = EGL_NONE;
    config.renderableType        = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;
    config.sampleBuffers         = 0;
    config.samples               = 0;
    config.stencilSize           = 8;
    config.surfaceType           = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
    config.optimalOrientation    = 0;
    config.transparentType       = EGL_NONE;
    config.transparentRedValue   = 0;
    config.transparentGreenValue = 0;
    config.transparentBlueValue  = 0;

    egl::ConfigSet configSet;
    configSet.add(config);
    return configSet;
}

bool DisplayWgpu::testDeviceLost()
{
    return false;
}

egl::Error DisplayWgpu::restoreLostDevice(const egl::Display *display)
{
    return egl::NoError();
}

bool DisplayWgpu::isValidNativeWindow(EGLNativeWindowType window) const
{
    return true;
}

std::string DisplayWgpu::getRendererDescription()
{
    return "Wgpu";
}

std::string DisplayWgpu::getVendorString()
{
    return "Wgpu";
}

std::string DisplayWgpu::getVersionString(bool includeFullVersion)
{
    return std::string();
}

DeviceImpl *DisplayWgpu::createDevice()
{
    return new DeviceWgpu();
}

egl::Error DisplayWgpu::waitClient(const gl::Context *context)
{
    return egl::NoError();
}

egl::Error DisplayWgpu::waitNative(const gl::Context *context, EGLint engine)
{
    return egl::NoError();
}

gl::Version DisplayWgpu::getMaxSupportedESVersion() const
{
    return gl::Version(3, 2);
}

Optional<gl::Version> DisplayWgpu::getMaxSupportedDesktopVersion() const
{
    return Optional<gl::Version>::Invalid();
}

gl::Version DisplayWgpu::getMaxConformantESVersion() const
{
    return getMaxSupportedESVersion();
}

SurfaceImpl *DisplayWgpu::createWindowSurface(const egl::SurfaceState &state,
                                              EGLNativeWindowType window,
                                              const egl::AttributeMap &attribs)
{
    return new SurfaceWgpu(state);
}

SurfaceImpl *DisplayWgpu::createPbufferSurface(const egl::SurfaceState &state,
                                               const egl::AttributeMap &attribs)
{
    return new SurfaceWgpu(state);
}

SurfaceImpl *DisplayWgpu::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                        EGLenum buftype,
                                                        EGLClientBuffer buffer,
                                                        const egl::AttributeMap &attribs)
{
    return new SurfaceWgpu(state);
}

SurfaceImpl *DisplayWgpu::createPixmapSurface(const egl::SurfaceState &state,
                                              NativePixmapType nativePixmap,
                                              const egl::AttributeMap &attribs)
{
    return new SurfaceWgpu(state);
}

ImageImpl *DisplayWgpu::createImage(const egl::ImageState &state,
                                    const gl::Context *context,
                                    EGLenum target,
                                    const egl::AttributeMap &attribs)
{
    return new ImageWgpu(state);
}

rx::ContextImpl *DisplayWgpu::createContext(const gl::State &state,
                                            gl::ErrorSet *errorSet,
                                            const egl::Config *configuration,
                                            const gl::Context *shareContext,
                                            const egl::AttributeMap &attribs)
{
    return new ContextWgpu(state, errorSet);
}

StreamProducerImpl *DisplayWgpu::createStreamProducerD3DTexture(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

ShareGroupImpl *DisplayWgpu::createShareGroup(const egl::ShareGroupState &state)
{
    return new ShareGroupWgpu(state);
}

void DisplayWgpu::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->createContextRobustness            = true;
    outExtensions->postSubBuffer                      = true;
    outExtensions->createContext                      = true;
    outExtensions->image                              = true;
    outExtensions->imageBase                          = true;
    outExtensions->glTexture2DImage                   = true;
    outExtensions->glTextureCubemapImage              = true;
    outExtensions->glTexture3DImage                   = true;
    outExtensions->glRenderbufferImage                = true;
    outExtensions->getAllProcAddresses                = true;
    outExtensions->noConfigContext                    = true;
    outExtensions->directComposition                  = true;
    outExtensions->createContextNoError               = true;
    outExtensions->createContextWebGLCompatibility    = true;
    outExtensions->createContextBindGeneratesResource = true;
    outExtensions->swapBuffersWithDamage              = true;
    outExtensions->pixelFormatFloat                   = true;
    outExtensions->surfacelessContext                 = true;
    outExtensions->displayTextureShareGroup           = true;
    outExtensions->displaySemaphoreShareGroup         = true;
    outExtensions->createContextClientArrays          = true;
    outExtensions->programCacheControlANGLE           = true;
    outExtensions->robustResourceInitializationANGLE  = true;
}

void DisplayWgpu::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;
}

}  // namespace rx
