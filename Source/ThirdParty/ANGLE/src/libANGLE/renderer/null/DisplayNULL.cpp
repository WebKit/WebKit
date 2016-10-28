//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayNULL.cpp:
//    Implements the class methods for DisplayNULL.
//

#include "libANGLE/renderer/null/DisplayNULL.h"

#include "common/debug.h"

namespace rx
{

DisplayNULL::DisplayNULL() : DisplayImpl()
{
}

DisplayNULL::~DisplayNULL()
{
}

egl::Error DisplayNULL::initialize(egl::Display *display)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

void DisplayNULL::terminate()
{
    UNIMPLEMENTED();
}

egl::Error DisplayNULL::makeCurrent(egl::Surface *drawSurface,
                                    egl::Surface *readSurface,
                                    gl::Context *context)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::ConfigSet DisplayNULL::generateConfigs()
{
    UNIMPLEMENTED();
    return egl::ConfigSet();
}

bool DisplayNULL::testDeviceLost()
{
    UNIMPLEMENTED();
    return bool();
}

egl::Error DisplayNULL::restoreLostDevice()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

bool DisplayNULL::isValidNativeWindow(EGLNativeWindowType window) const
{
    UNIMPLEMENTED();
    return bool();
}

std::string DisplayNULL::getVendorString() const
{
    UNIMPLEMENTED();
    return std::string();
}

egl::Error DisplayNULL::getDevice(DeviceImpl **device)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error DisplayNULL::waitClient() const
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error DisplayNULL::waitNative(EGLint engine,
                                   egl::Surface *drawSurface,
                                   egl::Surface *readSurface) const
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

gl::Version DisplayNULL::getMaxSupportedESVersion() const
{
    UNIMPLEMENTED();
    return gl::Version();
}

SurfaceImpl *DisplayNULL::createWindowSurface(const egl::SurfaceState &state,
                                              const egl::Config *configuration,
                                              EGLNativeWindowType window,
                                              const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

SurfaceImpl *DisplayNULL::createPbufferSurface(const egl::SurfaceState &state,
                                               const egl::Config *configuration,
                                               const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

SurfaceImpl *DisplayNULL::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                        const egl::Config *configuration,
                                                        EGLClientBuffer shareHandle,
                                                        const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

SurfaceImpl *DisplayNULL::createPixmapSurface(const egl::SurfaceState &state,
                                              const egl::Config *configuration,
                                              NativePixmapType nativePixmap,
                                              const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

ImageImpl *DisplayNULL::createImage(EGLenum target,
                                    egl::ImageSibling *buffer,
                                    const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<ImageImpl *>(0);
}

ContextImpl *DisplayNULL::createContext(const gl::ContextState &state)
{
    UNIMPLEMENTED();
    return static_cast<ContextImpl *>(0);
}

StreamProducerImpl *DisplayNULL::createStreamProducerD3DTextureNV12(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<StreamProducerImpl *>(0);
}

void DisplayNULL::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    UNIMPLEMENTED();
}

void DisplayNULL::generateCaps(egl::Caps *outCaps) const
{
    UNIMPLEMENTED();
}

}  // namespace rx
