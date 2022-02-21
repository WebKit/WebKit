//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLContextImpl.h: Defines the abstract rx::CLContextImpl class.

#ifndef LIBANGLE_RENDERER_CLCONTEXTIMPL_H_
#define LIBANGLE_RENDERER_CLCONTEXTIMPL_H_

#include "libANGLE/renderer/CLCommandQueueImpl.h"
#include "libANGLE/renderer/CLEventImpl.h"
#include "libANGLE/renderer/CLMemoryImpl.h"
#include "libANGLE/renderer/CLProgramImpl.h"
#include "libANGLE/renderer/CLSamplerImpl.h"

namespace rx
{

class CLContextImpl : angle::NonCopyable
{
  public:
    using Ptr = std::unique_ptr<CLContextImpl>;

    CLContextImpl(const cl::Context &context);
    virtual ~CLContextImpl();

    virtual cl::DevicePtrs getDevices(cl_int &errorCode) const = 0;

    virtual CLCommandQueueImpl::Ptr createCommandQueue(const cl::CommandQueue &commandQueue,
                                                       cl_int &errorCode) = 0;

    virtual CLMemoryImpl::Ptr createBuffer(const cl::Buffer &buffer,
                                           size_t size,
                                           void *hostPtr,
                                           cl_int &errorCode) = 0;

    virtual CLMemoryImpl::Ptr createImage(const cl::Image &image,
                                          cl::MemFlags flags,
                                          const cl_image_format &format,
                                          const cl::ImageDescriptor &desc,
                                          void *hostPtr,
                                          cl_int &errorCode) = 0;

    virtual cl_int getSupportedImageFormats(cl::MemFlags flags,
                                            cl::MemObjectType imageType,
                                            cl_uint numEntries,
                                            cl_image_format *imageFormats,
                                            cl_uint *numImageFormats) = 0;

    virtual CLSamplerImpl::Ptr createSampler(const cl::Sampler &sampler, cl_int &errorCode) = 0;

    virtual CLProgramImpl::Ptr createProgramWithSource(const cl::Program &program,
                                                       const std::string &source,
                                                       cl_int &errorCode) = 0;

    virtual CLProgramImpl::Ptr createProgramWithIL(const cl::Program &program,
                                                   const void *il,
                                                   size_t length,
                                                   cl_int &errorCode) = 0;

    virtual CLProgramImpl::Ptr createProgramWithBinary(const cl::Program &program,
                                                       const size_t *lengths,
                                                       const unsigned char **binaries,
                                                       cl_int *binaryStatus,
                                                       cl_int &errorCode) = 0;

    virtual CLProgramImpl::Ptr createProgramWithBuiltInKernels(const cl::Program &program,
                                                               const char *kernel_names,
                                                               cl_int &errorCode) = 0;

    virtual CLProgramImpl::Ptr linkProgram(const cl::Program &program,
                                           const cl::DevicePtrs &devices,
                                           const char *options,
                                           const cl::ProgramPtrs &inputPrograms,
                                           cl::Program *notify,
                                           cl_int &errorCode) = 0;

    virtual CLEventImpl::Ptr createUserEvent(const cl::Event &event, cl_int &errorCode) = 0;

    virtual cl_int waitForEvents(const cl::EventPtrs &events) = 0;

  protected:
    const cl::Context &mContext;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_CLCONTEXTIMPL_H_
