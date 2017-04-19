//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayImpl.h: Implementation methods of egl::Display

#ifndef LIBANGLE_RENDERER_DISPLAYIMPL_H_
#define LIBANGLE_RENDERER_DISPLAYIMPL_H_

#include "common/angleutils.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Config.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/EGLImplFactory.h"
#include "libANGLE/Stream.h"
#include "libANGLE/Version.h"

#include <set>
#include <vector>

namespace egl
{
class AttributeMap;
class Display;
struct DisplayState;
struct Config;
class Surface;
class ImageSibling;
}

namespace gl
{
class Context;
}

namespace rx
{
class SurfaceImpl;
class ImageImpl;
struct ConfigDesc;
class DeviceImpl;
class StreamProducerImpl;

class DisplayImpl : public EGLImplFactory
{
  public:
    DisplayImpl(const egl::DisplayState &state);
    virtual ~DisplayImpl();

    virtual egl::Error initialize(egl::Display *display) = 0;
    virtual void terminate() = 0;

    virtual egl::Error makeCurrent(egl::Surface *drawSurface, egl::Surface *readSurface, gl::Context *context) = 0;

    virtual egl::ConfigSet generateConfigs() = 0;

    virtual bool testDeviceLost() = 0;
    virtual egl::Error restoreLostDevice() = 0;

    virtual bool isValidNativeWindow(EGLNativeWindowType window) const = 0;
    virtual egl::Error validateClientBuffer(const egl::Config *configuration,
                                            EGLenum buftype,
                                            EGLClientBuffer clientBuffer,
                                            const egl::AttributeMap &attribs) const;

    virtual std::string getVendorString() const = 0;

    virtual egl::Error getDevice(DeviceImpl **device) = 0;

    virtual egl::Error waitClient() const = 0;
    virtual egl::Error waitNative(EGLint engine,
                                  egl::Surface *drawSurface,
                                  egl::Surface *readSurface) const = 0;
    virtual gl::Version getMaxSupportedESVersion() const           = 0;
    const egl::Caps &getCaps() const;

    const egl::DisplayExtensions &getExtensions() const;

  protected:
    const egl::DisplayState &mState;

  private:
    virtual void generateExtensions(egl::DisplayExtensions *outExtensions) const = 0;
    virtual void generateCaps(egl::Caps *outCaps) const = 0;

    mutable bool mExtensionsInitialized;
    mutable egl::DisplayExtensions mExtensions;

    mutable bool mCapsInitialized;
    mutable egl::Caps mCaps;
};

}

#endif // LIBANGLE_RENDERER_DISPLAYIMPL_H_
