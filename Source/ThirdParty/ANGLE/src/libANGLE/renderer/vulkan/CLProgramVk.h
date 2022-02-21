//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLProgramVk.h: Defines the class interface for CLProgramVk, implementing CLProgramImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLPROGRAMVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLPROGRAMVK_H_

#include "libANGLE/renderer/vulkan/cl_types.h"

#include "libANGLE/renderer/CLProgramImpl.h"

namespace rx
{

class CLProgramVk : public CLProgramImpl
{
  public:
    CLProgramVk(const cl::Program &program);
    ~CLProgramVk() override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLPROGRAMVK_H_
