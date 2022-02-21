//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLContextVk.h: Defines the class interface for CLContextVk, implementing CLContextImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLCONTEXTVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLCONTEXTVK_H_

#include "libANGLE/renderer/vulkan/cl_types.h"

#include "libANGLE/renderer/CLContextImpl.h"

namespace rx
{

class CLContextVk : public CLContextImpl
{
  public:
    CLContextVk(const cl::Context &context);
    ~CLContextVk() override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLCONTEXTVK_H_
