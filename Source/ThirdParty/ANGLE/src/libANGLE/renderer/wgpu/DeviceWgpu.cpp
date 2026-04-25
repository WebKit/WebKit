//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeviceWgpu.cpp:
//    Implements the class methods for DeviceWgpu.
//

#include "libANGLE/renderer/wgpu/DeviceWgpu.h"

#include "common/debug.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"

namespace rx
{

DeviceWgpu::DeviceWgpu() : DeviceImpl() {}

DeviceWgpu::~DeviceWgpu() {}

egl::Error DeviceWgpu::initialize()
{
    return egl::NoError();
}

egl::Error DeviceWgpu::getAttribute(const egl::Display *display, EGLint attribute, void **outValue)
{
    DisplayWgpu *displayWgpu  = webgpu::GetImpl(display);
    const DawnProcTable *wgpu = displayWgpu->getProcs();

    switch (attribute)
    {
        case EGL_WEBGPU_DEVICE_ANGLE:
        {
            webgpu::DeviceHandle device = displayWgpu->getDevice();
            wgpu->deviceAddRef(device.get());
            *outValue = device.get();
            break;
        }
        case EGL_WEBGPU_ADAPTER_ANGLE:
        {
            webgpu::AdapterHandle adapater = displayWgpu->getAdapter();
            wgpu->adapterAddRef(adapater.get());
            *outValue = adapater.get();
            break;
        }
        default:
            return egl::Error(EGL_BAD_ATTRIBUTE);
    }

    return egl::NoError();
}

void DeviceWgpu::generateExtensions(egl::DeviceExtensions *outExtensions) const
{
    outExtensions->deviceWebGPU = true;
}

}  // namespace rx
