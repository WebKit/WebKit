//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLKernelVk.cpp: Implements the class methods for CLKernelVk.

#include "libANGLE/renderer/vulkan/CLKernelVk.h"

#include "libANGLE/cl_utils.h"

namespace rx
{

CLKernelVk::CLKernelVk(const cl::Kernel &kernel) : CLKernelImpl(kernel) {}

CLKernelVk::~CLKernelVk() = default;

angle::Result CLKernelVk::setArg(cl_uint argIndex, size_t argSize, const void *argValue)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLKernelVk::createInfo(CLKernelImpl::Info *info) const
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

}  // namespace rx
