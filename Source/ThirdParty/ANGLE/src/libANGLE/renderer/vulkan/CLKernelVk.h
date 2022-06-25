//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLKernelVk.h: Defines the class interface for CLKernelVk, implementing CLKernelImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLKERNELVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLKERNELVK_H_

#include "libANGLE/renderer/vulkan/cl_types.h"

#include "libANGLE/renderer/CLKernelImpl.h"

namespace rx
{

class CLKernelVk : public CLKernelImpl
{
  public:
    CLKernelVk(const cl::Kernel &kernel);
    ~CLKernelVk() override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLKERNELVK_H_
