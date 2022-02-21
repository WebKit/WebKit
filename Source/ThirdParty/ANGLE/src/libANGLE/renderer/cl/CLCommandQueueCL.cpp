//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLCommandQueueCL.cpp: Implements the class methods for CLCommandQueueCL.

#include "libANGLE/renderer/cl/CLCommandQueueCL.h"

#include "libANGLE/renderer/cl/CLContextCL.h"
#include "libANGLE/renderer/cl/CLEventCL.h"
#include "libANGLE/renderer/cl/CLKernelCL.h"
#include "libANGLE/renderer/cl/CLMemoryCL.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLCommandQueue.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLImage.h"
#include "libANGLE/CLKernel.h"
#include "libANGLE/CLMemory.h"

namespace rx
{

namespace
{

void CheckCreateEvent(cl_int errorCode, cl_event nativeEvent, CLEventImpl::CreateFunc *createFunc)
{
    if (errorCode == CL_SUCCESS && createFunc != nullptr)
    {
        *createFunc = [nativeEvent](const cl::Event &event) {
            return CLEventImpl::Ptr(new CLEventCL(event, nativeEvent));
        };
    }
}

}  // namespace

CLCommandQueueCL::CLCommandQueueCL(const cl::CommandQueue &commandQueue, cl_command_queue native)
    : CLCommandQueueImpl(commandQueue), mNative(native)
{
    if (commandQueue.getProperties().isSet(CL_QUEUE_ON_DEVICE))
    {
        commandQueue.getContext().getImpl<CLContextCL>().mData->mDeviceQueues.emplace(
            commandQueue.getNative());
    }
}

CLCommandQueueCL::~CLCommandQueueCL()
{
    if (mCommandQueue.getProperties().isSet(CL_QUEUE_ON_DEVICE))
    {
        const size_t numRemoved =
            mCommandQueue.getContext().getImpl<CLContextCL>().mData->mDeviceQueues.erase(
                mCommandQueue.getNative());
        ASSERT(numRemoved == 1u);
    }

    if (mNative->getDispatch().clReleaseCommandQueue(mNative) != CL_SUCCESS)
    {
        ERR() << "Error while releasing CL command-queue";
    }
}

cl_int CLCommandQueueCL::setProperty(cl::CommandQueueProperties properties, cl_bool enable)
{
    return mNative->getDispatch().clSetCommandQueueProperty(mNative, properties.get(), enable,
                                                            nullptr);
}

cl_int CLCommandQueueCL::enqueueReadBuffer(const cl::Buffer &buffer,
                                           bool blocking,
                                           size_t offset,
                                           size_t size,
                                           void *ptr,
                                           const cl::EventPtrs &waitEvents,
                                           CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeBuffer                = buffer.getImpl<CLMemoryCL>().getNative();
    const cl_bool block                      = blocking ? CL_TRUE : CL_FALSE;
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode =
        mNative->getDispatch().clEnqueueReadBuffer(mNative, nativeBuffer, block, offset, size, ptr,
                                                   numEvents, nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueWriteBuffer(const cl::Buffer &buffer,
                                            bool blocking,
                                            size_t offset,
                                            size_t size,
                                            const void *ptr,
                                            const cl::EventPtrs &waitEvents,
                                            CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeBuffer                = buffer.getImpl<CLMemoryCL>().getNative();
    const cl_bool block                      = blocking ? CL_TRUE : CL_FALSE;
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode =
        mNative->getDispatch().clEnqueueWriteBuffer(mNative, nativeBuffer, block, offset, size, ptr,
                                                    numEvents, nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueReadBufferRect(const cl::Buffer &buffer,
                                               bool blocking,
                                               const size_t bufferOrigin[3],
                                               const size_t hostOrigin[3],
                                               const size_t region[3],
                                               size_t bufferRowPitch,
                                               size_t bufferSlicePitch,
                                               size_t hostRowPitch,
                                               size_t hostSlicePitch,
                                               void *ptr,
                                               const cl::EventPtrs &waitEvents,
                                               CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeBuffer                = buffer.getImpl<CLMemoryCL>().getNative();
    const cl_bool block                      = blocking ? CL_TRUE : CL_FALSE;
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueReadBufferRect(
        mNative, nativeBuffer, block, bufferOrigin, hostOrigin, region, bufferRowPitch,
        bufferSlicePitch, hostRowPitch, hostSlicePitch, ptr, numEvents, nativeEventsPtr,
        nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueWriteBufferRect(const cl::Buffer &buffer,
                                                bool blocking,
                                                const size_t bufferOrigin[3],
                                                const size_t hostOrigin[3],
                                                const size_t region[3],
                                                size_t bufferRowPitch,
                                                size_t bufferSlicePitch,
                                                size_t hostRowPitch,
                                                size_t hostSlicePitch,
                                                const void *ptr,
                                                const cl::EventPtrs &waitEvents,
                                                CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeBuffer                = buffer.getImpl<CLMemoryCL>().getNative();
    const cl_bool block                      = blocking ? CL_TRUE : CL_FALSE;
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueWriteBufferRect(
        mNative, nativeBuffer, block, bufferOrigin, hostOrigin, region, bufferRowPitch,
        bufferSlicePitch, hostRowPitch, hostSlicePitch, ptr, numEvents, nativeEventsPtr,
        nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueCopyBuffer(const cl::Buffer &srcBuffer,
                                           const cl::Buffer &dstBuffer,
                                           size_t srcOffset,
                                           size_t dstOffset,
                                           size_t size,
                                           const cl::EventPtrs &waitEvents,
                                           CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeSrc                   = srcBuffer.getImpl<CLMemoryCL>().getNative();
    const cl_mem nativeDst                   = dstBuffer.getImpl<CLMemoryCL>().getNative();
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueCopyBuffer(
        mNative, nativeSrc, nativeDst, srcOffset, dstOffset, size, numEvents, nativeEventsPtr,
        nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueCopyBufferRect(const cl::Buffer &srcBuffer,
                                               const cl::Buffer &dstBuffer,
                                               const size_t srcOrigin[3],
                                               const size_t dstOrigin[3],
                                               const size_t region[3],
                                               size_t srcRowPitch,
                                               size_t srcSlicePitch,
                                               size_t dstRowPitch,
                                               size_t dstSlicePitch,
                                               const cl::EventPtrs &waitEvents,
                                               CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeSrc                   = srcBuffer.getImpl<CLMemoryCL>().getNative();
    const cl_mem nativeDst                   = dstBuffer.getImpl<CLMemoryCL>().getNative();
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueCopyBufferRect(
        mNative, nativeSrc, nativeDst, srcOrigin, dstOrigin, region, srcRowPitch, srcSlicePitch,
        dstRowPitch, dstSlicePitch, numEvents, nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueFillBuffer(const cl::Buffer &buffer,
                                           const void *pattern,
                                           size_t patternSize,
                                           size_t offset,
                                           size_t size,
                                           const cl::EventPtrs &waitEvents,
                                           CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeBuffer                = buffer.getImpl<CLMemoryCL>().getNative();
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueFillBuffer(
        mNative, nativeBuffer, pattern, patternSize, offset, size, numEvents, nativeEventsPtr,
        nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

void *CLCommandQueueCL::enqueueMapBuffer(const cl::Buffer &buffer,
                                         bool blocking,
                                         cl::MapFlags mapFlags,
                                         size_t offset,
                                         size_t size,
                                         const cl::EventPtrs &waitEvents,
                                         CLEventImpl::CreateFunc *eventCreateFunc,
                                         cl_int &errorCode)
{
    const cl_mem nativeBuffer                = buffer.getImpl<CLMemoryCL>().getNative();
    const cl_bool block                      = blocking ? CL_TRUE : CL_FALSE;
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    void *const map = mNative->getDispatch().clEnqueueMapBuffer(
        mNative, nativeBuffer, block, mapFlags.get(), offset, size, numEvents, nativeEventsPtr,
        nativeEventPtr, &errorCode);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return map;
}

cl_int CLCommandQueueCL::enqueueReadImage(const cl::Image &image,
                                          bool blocking,
                                          const size_t origin[3],
                                          const size_t region[3],
                                          size_t rowPitch,
                                          size_t slicePitch,
                                          void *ptr,
                                          const cl::EventPtrs &waitEvents,
                                          CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeImage                 = image.getImpl<CLMemoryCL>().getNative();
    const cl_bool block                      = blocking ? CL_TRUE : CL_FALSE;
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueReadImage(
        mNative, nativeImage, block, origin, region, rowPitch, slicePitch, ptr, numEvents,
        nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueWriteImage(const cl::Image &image,
                                           bool blocking,
                                           const size_t origin[3],
                                           const size_t region[3],
                                           size_t inputRowPitch,
                                           size_t inputSlicePitch,
                                           const void *ptr,
                                           const cl::EventPtrs &waitEvents,
                                           CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeImage                 = image.getImpl<CLMemoryCL>().getNative();
    const cl_bool block                      = blocking ? CL_TRUE : CL_FALSE;
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueWriteImage(
        mNative, nativeImage, block, origin, region, inputRowPitch, inputSlicePitch, ptr, numEvents,
        nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueCopyImage(const cl::Image &srcImage,
                                          const cl::Image &dstImage,
                                          const size_t srcOrigin[3],
                                          const size_t dstOrigin[3],
                                          const size_t region[3],
                                          const cl::EventPtrs &waitEvents,
                                          CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeSrc                   = srcImage.getImpl<CLMemoryCL>().getNative();
    const cl_mem nativeDst                   = dstImage.getImpl<CLMemoryCL>().getNative();
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueCopyImage(
        mNative, nativeSrc, nativeDst, srcOrigin, dstOrigin, region, numEvents, nativeEventsPtr,
        nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueFillImage(const cl::Image &image,
                                          const void *fillColor,
                                          const size_t origin[3],
                                          const size_t region[3],
                                          const cl::EventPtrs &waitEvents,
                                          CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeImage                 = image.getImpl<CLMemoryCL>().getNative();
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode =
        mNative->getDispatch().clEnqueueFillImage(mNative, nativeImage, fillColor, origin, region,
                                                  numEvents, nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueCopyImageToBuffer(const cl::Image &srcImage,
                                                  const cl::Buffer &dstBuffer,
                                                  const size_t srcOrigin[3],
                                                  const size_t region[3],
                                                  size_t dstOffset,
                                                  const cl::EventPtrs &waitEvents,
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeSrc                   = srcImage.getImpl<CLMemoryCL>().getNative();
    const cl_mem nativeDst                   = dstBuffer.getImpl<CLMemoryCL>().getNative();
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueCopyImageToBuffer(
        mNative, nativeSrc, nativeDst, srcOrigin, region, dstOffset, numEvents, nativeEventsPtr,
        nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueCopyBufferToImage(const cl::Buffer &srcBuffer,
                                                  const cl::Image &dstImage,
                                                  size_t srcOffset,
                                                  const size_t dstOrigin[3],
                                                  const size_t region[3],
                                                  const cl::EventPtrs &waitEvents,
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeSrc                   = srcBuffer.getImpl<CLMemoryCL>().getNative();
    const cl_mem nativeDst                   = dstImage.getImpl<CLMemoryCL>().getNative();
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueCopyBufferToImage(
        mNative, nativeSrc, nativeDst, srcOffset, dstOrigin, region, numEvents, nativeEventsPtr,
        nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

void *CLCommandQueueCL::enqueueMapImage(const cl::Image &image,
                                        bool blocking,
                                        cl::MapFlags mapFlags,
                                        const size_t origin[3],
                                        const size_t region[3],
                                        size_t *imageRowPitch,
                                        size_t *imageSlicePitch,
                                        const cl::EventPtrs &waitEvents,
                                        CLEventImpl::CreateFunc *eventCreateFunc,
                                        cl_int &errorCode)
{
    const cl_mem nativeImage                 = image.getImpl<CLMemoryCL>().getNative();
    const cl_bool block                      = blocking ? CL_TRUE : CL_FALSE;
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    void *const map = mNative->getDispatch().clEnqueueMapImage(
        mNative, nativeImage, block, mapFlags.get(), origin, region, imageRowPitch, imageSlicePitch,
        numEvents, nativeEventsPtr, nativeEventPtr, &errorCode);

    // TODO(jplate) Remove workaround after bug is fixed http://anglebug.com/6066
    if (imageSlicePitch != nullptr && (image.getType() == cl::MemObjectType::Image1D ||
                                       image.getType() == cl::MemObjectType::Image1D_Buffer ||
                                       image.getType() == cl::MemObjectType::Image2D))
    {
        *imageSlicePitch = 0u;
    }

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return map;
}

cl_int CLCommandQueueCL::enqueueUnmapMemObject(const cl::Memory &memory,
                                               void *mappedPtr,
                                               const cl::EventPtrs &waitEvents,
                                               CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_mem nativeMemory                = memory.getImpl<CLMemoryCL>().getNative();
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueUnmapMemObject(
        mNative, nativeMemory, mappedPtr, numEvents, nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueMigrateMemObjects(const cl::MemoryPtrs &memObjects,
                                                  cl::MemMigrationFlags flags,
                                                  const cl::EventPtrs &waitEvents,
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::vector<cl_mem> nativeMemories;
    nativeMemories.reserve(memObjects.size());
    for (const cl::MemoryPtr &memory : memObjects)
    {
        nativeMemories.emplace_back(memory->getImpl<CLMemoryCL>().getNative());
    }
    const cl_uint numMemories                = static_cast<cl_uint>(nativeMemories.size());
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueMigrateMemObjects(
        mNative, numMemories, nativeMemories.data(), flags.get(), numEvents, nativeEventsPtr,
        nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueNDRangeKernel(const cl::Kernel &kernel,
                                              cl_uint workDim,
                                              const size_t *globalWorkOffset,
                                              const size_t *globalWorkSize,
                                              const size_t *localWorkSize,
                                              const cl::EventPtrs &waitEvents,
                                              CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_kernel nativeKernel             = kernel.getImpl<CLKernelCL>().getNative();
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueNDRangeKernel(
        mNative, nativeKernel, workDim, globalWorkOffset, globalWorkSize, localWorkSize, numEvents,
        nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueTask(const cl::Kernel &kernel,
                                     const cl::EventPtrs &waitEvents,
                                     CLEventImpl::CreateFunc *eventCreateFunc)
{
    const cl_kernel nativeKernel             = kernel.getImpl<CLKernelCL>().getNative();
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueTask(mNative, nativeKernel, numEvents,
                                                                  nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueNativeKernel(cl::UserFunc userFunc,
                                             void *args,
                                             size_t cbArgs,
                                             const cl::BufferPtrs &buffers,
                                             const std::vector<size_t> bufferPtrOffsets,
                                             const cl::EventPtrs &waitEvents,
                                             CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::vector<unsigned char> funcArgs;
    std::vector<const void *> locs;
    if (!bufferPtrOffsets.empty())
    {
        // If argument memory block contains buffers, make a copy.
        funcArgs.resize(cbArgs);
        std::memcpy(funcArgs.data(), args, cbArgs);

        locs.reserve(bufferPtrOffsets.size());
        for (size_t offset : bufferPtrOffsets)
        {
            // Fetch location of buffer in copied function argument memory block.
            void *const loc = &funcArgs[offset];
            locs.emplace_back(loc);

            // Cast cl::Buffer to native cl_mem object in place.
            cl::Buffer *const buffer         = *reinterpret_cast<cl::Buffer **>(loc);
            *reinterpret_cast<cl_mem *>(loc) = buffer->getImpl<CLMemoryCL>().getNative();
        }

        // Use copied argument memory block.
        args = funcArgs.data();
    }

    std::vector<cl_mem> nativeBuffers;
    nativeBuffers.reserve(buffers.size());
    for (const cl::BufferPtr &buffer : buffers)
    {
        nativeBuffers.emplace_back(buffer->getImpl<CLMemoryCL>().getNative());
    }
    const cl_uint numBuffers             = static_cast<cl_uint>(nativeBuffers.size());
    const cl_mem *const nativeBuffersPtr = nativeBuffers.empty() ? nullptr : nativeBuffers.data();
    const void **const locsPtr           = locs.empty() ? nullptr : locs.data();

    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueNativeKernel(
        mNative, userFunc, args, cbArgs, numBuffers, nativeBuffersPtr, locsPtr, numEvents,
        nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueMarkerWithWaitList(const cl::EventPtrs &waitEvents,
                                                   CLEventImpl::CreateFunc *eventCreateFunc)
{
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueMarkerWithWaitList(
        mNative, numEvents, nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueMarker(CLEventImpl::CreateFunc &eventCreateFunc)
{
    cl_event nativeEvent = nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueMarker(mNative, &nativeEvent);

    if (errorCode == CL_SUCCESS)
    {
        eventCreateFunc = [nativeEvent](const cl::Event &event) {
            return CLEventImpl::Ptr(new CLEventCL(event, nativeEvent));
        };
    }
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueWaitForEvents(const cl::EventPtrs &events)
{
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(events);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());

    return mNative->getDispatch().clEnqueueWaitForEvents(mNative, numEvents, nativeEvents.data());
}

cl_int CLCommandQueueCL::enqueueBarrierWithWaitList(const cl::EventPtrs &waitEvents,
                                                    CLEventImpl::CreateFunc *eventCreateFunc)
{
    const std::vector<cl_event> nativeEvents = CLEventCL::Cast(waitEvents);
    const cl_uint numEvents                  = static_cast<cl_uint>(nativeEvents.size());
    const cl_event *const nativeEventsPtr    = nativeEvents.empty() ? nullptr : nativeEvents.data();
    cl_event nativeEvent                     = nullptr;
    cl_event *const nativeEventPtr           = eventCreateFunc != nullptr ? &nativeEvent : nullptr;

    const cl_int errorCode = mNative->getDispatch().clEnqueueBarrierWithWaitList(
        mNative, numEvents, nativeEventsPtr, nativeEventPtr);

    CheckCreateEvent(errorCode, nativeEvent, eventCreateFunc);
    return errorCode;
}

cl_int CLCommandQueueCL::enqueueBarrier()
{
    return mNative->getDispatch().clEnqueueBarrier(mNative);
}

cl_int CLCommandQueueCL::flush()
{
    return mNative->getDispatch().clFlush(mNative);
}

cl_int CLCommandQueueCL::finish()
{
    return mNative->getDispatch().clFinish(mNative);
}

}  // namespace rx
