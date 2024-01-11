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

#include "libANGLE/CLDevice.h"

namespace rx
{

class CLContextVk : public CLContextImpl
{
  public:
    CLContextVk(const cl::Context &context);
    ~CLContextVk() override;

    bool hasMemory(cl_mem memory) const;

    angle::Result getDevices(cl::DevicePtrs *devicePtrsOut) const override;

    angle::Result createCommandQueue(const cl::CommandQueue &commandQueue,
                                     CLCommandQueueImpl::Ptr *commandQueueOut) override;

    angle::Result createBuffer(const cl::Buffer &buffer,
                               size_t size,
                               void *hostPtr,
                               CLMemoryImpl::Ptr *bufferOut) override;

    angle::Result createImage(const cl::Image &image,
                              cl::MemFlags flags,
                              const cl_image_format &format,
                              const cl::ImageDescriptor &desc,
                              void *hostPtr,
                              CLMemoryImpl::Ptr *imageOut) override;

    angle::Result getSupportedImageFormats(cl::MemFlags flags,
                                           cl::MemObjectType imageType,
                                           cl_uint numEntries,
                                           cl_image_format *imageFormats,
                                           cl_uint *numImageFormats) override;

    angle::Result createSampler(const cl::Sampler &sampler,
                                CLSamplerImpl::Ptr *samplerOut) override;

    angle::Result createProgramWithSource(const cl::Program &program,
                                          const std::string &source,
                                          CLProgramImpl::Ptr *programOut) override;

    angle::Result createProgramWithIL(const cl::Program &program,
                                      const void *il,
                                      size_t length,
                                      CLProgramImpl::Ptr *programOut) override;

    angle::Result createProgramWithBinary(const cl::Program &program,
                                          const size_t *lengths,
                                          const unsigned char **binaries,
                                          cl_int *binaryStatus,
                                          CLProgramImpl::Ptr *programOut) override;

    angle::Result createProgramWithBuiltInKernels(const cl::Program &program,
                                                  const char *kernel_names,
                                                  CLProgramImpl::Ptr *programOut) override;

    angle::Result linkProgram(const cl::Program &program,
                              const cl::DevicePtrs &devices,
                              const char *options,
                              const cl::ProgramPtrs &inputPrograms,
                              cl::Program *notify,
                              CLProgramImpl::Ptr *programOut) override;

    angle::Result createUserEvent(const cl::Event &event, CLEventImpl::Ptr *eventOut) override;

    angle::Result waitForEvents(const cl::EventPtrs &events) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLCONTEXTVK_H_
