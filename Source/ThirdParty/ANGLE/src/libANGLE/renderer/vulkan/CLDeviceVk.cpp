//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLDeviceVk.cpp: Implements the class methods for CLDeviceVk.

#include "libANGLE/renderer/vulkan/CLDeviceVk.h"

#include "libANGLE/renderer/vulkan/CLPlatformVk.h"

namespace rx
{

CLDeviceVk::CLDeviceVk(const cl::Device &device) : CLDeviceImpl(device) {}

CLDeviceVk::~CLDeviceVk() = default;

CLDeviceImpl::Info CLDeviceVk::createInfo(cl::DeviceType type) const
{
    Info info(type);
    return info;
}

cl_int CLDeviceVk::getInfoUInt(cl::DeviceInfo name, cl_uint *value) const
{
    return CL_INVALID_VALUE;
}

cl_int CLDeviceVk::getInfoULong(cl::DeviceInfo name, cl_ulong *value) const
{
    return CL_INVALID_VALUE;
}

cl_int CLDeviceVk::getInfoSizeT(cl::DeviceInfo name, size_t *value) const
{
    return CL_INVALID_VALUE;
}

cl_int CLDeviceVk::getInfoStringLength(cl::DeviceInfo name, size_t *value) const
{
    return CL_INVALID_VALUE;
}

cl_int CLDeviceVk::getInfoString(cl::DeviceInfo name, size_t size, char *value) const
{
    return CL_INVALID_VALUE;
}

cl_int CLDeviceVk::createSubDevices(const cl_device_partition_property *properties,
                                    cl_uint numDevices,
                                    CreateFuncs &subDevices,
                                    cl_uint *numDevicesRet)
{
    return CL_INVALID_VALUE;
}

}  // namespace rx
