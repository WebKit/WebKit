//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLContextCL.cpp: Implements the class methods for CLContextCL.

#include "libANGLE/renderer/cl/CLContextCL.h"

#include "libANGLE/renderer/cl/CLCommandQueueCL.h"
#include "libANGLE/renderer/cl/CLDeviceCL.h"
#include "libANGLE/renderer/cl/CLEventCL.h"
#include "libANGLE/renderer/cl/CLMemoryCL.h"
#include "libANGLE/renderer/cl/CLPlatformCL.h"
#include "libANGLE/renderer/cl/CLProgramCL.h"
#include "libANGLE/renderer/cl/CLSamplerCL.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLCommandQueue.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLDevice.h"
#include "libANGLE/CLEvent.h"
#include "libANGLE/CLImage.h"
#include "libANGLE/CLMemory.h"
#include "libANGLE/CLPlatform.h"
#include "libANGLE/CLProgram.h"
#include "libANGLE/CLSampler.h"
#include "libANGLE/cl_utils.h"

namespace rx
{

CLContextCL::CLContextCL(const cl::Context &context, cl_context native)
    : CLContextImpl(context), mNative(native)
{}

CLContextCL::~CLContextCL()
{
    if (mNative->getDispatch().clReleaseContext(mNative) != CL_SUCCESS)
    {
        ERR() << "Error while releasing CL context";
    }
}

cl::DevicePtrs CLContextCL::getDevices(cl_int &errorCode) const
{
    size_t valueSize = 0u;
    errorCode = mNative->getDispatch().clGetContextInfo(mNative, CL_CONTEXT_DEVICES, 0u, nullptr,
                                                        &valueSize);
    if (errorCode == CL_SUCCESS && (valueSize % sizeof(cl_device_id)) == 0u)
    {
        std::vector<cl_device_id> nativeDevices(valueSize / sizeof(cl_device_id), nullptr);
        errorCode = mNative->getDispatch().clGetContextInfo(mNative, CL_CONTEXT_DEVICES, valueSize,
                                                            nativeDevices.data(), nullptr);
        if (errorCode == CL_SUCCESS)
        {
            const cl::DevicePtrs &platformDevices = mContext.getPlatform().getDevices();
            cl::DevicePtrs devices;
            devices.reserve(nativeDevices.size());
            for (cl_device_id nativeDevice : nativeDevices)
            {
                auto it = platformDevices.cbegin();
                while (it != platformDevices.cend() &&
                       (*it)->getImpl<CLDeviceCL>().getNative() != nativeDevice)
                {
                    ++it;
                }
                if (it != platformDevices.cend())
                {
                    devices.emplace_back(it->get());
                }
                else
                {
                    ASSERT(false);
                    errorCode = CL_INVALID_DEVICE;
                    ERR() << "Device not found in platform list";
                    return cl::DevicePtrs{};
                }
            }
            return devices;
        }
    }
    return cl::DevicePtrs{};
}

CLCommandQueueImpl::Ptr CLContextCL::createCommandQueue(const cl::CommandQueue &commandQueue,
                                                        cl_int &errorCode)
{
    const cl::Device &device        = commandQueue.getDevice();
    const cl_device_id nativeDevice = device.getImpl<CLDeviceCL>().getNative();
    cl_command_queue nativeQueue    = nullptr;
    if (!device.isVersionOrNewer(2u, 0u))
    {
        nativeQueue = mNative->getDispatch().clCreateCommandQueue(
            mNative, nativeDevice, commandQueue.getProperties().get(), &errorCode);
    }
    else
    {
        const cl_queue_properties propArray[] = {
            CL_QUEUE_PROPERTIES, commandQueue.getProperties().get(),
            commandQueue.hasSize() ? CL_QUEUE_SIZE : 0u, commandQueue.getSize(), 0u};
        nativeQueue = mNative->getDispatch().clCreateCommandQueueWithProperties(
            mNative, nativeDevice, propArray, &errorCode);
    }
    return CLCommandQueueImpl::Ptr(
        nativeQueue != nullptr ? new CLCommandQueueCL(commandQueue, nativeQueue) : nullptr);
}

CLMemoryImpl::Ptr CLContextCL::createBuffer(const cl::Buffer &buffer,
                                            size_t size,
                                            void *hostPtr,
                                            cl_int &errorCode)
{
    cl_mem nativeBuffer = nullptr;
    if (buffer.getProperties().empty())
    {
        nativeBuffer = mNative->getDispatch().clCreateBuffer(mNative, buffer.getFlags().get(), size,
                                                             hostPtr, &errorCode);
    }
    else
    {
        nativeBuffer = mNative->getDispatch().clCreateBufferWithProperties(
            mNative, buffer.getProperties().data(), buffer.getFlags().get(), size, hostPtr,
            &errorCode);
    }
    return CLMemoryImpl::Ptr(nativeBuffer != nullptr ? new CLMemoryCL(buffer, nativeBuffer)
                                                     : nullptr);
}

CLMemoryImpl::Ptr CLContextCL::createImage(const cl::Image &image,
                                           cl::MemFlags flags,
                                           const cl_image_format &format,
                                           const cl::ImageDescriptor &desc,
                                           void *hostPtr,
                                           cl_int &errorCode)
{
    cl_mem nativeImage = nullptr;

    if (mContext.getPlatform().isVersionOrNewer(1u, 2u))
    {
        const cl_mem_object_type nativeType = cl::ToCLenum(desc.type);
        const cl_mem nativeParent =
            image.getParent() ? image.getParent()->getImpl<CLMemoryCL>().getNative() : nullptr;
        const cl_image_desc nativeDesc = {
            nativeType,    desc.width,      desc.height,       desc.depth,      desc.arraySize,
            desc.rowPitch, desc.slicePitch, desc.numMipLevels, desc.numSamples, {nativeParent}};

        if (image.getProperties().empty())
        {
            nativeImage = mNative->getDispatch().clCreateImage(mNative, flags.get(), &format,
                                                               &nativeDesc, hostPtr, &errorCode);
        }
        else
        {
            nativeImage = mNative->getDispatch().clCreateImageWithProperties(
                mNative, image.getProperties().data(), flags.get(), &format, &nativeDesc, hostPtr,
                &errorCode);
        }
    }
    else
    {
        switch (desc.type)
        {
            case cl::MemObjectType::Image2D:
                nativeImage = mNative->getDispatch().clCreateImage2D(
                    mNative, flags.get(), &format, desc.width, desc.height, desc.rowPitch, hostPtr,
                    &errorCode);
                break;
            case cl::MemObjectType::Image3D:
                nativeImage = mNative->getDispatch().clCreateImage3D(
                    mNative, flags.get(), &format, desc.width, desc.height, desc.depth,
                    desc.rowPitch, desc.slicePitch, hostPtr, &errorCode);
                break;
            default:
                ASSERT(false);
                ERR() << "Failed to create unsupported image type";
                break;
        }
    }

    return CLMemoryImpl::Ptr(nativeImage != nullptr ? new CLMemoryCL(image, nativeImage) : nullptr);
}

cl_int CLContextCL::getSupportedImageFormats(cl::MemFlags flags,
                                             cl::MemObjectType imageType,
                                             cl_uint numEntries,
                                             cl_image_format *imageFormats,
                                             cl_uint *numImageFormats)
{
    // Fetch available image formats for given flags and image type.
    cl_uint numFormats = 0u;
    ANGLE_CL_TRY(mNative->getDispatch().clGetSupportedImageFormats(
        mNative, flags.get(), cl::ToCLenum(imageType), 0u, nullptr, &numFormats));
    std::vector<cl_image_format> formats(numFormats);
    ANGLE_CL_TRY(mNative->getDispatch().clGetSupportedImageFormats(
        mNative, flags.get(), cl::ToCLenum(imageType), numFormats, formats.data(), nullptr));

    // Filter out formats which are not supported by front end.
    const CLPlatformImpl::Info &info = mContext.getPlatform().getInfo();
    std::vector<cl_image_format> supportedFormats;
    supportedFormats.reserve(formats.size());
    std::copy_if(
        formats.cbegin(), formats.cend(), std::back_inserter(supportedFormats),
        [&](const cl_image_format &format) { return cl::IsValidImageFormat(&format, info); });

    if (imageFormats != nullptr)
    {
        auto formatIt = supportedFormats.cbegin();
        while (numEntries-- != 0u && formatIt != supportedFormats.cend())
        {
            *imageFormats++ = *formatIt++;
        }
    }
    if (numImageFormats != nullptr)
    {
        *numImageFormats = static_cast<cl_uint>(supportedFormats.size());
    }
    return CL_SUCCESS;
}

CLSamplerImpl::Ptr CLContextCL::createSampler(const cl::Sampler &sampler, cl_int &errorCode)
{
    cl_sampler nativeSampler = nullptr;
    if (!mContext.getPlatform().isVersionOrNewer(2u, 0u))
    {
        nativeSampler = mNative->getDispatch().clCreateSampler(
            mNative, sampler.getNormalizedCoords(), cl::ToCLenum(sampler.getAddressingMode()),
            cl::ToCLenum(sampler.getFilterMode()), &errorCode);
    }
    else if (!sampler.getProperties().empty())
    {
        nativeSampler = mNative->getDispatch().clCreateSamplerWithProperties(
            mNative, sampler.getProperties().data(), &errorCode);
    }
    else
    {
        const cl_sampler_properties propArray[] = {CL_SAMPLER_NORMALIZED_COORDS,
                                                   sampler.getNormalizedCoords(),
                                                   CL_SAMPLER_ADDRESSING_MODE,
                                                   cl::ToCLenum(sampler.getAddressingMode()),
                                                   CL_SAMPLER_FILTER_MODE,
                                                   cl::ToCLenum(sampler.getFilterMode()),
                                                   0u};
        nativeSampler =
            mNative->getDispatch().clCreateSamplerWithProperties(mNative, propArray, &errorCode);
    }
    return CLSamplerImpl::Ptr(nativeSampler != nullptr ? new CLSamplerCL(sampler, nativeSampler)
                                                       : nullptr);
}

CLProgramImpl::Ptr CLContextCL::createProgramWithSource(const cl::Program &program,
                                                        const std::string &source,
                                                        cl_int &errorCode)
{
    const char *sourceStr          = source.c_str();
    const size_t length            = source.length();
    const cl_program nativeProgram = mNative->getDispatch().clCreateProgramWithSource(
        mNative, 1u, &sourceStr, &length, &errorCode);
    return CLProgramImpl::Ptr(nativeProgram != nullptr ? new CLProgramCL(program, nativeProgram)
                                                       : nullptr);
}

CLProgramImpl::Ptr CLContextCL::createProgramWithIL(const cl::Program &program,
                                                    const void *il,
                                                    size_t length,
                                                    cl_int &errorCode)
{
    const cl_program nativeProgram =
        mNative->getDispatch().clCreateProgramWithIL(mNative, il, length, &errorCode);
    return CLProgramImpl::Ptr(nativeProgram != nullptr ? new CLProgramCL(program, nativeProgram)
                                                       : nullptr);
}

CLProgramImpl::Ptr CLContextCL::createProgramWithBinary(const cl::Program &program,
                                                        const size_t *lengths,
                                                        const unsigned char **binaries,
                                                        cl_int *binaryStatus,
                                                        cl_int &errorCode)
{
    std::vector<cl_device_id> nativeDevices;
    for (const cl::DevicePtr &device : program.getDevices())
    {
        nativeDevices.emplace_back(device->getImpl<CLDeviceCL>().getNative());
    }

    cl_program nativeProgram = mNative->getDispatch().clCreateProgramWithBinary(
        mNative, static_cast<cl_uint>(nativeDevices.size()), nativeDevices.data(), lengths,
        binaries, binaryStatus, &errorCode);

    return CLProgramImpl::Ptr(nativeProgram != nullptr ? new CLProgramCL(program, nativeProgram)
                                                       : nullptr);
}

CLProgramImpl::Ptr CLContextCL::createProgramWithBuiltInKernels(const cl::Program &program,
                                                                const char *kernel_names,
                                                                cl_int &errorCode)
{
    std::vector<cl_device_id> nativeDevices;
    for (const cl::DevicePtr &device : program.getDevices())
    {
        nativeDevices.emplace_back(device->getImpl<CLDeviceCL>().getNative());
    }
    const cl_program nativeProgram = mNative->getDispatch().clCreateProgramWithBuiltInKernels(
        mNative, static_cast<cl_uint>(nativeDevices.size()), nativeDevices.data(), kernel_names,
        &errorCode);
    return CLProgramImpl::Ptr(nativeProgram != nullptr ? new CLProgramCL(program, nativeProgram)
                                                       : nullptr);
}

CLProgramImpl::Ptr CLContextCL::linkProgram(const cl::Program &program,
                                            const cl::DevicePtrs &devices,
                                            const char *options,
                                            const cl::ProgramPtrs &inputPrograms,
                                            cl::Program *notify,
                                            cl_int &errorCode)
{
    std::vector<cl_device_id> nativeDevices;
    for (const cl::DevicePtr &device : devices)
    {
        nativeDevices.emplace_back(device->getImpl<CLDeviceCL>().getNative());
    }
    const cl_uint numDevices = static_cast<cl_uint>(nativeDevices.size());
    const cl_device_id *const nativeDevicesPtr =
        !nativeDevices.empty() ? nativeDevices.data() : nullptr;

    std::vector<cl_program> nativePrograms;
    for (const cl::ProgramPtr &inputProgram : inputPrograms)
    {
        nativePrograms.emplace_back(inputProgram->getImpl<CLProgramCL>().getNative());
    }
    const cl_uint numInputHeaders = static_cast<cl_uint>(nativePrograms.size());

    const cl::ProgramCB callback   = notify != nullptr ? CLProgramCL::Callback : nullptr;
    const cl_program nativeProgram = mNative->getDispatch().clLinkProgram(
        mNative, numDevices, nativeDevicesPtr, options, numInputHeaders, nativePrograms.data(),
        callback, notify, &errorCode);
    return CLProgramImpl::Ptr(nativeProgram != nullptr ? new CLProgramCL(program, nativeProgram)
                                                       : nullptr);
}

CLEventImpl::Ptr CLContextCL::createUserEvent(const cl::Event &event, cl_int &errorCode)
{
    const cl_event nativeEvent = mNative->getDispatch().clCreateUserEvent(mNative, &errorCode);
    return CLEventImpl::Ptr(nativeEvent != nullptr ? new CLEventCL(event, nativeEvent) : nullptr);
}

cl_int CLContextCL::waitForEvents(const cl::EventPtrs &events)
{
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(events);
    return mNative->getDispatch().clWaitForEvents(static_cast<cl_uint>(nativeEvents.size()),
                                                  nativeEvents.data());
}

}  // namespace rx
