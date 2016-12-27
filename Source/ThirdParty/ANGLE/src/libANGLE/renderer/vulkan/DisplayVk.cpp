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
#include "libANGLE/renderer/vulkan/ContextVk.h"

namespace rx
{

DisplayVk::DisplayVk() : DisplayImpl(), mRenderer(nullptr)
{
}

DisplayVk::~DisplayVk()
{
}

egl::Error DisplayVk::initialize(egl::Display *display)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

void DisplayVk::terminate()
{
    UNIMPLEMENTED();
}

egl::Error DisplayVk::makeCurrent(egl::Surface *drawSurface,
                                  egl::Surface *readSurface,
                                  gl::Context *context)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::ConfigSet DisplayVk::generateConfigs()
{
    UNIMPLEMENTED();
    return egl::ConfigSet();
}

bool DisplayVk::testDeviceLost()
{
    UNIMPLEMENTED();
    return bool();
}

egl::Error DisplayVk::restoreLostDevice()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

bool DisplayVk::isValidNativeWindow(EGLNativeWindowType window) const
{
    UNIMPLEMENTED();
    return bool();
}

std::string DisplayVk::getVendorString() const
{
    UNIMPLEMENTED();
    return std::string();
}

egl::Error DisplayVk::getDevice(DeviceImpl **device)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
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
                                            const egl::Config *configuration,
                                            EGLNativeWindowType window,
                                            const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

SurfaceImpl *DisplayVk::createPbufferSurface(const egl::SurfaceState &state,
                                             const egl::Config *configuration,
                                             const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

SurfaceImpl *DisplayVk::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                      const egl::Config *configuration,
                                                      EGLClientBuffer shareHandle,
                                                      const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

SurfaceImpl *DisplayVk::createPixmapSurface(const egl::SurfaceState &state,
                                            const egl::Config *configuration,
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
    return new ContextVk(state, mRenderer);
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
    UNIMPLEMENTED();
}

void DisplayVk::generateCaps(egl::Caps *outCaps) const
{
    UNIMPLEMENTED();
}

}  // namespace rx
