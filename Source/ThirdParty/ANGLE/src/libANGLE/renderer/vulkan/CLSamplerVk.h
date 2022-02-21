//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLSamplerVk.h: Defines the class interface for CLSamplerVk, implementing CLSamplerImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLSAMPLERVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLSAMPLERVK_H_

#include "libANGLE/renderer/vulkan/cl_types.h"

#include "libANGLE/renderer/CLSamplerImpl.h"

namespace rx
{

class CLSamplerVk : public CLSamplerImpl
{
  public:
    CLSamplerVk(const cl::Sampler &sampler);
    ~CLSamplerVk() override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLSAMPLERVK_H_
