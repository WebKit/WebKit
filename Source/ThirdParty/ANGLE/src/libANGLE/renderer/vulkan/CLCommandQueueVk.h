//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLCommandQueueVk.h: Defines the class interface for CLCommandQueueVk,
// implementing CLCommandQueueImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLCOMMANDQUEUEVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLCOMMANDQUEUEVK_H_

#include "libANGLE/renderer/vulkan/cl_types.h"

#include "libANGLE/renderer/CLCommandQueueImpl.h"

namespace rx
{

class CLCommandQueueVk : public CLCommandQueueImpl
{
  public:
    CLCommandQueueVk(const cl::CommandQueue &commandQueue);
    ~CLCommandQueueVk() override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLCOMMANDQUEUEVK_H_
