//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DeviceD3D.cpp: D3D implementation of egl::Device

#include "libANGLE/renderer/d3d/DeviceD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"

#include "libANGLE/Device.h"
#include "libANGLE/Display.h"

#include <EGL/eglext.h>

namespace rx
{

DeviceD3D::DeviceD3D(GLint deviceType, void *nativeDevice)
    : mDevice(nativeDevice), mDeviceType(deviceType), mIsInitialized(false)
{}

DeviceD3D::~DeviceD3D()
{
#if defined(ANGLE_ENABLE_D3D11)
    if (mIsInitialized && mDeviceType == EGL_D3D11_DEVICE_ANGLE)
    {
        // DeviceD3D holds a ref to an externally-sourced D3D11 device. We must release it.
        ID3D11Device *device = static_cast<ID3D11Device *>(mDevice);
        device->Release();
    }
#endif
}

egl::Error DeviceD3D::getDevice(void **outValue)
{
    ASSERT(mIsInitialized);
    *outValue = mDevice;
    return egl::NoError();
}

egl::Error DeviceD3D::initialize()
{
    ASSERT(!mIsInitialized);

#if defined(ANGLE_ENABLE_D3D11)
    if (mDeviceType == EGL_D3D11_DEVICE_ANGLE)
    {
        // Validate the device
        IUnknown *iunknown = static_cast<IUnknown *>(mDevice);

        ID3D11Device *d3dDevice = nullptr;
        HRESULT hr =
            iunknown->QueryInterface(__uuidof(ID3D11Device), reinterpret_cast<void **>(&d3dDevice));
        if (FAILED(hr))
        {
            return egl::EglBadAttribute() << "Invalid D3D device passed into EGLDeviceEXT";
        }

        // The QI to ID3D11Device adds a ref to the D3D11 device.
        // Deliberately don't release the ref here, so that the DeviceD3D holds a ref to the
        // D3D11 device.
    }
#endif

    mIsInitialized = true;

    return egl::NoError();
}

EGLint DeviceD3D::getType()
{
    return mDeviceType;
}

void DeviceD3D::generateExtensions(egl::DeviceExtensions *outExtensions) const
{
    outExtensions->deviceD3D = true;
}

}  // namespace rx
