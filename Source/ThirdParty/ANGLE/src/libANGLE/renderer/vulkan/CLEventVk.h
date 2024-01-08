//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLEventVk.h: Defines the class interface for CLEventVk, implementing CLEventImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLEVENTVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLEVENTVK_H_

#include "libANGLE/renderer/vulkan/cl_types.h"

#include "libANGLE/renderer/CLEventImpl.h"

namespace rx
{

class CLEventVk : public CLEventImpl
{
  public:
    CLEventVk(const cl::Event &event);
    ~CLEventVk() override;

    angle::Result getCommandExecutionStatus(cl_int &executionStatus) override;

    angle::Result setUserEventStatus(cl_int executionStatus) override;

    angle::Result setCallback(cl::Event &event, cl_int commandExecCallbackType) override;

    angle::Result getProfilingInfo(cl::ProfilingInfo name,
                                   size_t valueSize,
                                   void *value,
                                   size_t *valueSizeRet) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLEVENTVK_H_
