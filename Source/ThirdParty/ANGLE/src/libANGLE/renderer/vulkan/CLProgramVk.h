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

    angle::Result build(const cl::DevicePtrs &devices,
                        const char *options,
                        cl::Program *notify) override;

    angle::Result compile(const cl::DevicePtrs &devices,
                          const char *options,
                          const cl::ProgramPtrs &inputHeaders,
                          const char **headerIncludeNames,
                          cl::Program *notify) override;

    angle::Result getInfo(cl::ProgramInfo name,
                          size_t valueSize,
                          void *value,
                          size_t *valueSizeRet) const override;

    angle::Result getBuildInfo(const cl::Device &device,
                               cl::ProgramBuildInfo name,
                               size_t valueSize,
                               void *value,
                               size_t *valueSizeRet) const override;

    angle::Result createKernel(const cl::Kernel &kernel,
                               const char *name,
                               CLKernelImpl::Ptr *kernelOut) override;

    angle::Result createKernels(cl_uint numKernels,
                                CLKernelImpl::CreateFuncs &createFuncs,
                                cl_uint *numKernelsRet) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLPROGRAMVK_H_
