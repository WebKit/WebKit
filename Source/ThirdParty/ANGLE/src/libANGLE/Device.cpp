//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Device.cpp: Implements the egl::Device class, representing the abstract
// device. Implements EGLDevice.

#include "libANGLE/Device.h"

#include <iterator>

#include <platform/Platform.h>
#include <EGL/eglext.h>

#include "common/debug.h"
#include "common/platform.h"
#include "libANGLE/renderer/DeviceImpl.h"

#if defined(ANGLE_ENABLE_D3D11)
#include "libANGLE/renderer/d3d/DeviceD3D.h"
#endif

namespace egl
{

template <typename T>
static std::string GenerateExtensionsString(const T &extensions)
{
    std::vector<std::string> extensionsVector = extensions.getStrings();

    std::ostringstream stream;
    std::copy(extensionsVector.begin(), extensionsVector.end(), std::ostream_iterator<std::string>(stream, " "));
    return stream.str();
}

typedef std::set<egl::Device *> DeviceSet;
static DeviceSet *GetDeviceSet()
{
    static DeviceSet devices;
    return &devices;
}

// Static factory methods
egl::Error Device::CreateDevice(void *devicePointer, EGLint deviceType, Device **outDevice)
{
    *outDevice = nullptr;

#if defined(ANGLE_ENABLE_D3D11)
    if (deviceType == EGL_D3D11_DEVICE_ANGLE)
    {
        std::unique_ptr<rx::DeviceD3D> deviceD3D(new rx::DeviceD3D());
        ANGLE_TRY(deviceD3D->initialize(devicePointer, deviceType, EGL_TRUE));
        *outDevice = new Device(nullptr, deviceD3D.release());
        GetDeviceSet()->insert(*outDevice);
        return NoError();
    }
#endif

    // Note that creating an EGL device from inputted D3D9 parameters isn't currently supported
    return EglBadAttribute();
}

egl::Error Device::CreateDevice(Display *owningDisplay, rx::DeviceImpl *impl, Device **outDevice)
{
    *outDevice = new Device(owningDisplay, impl);
    GetDeviceSet()->insert(*outDevice);
    return NoError();
}

bool Device::IsValidDevice(Device *device)
{
    const DeviceSet *deviceSet = GetDeviceSet();
    return deviceSet->find(device) != deviceSet->end();
}

Device::Device(Display *owningDisplay, rx::DeviceImpl *impl)
    : mOwningDisplay(owningDisplay), mImplementation(impl)
{
    initDeviceExtensions();
}

Device::~Device()
{
    ASSERT(GetDeviceSet()->find(this) != GetDeviceSet()->end());
    GetDeviceSet()->erase(this);

    if (mImplementation->deviceExternallySourced())
    {
        // If the device isn't externally sourced then it is up to the renderer to delete the impl
        SafeDelete(mImplementation);
    }
}

Error Device::getDevice(EGLAttrib *value)
{
    void *nativeDevice = nullptr;
    egl::Error error = getImplementation()->getDevice(&nativeDevice);
    *value = reinterpret_cast<EGLAttrib>(nativeDevice);
    return error;
}

EGLint Device::getType()
{
    return getImplementation()->getType();
}

void Device::initDeviceExtensions()
{
    mImplementation->generateExtensions(&mDeviceExtensions);
    mDeviceExtensionString = GenerateExtensionsString(mDeviceExtensions);
}

const DeviceExtensions &Device::getExtensions() const
{
    return mDeviceExtensions;
}

const std::string &Device::getExtensionString() const
{
    return mDeviceExtensionString;
}

}
