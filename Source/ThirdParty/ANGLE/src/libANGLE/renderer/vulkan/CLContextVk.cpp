//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLContextVk.cpp: Implements the class methods for CLContextVk.

#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLCommandQueueVk.h"
#include "libANGLE/renderer/vulkan/CLProgramVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

#include "libANGLE/cl_utils.h"

namespace rx
{

CLContextVk::CLContextVk(const cl::Context &context,
                         const egl::Display *display,
                         const cl::DevicePtrs devicePtrs)
    : CLContextImpl(context),
      vk::Context(GetImplAs<DisplayVk>(display)->getRenderer()),
      mAssociatedDevices(devicePtrs)
{}

CLContextVk::~CLContextVk() = default;

void CLContextVk::handleError(VkResult errorCode,
                              const char *file,
                              const char *function,
                              unsigned int line)
{
    ASSERT(errorCode != VK_SUCCESS);

    CLenum clErrorCode = CL_SUCCESS;
    switch (errorCode)
    {
        case VK_ERROR_TOO_MANY_OBJECTS:
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            clErrorCode = CL_OUT_OF_HOST_MEMORY;
            break;
        default:
            clErrorCode = CL_INVALID_OPERATION;
    }
    ERR() << "Internal Vulkan error (" << errorCode << "): " << VulkanResultString(errorCode)
          << ". " << "CL error (" << clErrorCode << ")";

    if (errorCode == VK_ERROR_DEVICE_LOST)
    {
        handleDeviceLost();
    }
    ANGLE_CL_SET_ERROR(clErrorCode);
}

void CLContextVk::handleDeviceLost() const
{
    // For now just notify the renderer
    getRenderer()->notifyDeviceLost();
}

angle::Result CLContextVk::getDevices(cl::DevicePtrs *devicePtrsOut) const
{
    ASSERT(!mAssociatedDevices.empty());
    *devicePtrsOut = mAssociatedDevices;
    return angle::Result::Continue;
}

angle::Result CLContextVk::createCommandQueue(const cl::CommandQueue &commandQueue,
                                              CLCommandQueueImpl::Ptr *commandQueueOut)
{
    *commandQueueOut = CLCommandQueueVk::Ptr(new (std::nothrow) CLCommandQueueVk(commandQueue));
    if (*commandQueueOut == nullptr)
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_HOST_MEMORY);
    }
    return angle::Result::Continue;
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
    CLProgramVk *programVk = new (std::nothrow) CLProgramVk(program);
    if (programVk == nullptr)
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_HOST_MEMORY);
    }
    ANGLE_TRY(programVk->init());
    *programOut = CLProgramImpl::Ptr(std::move(programVk));

    return angle::Result::Continue;
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
    CLProgramVk *programVk = new (std::nothrow) CLProgramVk(program);
    if (programVk == nullptr)
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_HOST_MEMORY);
    }
    ANGLE_TRY(programVk->init(lengths, binaries, binaryStatus));
    *programOut = CLProgramImpl::Ptr(std::move(programVk));

    return angle::Result::Continue;
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
