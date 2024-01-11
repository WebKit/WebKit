//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLContextVk.cpp: Implements the class methods for CLContextVk.

#include "libANGLE/renderer/vulkan/CLContextVk.h"

#include "libANGLE/cl_utils.h"

namespace rx
{

CLContextVk::CLContextVk(const cl::Context &context) : CLContextImpl(context) {}

CLContextVk::~CLContextVk() = default;

angle::Result CLContextVk::getDevices(cl::DevicePtrs *devicePtrsOut) const
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::createCommandQueue(const cl::CommandQueue &commandQueue,
                                              CLCommandQueueImpl::Ptr *commandQueueOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::createBuffer(const cl::Buffer &buffer,
                                        size_t size,
                                        void *hostPtr,
                                        CLMemoryImpl::Ptr *bufferOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::createImage(const cl::Image &image,
                                       cl::MemFlags flags,
                                       const cl_image_format &format,
                                       const cl::ImageDescriptor &desc,
                                       void *hostPtr,
                                       CLMemoryImpl::Ptr *imageOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::getSupportedImageFormats(cl::MemFlags flags,
                                                    cl::MemObjectType imageType,
                                                    cl_uint numEntries,
                                                    cl_image_format *imageFormats,
                                                    cl_uint *numImageFormats)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::createSampler(const cl::Sampler &sampler, CLSamplerImpl::Ptr *samplerOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::createProgramWithSource(const cl::Program &program,
                                                   const std::string &source,
                                                   CLProgramImpl::Ptr *programOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::createProgramWithIL(const cl::Program &program,
                                               const void *il,
                                               size_t length,
                                               CLProgramImpl::Ptr *programOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::createProgramWithBinary(const cl::Program &program,
                                                   const size_t *lengths,
                                                   const unsigned char **binaries,
                                                   cl_int *binaryStatus,
                                                   CLProgramImpl::Ptr *programOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::createProgramWithBuiltInKernels(const cl::Program &program,
                                                           const char *kernel_names,
                                                           CLProgramImpl::Ptr *programOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::linkProgram(const cl::Program &program,
                                       const cl::DevicePtrs &devices,
                                       const char *options,
                                       const cl::ProgramPtrs &inputPrograms,
                                       cl::Program *notify,
                                       CLProgramImpl::Ptr *programOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::createUserEvent(const cl::Event &event, CLEventImpl::Ptr *eventOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLContextVk::waitForEvents(const cl::EventPtrs &events)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

}  // namespace rx
