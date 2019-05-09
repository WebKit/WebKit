//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeviceVk.h:
//    Defines the class interface for DeviceVk, implementing DeviceImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_DEVICEVK_H_
#define LIBANGLE_RENDERER_VULKAN_DEVICEVK_H_

#include "libANGLE/renderer/DeviceImpl.h"

namespace rx
{

class DeviceVk : public DeviceImpl
{
  public:
    DeviceVk();
    ~DeviceVk() override;

    egl::Error initialize() override;
    egl::Error getDevice(void **outValue) override;
    EGLint getType() override;
    void generateExtensions(egl::DeviceExtensions *outExtensions) const override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_DEVICEVK_H_
