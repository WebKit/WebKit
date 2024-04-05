//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLContextVk.h: Defines the class interface for CLContextVk, implementing CLContextImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLCONTEXTVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLCONTEXTVK_H_

#include "libANGLE/renderer/vulkan/CLPlatformVk.h"
#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

#include "libANGLE/renderer/CLContextImpl.h"

#include <libANGLE/CLContext.h>
#include "libANGLE/CLDevice.h"

namespace rx
{

class CLContextVk : public CLContextImpl, public vk::Context
{
  public:
    CLContextVk(const cl::Context &context, const cl::DevicePtrs devicePtrs);

    ~CLContextVk() override;

    void handleError(VkResult errorCode,
                     const char *file,
                     const char *function,
                     unsigned int line) override;

    bool hasMemory(cl_mem memory) const;

    angle::Result getDevices(cl::DevicePtrs *devicePtrsOut) const override;

    angle::Result createCommandQueue(const cl::CommandQueue &commandQueue,
                                     CLCommandQueueImpl::Ptr *commandQueueOut) override;

    angle::Result createBuffer(const cl::Buffer &buffer,
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

    CLPlatformVk *getPlatform() { return &mContext.getPlatform().getImpl<CLPlatformVk>(); }

  private:
    void handleDeviceLost() const;

    // Have the CL Context keep tabs on associated CL objects
    struct Mutable
    {
        std::unordered_set<const _cl_mem *> mMemories;
    };
    using MutableData = angle::SynchronizedValue<Mutable>;
    MutableData mAssociatedObjects;

    const cl::DevicePtrs mAssociatedDevices;

    friend class CLMemoryVk;
};

inline bool CLContextVk::hasMemory(cl_mem memory) const
{
    const auto data = mAssociatedObjects.synchronize();
    return data->mMemories.find(memory) != data->mMemories.cend();
}

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLCONTEXTVK_H_
