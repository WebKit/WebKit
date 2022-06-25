//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLDeviceVk.h: Defines the class interface for CLDeviceVk, implementing CLDeviceImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLDEVICEVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLDEVICEVK_H_

#include "libANGLE/renderer/vulkan/cl_types.h"

#include "libANGLE/renderer/CLDeviceImpl.h"

namespace rx
{

class CLDeviceVk : public CLDeviceImpl
{
  public:
    explicit CLDeviceVk(const cl::Device &device);
    ~CLDeviceVk() override;

    Info createInfo(cl::DeviceType type) const override;

    cl_int getInfoUInt(cl::DeviceInfo name, cl_uint *value) const override;
    cl_int getInfoULong(cl::DeviceInfo name, cl_ulong *value) const override;
    cl_int getInfoSizeT(cl::DeviceInfo name, size_t *value) const override;
    cl_int getInfoStringLength(cl::DeviceInfo name, size_t *value) const override;
    cl_int getInfoString(cl::DeviceInfo name, size_t size, char *value) const override;

    cl_int createSubDevices(const cl_device_partition_property *properties,
                            cl_uint numDevices,
                            CreateFuncs &subDevices,
                            cl_uint *numDevicesRet) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLDEVICEVK_H_
