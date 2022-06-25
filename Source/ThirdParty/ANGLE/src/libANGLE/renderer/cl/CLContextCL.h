//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLContextCL.h: Defines the class interface for CLContextCL, implementing CLContextImpl.

#ifndef LIBANGLE_RENDERER_CL_CLCONTEXTCL_H_
#define LIBANGLE_RENDERER_CL_CLCONTEXTCL_H_

#include "libANGLE/renderer/cl/cl_types.h"

#include "libANGLE/renderer/CLContextImpl.h"

#include "common/Spinlock.h"
#include "common/SynchronizedValue.h"

#include <unordered_set>

namespace rx
{

class CLContextCL : public CLContextImpl
{
  public:
    CLContextCL(const cl::Context &context, cl_context native);
    ~CLContextCL() override;

    bool hasMemory(cl_mem memory) const;
    bool hasSampler(cl_sampler sampler) const;
    bool hasDeviceQueue(cl_command_queue queue) const;

    cl::DevicePtrs getDevices(cl_int &errorCode) const override;

    CLCommandQueueImpl::Ptr createCommandQueue(const cl::CommandQueue &commandQueue,
                                               cl_int &errorCode) override;

    CLMemoryImpl::Ptr createBuffer(const cl::Buffer &buffer,
                                   size_t size,
                                   void *hostPtr,
                                   cl_int &errorCode) override;

    CLMemoryImpl::Ptr createImage(const cl::Image &image,
                                  cl::MemFlags flags,
                                  const cl_image_format &format,
                                  const cl::ImageDescriptor &desc,
                                  void *hostPtr,
                                  cl_int &errorCode) override;

    cl_int getSupportedImageFormats(cl::MemFlags flags,
                                    cl::MemObjectType imageType,
                                    cl_uint numEntries,
                                    cl_image_format *imageFormats,
                                    cl_uint *numImageFormats) override;

    CLSamplerImpl::Ptr createSampler(const cl::Sampler &sampler, cl_int &errorCode) override;

    CLProgramImpl::Ptr createProgramWithSource(const cl::Program &program,
                                               const std::string &source,
                                               cl_int &errorCode) override;

    CLProgramImpl::Ptr createProgramWithIL(const cl::Program &program,
                                           const void *il,
                                           size_t length,
                                           cl_int &errorCode) override;

    CLProgramImpl::Ptr createProgramWithBinary(const cl::Program &program,
                                               const size_t *lengths,
                                               const unsigned char **binaries,
                                               cl_int *binaryStatus,
                                               cl_int &errorCode) override;

    CLProgramImpl::Ptr createProgramWithBuiltInKernels(const cl::Program &program,
                                                       const char *kernel_names,
                                                       cl_int &errorCode) override;

    CLProgramImpl::Ptr linkProgram(const cl::Program &program,
                                   const cl::DevicePtrs &devices,
                                   const char *options,
                                   const cl::ProgramPtrs &inputPrograms,
                                   cl::Program *notify,
                                   cl_int &errorCode) override;

    CLEventImpl::Ptr createUserEvent(const cl::Event &event, cl_int &errorCode) override;

    cl_int waitForEvents(const cl::EventPtrs &events) override;

  private:
    struct Mutable
    {
        std::unordered_set<const _cl_mem *> mMemories;
        std::unordered_set<const _cl_sampler *> mSamplers;
        std::unordered_set<const _cl_command_queue *> mDeviceQueues;
    };
    using MutableData = angle::SynchronizedValue<Mutable, angle::Spinlock>;

    const cl_context mNative;
    MutableData mData;

    friend class CLCommandQueueCL;
    friend class CLMemoryCL;
    friend class CLSamplerCL;
};

inline bool CLContextCL::hasMemory(cl_mem memory) const
{
    const auto data = mData.synchronize();
    return data->mMemories.find(memory) != data->mMemories.cend();
}

inline bool CLContextCL::hasSampler(cl_sampler sampler) const
{
    const auto data = mData.synchronize();
    return data->mSamplers.find(sampler) != data->mSamplers.cend();
}

inline bool CLContextCL::hasDeviceQueue(cl_command_queue queue) const
{
    const auto data = mData.synchronize();
    return data->mDeviceQueues.find(queue) != data->mDeviceQueues.cend();
}

}  // namespace rx

#endif  // LIBANGLE_RENDERER_CL_CLCONTEXTCL_H_
