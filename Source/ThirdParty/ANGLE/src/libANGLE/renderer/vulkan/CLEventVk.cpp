//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLEventVk.cpp: Implements the class methods for CLEventVk.

#include "libANGLE/renderer/vulkan/CLEventVk.h"

#include "libANGLE/cl_utils.h"

namespace rx
{

CLEventVk::CLEventVk(const cl::Event &event) : CLEventImpl(event) {}

CLEventVk::~CLEventVk() = default;

angle::Result CLEventVk::getCommandExecutionStatus(cl_int &executionStatus)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLEventVk::setUserEventStatus(cl_int executionStatus)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLEventVk::setCallback(cl::Event &event, cl_int commandExecCallbackType)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLEventVk::getProfilingInfo(cl::ProfilingInfo name,
                                          size_t valueSize,
                                          void *value,
                                          size_t *valueSizeRet)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

}  // namespace rx
