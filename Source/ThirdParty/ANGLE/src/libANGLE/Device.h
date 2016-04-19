//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Device.h: Implements the egl::Device class, representing the abstract
// device. Implements EGLDevice.

#ifndef LIBANGLE_DEVICE_H_
#define LIBANGLE_DEVICE_H_

#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/Display.h"

namespace rx
{
class DeviceImpl;
}

namespace egl
{
class Device final : angle::NonCopyable
{
  public:
    virtual ~Device();

    Error getDevice(EGLAttrib *value);
    Display *getOwningDisplay() { return mOwningDisplay; };
    EGLint getType();

    const DeviceExtensions &getExtensions() const;
    const std::string &getExtensionString() const;

    rx::DeviceImpl *getImplementation() { return mImplementation; }

    static egl::Error CreateDevice(void *devicePointer, EGLint deviceType, Device **outDevice);
    static egl::Error CreateDevice(Display *owningDisplay,
                                   rx::DeviceImpl *impl,
                                   Device **outDevice);

    static bool IsValidDevice(Device *device);

  private:
    Device(Display *owningDisplay, rx::DeviceImpl *impl);
    void initDeviceExtensions();

    Display *mOwningDisplay;
    rx::DeviceImpl *mImplementation;

    DeviceExtensions mDeviceExtensions;
    std::string mDeviceExtensionString;
};

}

#endif   // LIBANGLE_DEVICE_H_
