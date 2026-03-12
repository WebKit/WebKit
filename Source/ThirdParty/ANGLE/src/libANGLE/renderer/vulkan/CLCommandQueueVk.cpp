//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLCommandQueueVk.cpp: Implements the class methods for CLCommandQueueVk.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "common/PackedCLEnums_autogen.h"
#include "common/SimpleMutex.h"
#include "common/log_utils.h"
#include "common/system_utils.h"

#include "libANGLE/renderer/vulkan/CLCommandQueueVk.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLDeviceVk.h"
#include "libANGLE/renderer/vulkan/CLEventVk.h"
#include "libANGLE/renderer/vulkan/CLKernelVk.h"
#include "libANGLE/renderer/vulkan/CLMemoryVk.h"
#include "libANGLE/renderer/vulkan/CLProgramVk.h"
#include "libANGLE/renderer/vulkan/CLSamplerVk.h"
#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/clspv_utils.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_cl_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

#include "libANGLE/renderer/serial_utils.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLCommandQueue.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLEvent.h"
#include "libANGLE/CLImage.h"
#include "libANGLE/CLKernel.h"
#include "libANGLE/CLSampler.h"
#include "libANGLE/Error.h"
#include "libANGLE/cl_types.h"
#include "libANGLE/cl_utils.h"

#include "spirv/unified1/NonSemanticClspvReflection.h"
#include "vulkan/vulkan_core.h"

#include <chrono>

namespace rx
{

namespace
{
// Given an image and rect region to copy in to a buffer, calculate the VKBufferImageCopy struct to
// be using in VkCmd's.
VkBufferImageCopy CalculateBufferImageCopyRegion(const size_t bufferOffset,
                                                 const uint32_t rowPitch,
                                                 const uint32_t slicePitch,
                                                 const cl::Offset &origin,
                                                 const cl::Extents &region,
                                                 CLImageVk *imageVk)
{
    VkBufferImageCopy copyRegion{
        .bufferOffset = bufferOffset,
        .bufferRowLength =
            rowPitch == 0 ? 0 : rowPitch / static_cast<uint32_t>(imageVk->getElementSize()),
        .bufferImageHeight = rowPitch == 0 ? 0 : slicePitch / rowPitch,
        .imageSubresource = imageVk->getSubresourceLayersForCopy(origin, region, imageVk->getType(),
                                                                 ImageCopyWith::Buffer),
        .imageOffset      = cl_vk::GetOffset(origin),
        .imageExtent      = cl_vk::GetExtent(region)};
    ASSERT((copyRegion.bufferRowLength == 0 && copyRegion.bufferImageHeight == 0) ||
           (copyRegion.bufferRowLength >= region.width &&
            copyRegion.bufferImageHeight >= region.height));

    return copyRegion;
}

constexpr size_t kTimeoutInMS            = 10000;
constexpr size_t kSleepInMS              = 500;
constexpr size_t kTimeoutCheckIterations = kTimeoutInMS / kSleepInMS;

DispatchWorkThread::DispatchWorkThread(CLCommandQueueVk *commandQueue)
    : mCommandQueue(commandQueue),
      mIsTerminating(false),
      mQueueSerials(kFixedQueueLimit),
      mQueueSerialIndex(kInvalidQueueSerialIndex)
{}

DispatchWorkThread::~DispatchWorkThread()
{
    ASSERT(mIsTerminating);
}

angle::Result DispatchWorkThread::init()
{
    mQueueSerialIndex = mCommandQueue->getQueueSerialIndex();
    ASSERT(mQueueSerialIndex != kInvalidQueueSerialIndex);

    mWorkerThread = std::thread(&DispatchWorkThread::finishLoop, this);

    return angle::Result::Continue;
}

void DispatchWorkThread::terminate()
{
    // Terminate the background thread
    {
        std::unique_lock<std::mutex> lock(mThreadMutex);
        mIsTerminating = true;
    }
    mHasWorkSubmitted.notify_all();
    if (mWorkerThread.joinable())
    {
        mWorkerThread.join();
    }
}

angle::Result DispatchWorkThread::notify(QueueSerial queueSerial)
{
    ASSERT(queueSerial.getIndex() == mQueueSerialIndex);

    // QueueSerials are always received in order, its either same or greater than last one
    std::unique_lock<std::mutex> ul(mThreadMutex);
    if (!mQueueSerials.empty())
    {
        QueueSerial &lastSerial = mQueueSerials.back();
        ASSERT(queueSerial >= lastSerial);
        if (queueSerial == lastSerial)
        {
            return angle::Result::Continue;
        }
    }

    // if the queue is full, it might be that device is lost, check for timeout
    size_t numIterations = 0;
    while (mQueueSerials.full() && numIterations < kTimeoutCheckIterations)
    {
        mHasEmptySlot.wait_for(ul, std::chrono::milliseconds(kSleepInMS),
                               [this]() { return !mQueueSerials.full(); });
        numIterations++;
    }
    if (numIterations == kTimeoutCheckIterations)
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    mQueueSerials.push(queueSerial);
    mHasWorkSubmitted.notify_one();

    return angle::Result::Continue;
}

angle::Result DispatchWorkThread::finishLoop()
{
    angle::SetCurrentThreadName("ANGLE-CL-CQD");

    while (true)
    {
        std::unique_lock<std::mutex> ul(mThreadMutex);
        mHasWorkSubmitted.wait(ul, [this]() { return !mQueueSerials.empty() || mIsTerminating; });

        while (!mQueueSerials.empty())
        {
            QueueSerial queueSerial = mQueueSerials.front();
            mQueueSerials.pop();
            mHasEmptySlot.notify_one();
            ul.unlock();
            // finish the work associated with the queue serial
            ANGLE_TRY(mCommandQueue->finishQueueSerial(queueSerial));
            ul.lock();
        }

        if (mIsTerminating)
        {
            break;
        }
    }
    return angle::Result::Continue;
}

egl::ContextPriority convertClToEglPriority(cl_queue_priority_khr priority)
{
    switch (priority)
    {
        case CL_QUEUE_PRIORITY_HIGH_KHR:
            return egl::ContextPriority::High;
        case CL_QUEUE_PRIORITY_MED_KHR:
            return egl::ContextPriority::Medium;
        case CL_QUEUE_PRIORITY_LOW_KHR:
            return egl::ContextPriority::Low;
        default:
            UNREACHABLE();
            return egl::ContextPriority::Medium;
    }
}

class HostTransferConfigVisitor
{
  public:
    HostTransferConfigVisitor(CLBufferVk &buffer) : bufferVk(buffer) {}

    angle::Result operator()(const HostReadTransferConfig &transferConfig)
    {
        switch (transferConfig.getType())
        {
            case CL_COMMAND_READ_BUFFER:
                ANGLE_TRY(bufferVk.syncHost(CLBufferVk::SyncHostDirection::ToHost));
                break;
            case CL_COMMAND_READ_BUFFER_RECT:
                ANGLE_TRY(bufferVk.syncHost(CLBufferVk::SyncHostDirection::ToHost,
                                            transferConfig.getHostRect()));
                break;
            case CL_COMMAND_READ_IMAGE:
                ANGLE_TRY(bufferVk.syncHost(CLBufferVk::SyncHostDirection::ToHost,
                                            transferConfig.getHostRect()));
                break;
            default:
                UNREACHABLE();
                break;
        }
        return angle::Result::Continue;
    }

    angle::Result operator()(const HostWriteTransferConfig &transferConfig)
    {
        switch (transferConfig.getType())
        {

            case CL_COMMAND_WRITE_BUFFER:
            case CL_COMMAND_WRITE_BUFFER_RECT:
            case CL_COMMAND_FILL_BUFFER:
            case CL_COMMAND_WRITE_IMAGE:
                // Nothing to do here
                break;
            default:
                UNIMPLEMENTED();
                break;
        }
        return angle::Result::Continue;
    }

  private:
    CLBufferVk &bufferVk;
};

}  // namespace

CLCommandQueueVk::CLCommandQueueVk(const cl::CommandQueue &commandQueue)
    : CLCommandQueueImpl(commandQueue),
      mContext(&commandQueue.getContext().getImpl<CLContextVk>()),
      mDevice(&commandQueue.getDevice().getImpl<CLDeviceVk>()),
      mPrintfBuffer(nullptr),
      mComputePassCommands(nullptr),
      mCommandState(mContext->getRenderer(),
                    vk::ProtectionType::Unprotected,
                    convertClToEglPriority(mCommandQueue.getPriority())),
      mQueueSerialIndex(kInvalidQueueSerialIndex),
      mNeedPrintfHandling(false),
      mFinishHandler(this)
{}

angle::Result CLCommandQueueVk::init()
{
    vk::Renderer *renderer = mContext->getRenderer();
    ASSERT(renderer);

    ANGLE_CL_IMPL_TRY_ERROR(vk::OutsideRenderPassCommandBuffer::InitializeCommandPool(
                                mContext, &mCommandPool.outsideRenderPassPool,
                                renderer->getQueueFamilyIndex(), getProtectionType()),
                            CL_OUT_OF_RESOURCES);

    ANGLE_CL_IMPL_TRY_ERROR(
        mContext->getRenderer()->getOutsideRenderPassCommandBufferHelper(
            mContext, &mCommandPool.outsideRenderPassPool, &mComputePassCommands),
        CL_OUT_OF_RESOURCES);

    // Generate initial QueueSerial for command buffer helper
    ANGLE_CL_IMPL_TRY_ERROR(mContext->getRenderer()->allocateQueueSerialIndex(&mQueueSerialIndex),
                            CL_OUT_OF_RESOURCES);
    // and set an initial queue serial for the compute pass commands
    mComputePassCommands->setQueueSerial(
        mQueueSerialIndex, mContext->getRenderer()->generateQueueSerial(mQueueSerialIndex));

    // Initialize serials to be valid but appear submitted and finished.
    mLastFlushedQueueSerial   = QueueSerial(mQueueSerialIndex, Serial());
    mLastSubmittedQueueSerial = mLastFlushedQueueSerial;

    ANGLE_TRY(mFinishHandler.init());

    return angle::Result::Continue;
}

CLCommandQueueVk::~CLCommandQueueVk()
{
    VkDevice vkDevice = mContext->getDevice();

    mCommandState.destroy(vkDevice);

    mFinishHandler.terminate();

    ASSERT(mComputePassCommands->empty());
    ASSERT(!mNeedPrintfHandling);

    if (mPrintfBuffer)
    {
        // The lifetime of printf buffer is scoped to command queue, release and destroy.
        const bool wasLastUser = mPrintfBuffer->release();
        ASSERT(wasLastUser);
        delete mPrintfBuffer;
    }

    if (mQueueSerialIndex != kInvalidQueueSerialIndex)
    {
        mContext->getRenderer()->releaseQueueSerialIndex(mQueueSerialIndex);
        mQueueSerialIndex = kInvalidQueueSerialIndex;
    }

    // Recycle the current command buffers
    mContext->getRenderer()->recycleOutsideRenderPassCommandBufferHelper(&mComputePassCommands);
    mCommandPool.outsideRenderPassPool.destroy(vkDevice);
}

angle::Result CLCommandQueueVk::setProperty(cl::CommandQueueProperties properties, cl_bool enable)
{
    // NOTE: "clSetCommandQueueProperty" has been deprecated as of OpenCL 1.1
    // http://man.opencl.org/deprecated.html
    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueReadBuffer(const cl::Buffer &buffer,
                                                  bool blocking,
                                                  size_t offset,
                                                  size_t size,
                                                  void *ptr,
                                                  const cl::EventPtrs &waitEvents,
                                                  cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(
        event, blocking ? cl::ExecutionStatus::Complete : cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    CLBufferVk *bufferVk = &buffer.getImpl<CLBufferVk>();
    if (blocking)
    {
        ANGLE_TRY(finishInternal());
        ANGLE_TRY(bufferVk->copyTo(ptr, offset, size));
    }
    else
    {
        // Stage a transfer routine
        HostReadTransferConfig transferConfig(CL_COMMAND_READ_BUFFER, size, offset, ptr);
        ANGLE_TRY(addToHostTransferList(bufferVk, transferConfig));
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueWriteBuffer(const cl::Buffer &buffer,
                                                   bool blocking,
                                                   size_t offset,
                                                   size_t size,
                                                   const void *ptr,
                                                   const cl::EventPtrs &waitEvents,
                                                   cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(
        event, blocking ? cl::ExecutionStatus::Complete : cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    auto bufferVk = &buffer.getImpl<CLBufferVk>();
    if (blocking)
    {
        ANGLE_TRY(finishInternal());
        ANGLE_TRY(bufferVk->copyFrom(ptr, offset, size));
    }
    else
    {
        // Stage a transfer routine
        HostWriteTransferConfig config(CL_COMMAND_WRITE_BUFFER, size, offset, ptr);
        ANGLE_TRY(addToHostTransferList(bufferVk, config));
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueReadBufferRect(const cl::Buffer &buffer,
                                                      bool blocking,
                                                      const cl::Offset &bufferOrigin,
                                                      const cl::Offset &hostOrigin,
                                                      const cl::Extents &region,
                                                      size_t bufferRowPitch,
                                                      size_t bufferSlicePitch,
                                                      size_t hostRowPitch,
                                                      size_t hostSlicePitch,
                                                      void *ptr,
                                                      const cl::EventPtrs &waitEvents,
                                                      cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(
        event, blocking ? cl::ExecutionStatus::Complete : cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    auto bufferVk = &buffer.getImpl<CLBufferVk>();
    cl::BufferRect bufferRect{bufferOrigin, region, bufferRowPitch, bufferSlicePitch, 1};
    cl::BufferRect ptrRect{hostOrigin, region, hostRowPitch, hostSlicePitch, 1};

    if (blocking)
    {
        ANGLE_TRY(finishInternal());
        ANGLE_TRY(bufferVk->getRect(bufferRect, ptrRect, ptr));
    }
    else
    {
        // Stage a transfer routine
        HostReadTransferConfig config(CL_COMMAND_READ_BUFFER_RECT, ptrRect.getRectSize(), ptr,
                                      bufferRect, ptrRect);
        ANGLE_TRY(addToHostTransferList(bufferVk, config));
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueWriteBufferRect(const cl::Buffer &buffer,
                                                       bool blocking,
                                                       const cl::Offset &bufferOrigin,
                                                       const cl::Offset &hostOrigin,
                                                       const cl::Extents &region,
                                                       size_t bufferRowPitch,
                                                       size_t bufferSlicePitch,
                                                       size_t hostRowPitch,
                                                       size_t hostSlicePitch,
                                                       const void *ptr,
                                                       const cl::EventPtrs &waitEvents,
                                                       cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(
        event, blocking ? cl::ExecutionStatus::Complete : cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    auto bufferVk = &buffer.getImpl<CLBufferVk>();
    cl::BufferRect bufferRect{bufferOrigin, region, bufferRowPitch, bufferSlicePitch, 1};
    cl::BufferRect ptrRect{hostOrigin, region, hostRowPitch, hostSlicePitch, 1};

    if (blocking)
    {
        ANGLE_TRY(finishInternal());
        ANGLE_TRY(bufferVk->setRect(ptr, ptrRect, bufferRect));
    }
    else
    {
        // Stage a transfer routine
        HostWriteTransferConfig config(CL_COMMAND_WRITE_BUFFER_RECT, ptrRect.getRectSize(),
                                       const_cast<void *>(ptr), bufferRect, ptrRect);
        ANGLE_TRY(addToHostTransferList(bufferVk, config));
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueCopyBuffer(const cl::Buffer &srcBuffer,
                                                  const cl::Buffer &dstBuffer,
                                                  size_t srcOffset,
                                                  size_t dstOffset,
                                                  size_t size,
                                                  const cl::EventPtrs &waitEvents,
                                                  cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    CLBufferVk *srcBufferVk = &srcBuffer.getImpl<CLBufferVk>();
    CLBufferVk *dstBufferVk = &dstBuffer.getImpl<CLBufferVk>();

    vk::CommandResources resources;
    if (srcBufferVk->isSubBuffer() && dstBufferVk->isSubBuffer() &&
        (srcBufferVk->getParent() == dstBufferVk->getParent()))
    {
        // this is a self copy
        resources.onBufferSelfCopy(&srcBufferVk->getBuffer());
    }
    else
    {
        resources.onBufferTransferRead(&srcBufferVk->getBuffer());
        resources.onBufferTransferWrite(&dstBufferVk->getBuffer());
    }

    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(getCommandBuffer(resources, &commandBuffer));

    VkBufferCopy copyRegion = {srcOffset, dstOffset, size};
    // update the offset in the case of sub-buffers
    if (srcBufferVk->getOffset())
    {
        copyRegion.srcOffset += srcBufferVk->getOffset();
    }
    if (dstBufferVk->getOffset())
    {
        copyRegion.dstOffset += dstBufferVk->getOffset();
    }
    commandBuffer->copyBuffer(srcBufferVk->getBuffer().getBuffer(),
                              dstBufferVk->getBuffer().getBuffer(), 1, &copyRegion);

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueCopyBufferRect(const cl::Buffer &srcBuffer,
                                                      const cl::Buffer &dstBuffer,
                                                      const cl::Offset &srcOrigin,
                                                      const cl::Offset &dstOrigin,
                                                      const cl::Extents &region,
                                                      size_t srcRowPitch,
                                                      size_t srcSlicePitch,
                                                      size_t dstRowPitch,
                                                      size_t dstSlicePitch,
                                                      const cl::EventPtrs &waitEvents,
                                                      cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Complete));
    ANGLE_TRY(processWaitlist(waitEvents));
    ANGLE_TRY(finishInternal());

    cl::BufferRect srcRect{srcOrigin, region, srcRowPitch, srcSlicePitch, 1};

    cl::BufferRect dstRect{dstOrigin, region, dstRowPitch, dstSlicePitch, 1};

    auto srcBufferVk = &srcBuffer.getImpl<CLBufferVk>();
    auto dstBufferVk = &dstBuffer.getImpl<CLBufferVk>();

    uint8_t *mapPointer = nullptr;
    ANGLE_TRY(srcBufferVk->map(mapPointer));
    cl::Defer deferUnmap([&srcBufferVk]() { srcBufferVk->unmap(); });

    if (srcBuffer.getFlags().intersects(CL_MEM_USE_HOST_PTR) && !srcBufferVk->supportsZeroCopy())
    {
        // UHP needs special handling when zero-copy is not supported
        ANGLE_TRY(srcBufferVk->copyTo(mapPointer, 0, srcBufferVk->getSize()));
    }

    ANGLE_TRY(dstBufferVk->setRect(static_cast<const void *>(mapPointer), srcRect, dstRect));

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueFillBuffer(const cl::Buffer &buffer,
                                                  const void *pattern,
                                                  size_t patternSize,
                                                  size_t offset,
                                                  size_t size,
                                                  const cl::EventPtrs &waitEvents,
                                                  cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    CLBufferVk *bufferVk = &buffer.getImpl<CLBufferVk>();

    // Stage a transfer routine
    HostWriteTransferConfig config(CL_COMMAND_FILL_BUFFER, size, offset, pattern, patternSize);
    ANGLE_TRY(addToHostTransferList(bufferVk, config));

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueMapBuffer(const cl::Buffer &buffer,
                                                 bool blocking,
                                                 cl::MapFlags mapFlags,
                                                 size_t offset,
                                                 size_t size,
                                                 const cl::EventPtrs &waitEvents,
                                                 cl::EventPtr &event,
                                                 void *&mapPtr)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(
        event, blocking ? cl::ExecutionStatus::Complete : cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    if (blocking)
    {
        ANGLE_TRY(finishInternal());
    }

    CLBufferVk *bufferVk = &buffer.getImpl<CLBufferVk>();
    uint8_t *mapPointer  = nullptr;
    ANGLE_TRY(bufferVk->map(mapPointer, offset));
    mapPtr = mapPointer;

    if (buffer.getFlags().intersects(CL_MEM_USE_HOST_PTR) && !bufferVk->supportsZeroCopy())
    {
        // UHP needs special handling when zero-copy is not supported
        ANGLE_TRY(bufferVk->copyTo(mapPointer, offset, size));
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::copyImageToFromBuffer(CLImageVk &imageVk,
                                                      CLBufferVk &buffer,
                                                      VkBufferImageCopy copyRegion,
                                                      ImageBufferCopyDirection direction)
{
    vk::Renderer *renderer = mContext->getRenderer();

    vk::CommandResources resources;
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    VkImageAspectFlags aspectFlags = imageVk.getImage().getAspectFlags();
    if (direction == ImageBufferCopyDirection::ToBuffer)
    {
        resources.onImageTransferRead(aspectFlags, &imageVk.getImage());
        resources.onBufferTransferWrite(&buffer.getBuffer());
    }
    else
    {
        resources.onImageTransferWrite(gl::LevelIndex(0), 1, 0,
                                       static_cast<uint32_t>(imageVk.getArraySize()), aspectFlags,
                                       &imageVk.getImage());
        resources.onBufferTransferRead(&buffer.getBuffer());
    }
    ANGLE_TRY(getCommandBuffer(resources, &commandBuffer));

    if (imageVk.isWritable())
    {
        // We need an execution barrier if image can be written to by kernel
        ANGLE_TRY(insertBarrier());
    }

    VkMemoryBarrier memBarrier = {};
    memBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT;
    memBarrier.dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    if (direction == ImageBufferCopyDirection::ToBuffer)
    {
        commandBuffer->copyImageToBuffer(
            imageVk.getImage().getImage(), imageVk.getImage().getCurrentLayout(renderer),
            buffer.getBuffer().getBuffer().getHandle(), 1, &copyRegion);

        mComputePassCommands->getCommandBuffer().pipelineBarrier(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memBarrier, 0,
            nullptr, 0, nullptr);
    }
    else
    {
        commandBuffer->copyBufferToImage(
            buffer.getBuffer().getBuffer().getHandle(), imageVk.getImage().getImage(),
            imageVk.getImage().getCurrentLayout(renderer), 1, &copyRegion);

        mComputePassCommands->getCommandBuffer().pipelineBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier,
            0, nullptr, 0, nullptr);
    }

    return angle::Result::Continue;
}

template <typename T>
angle::Result CLCommandQueueVk::addToHostTransferList(CLBufferVk *srcBuffer,
                                                      HostTransferConfig<T> transferConfig)
{
    // TODO(aannestrand): Flush here if we reach some max-transfer-buffer heuristic
    // http://anglebug.com/377545840

    cl::Memory *transferBufferHandle   = nullptr;
    cl::MemFlags transferBufferMemFlag = cl::MemFlags(CL_MEM_READ_WRITE);

    // We insert an appropriate copy command in the command stream. For the host ptr, we create CL
    // buffer with UHP flags to reflect the contents on the host side.
    switch (transferConfig.getType())
    {
        case CL_COMMAND_WRITE_BUFFER:
        case CL_COMMAND_WRITE_BUFFER_RECT:
        case CL_COMMAND_READ_BUFFER:
        case CL_COMMAND_READ_BUFFER_RECT:
            transferBufferMemFlag.set(CL_MEM_USE_HOST_PTR);
            break;
        default:  // zero-copy attempt is not supported for CL_COMMAND_FILL_BUFFER
            break;
    }

    transferBufferHandle = cl::Buffer::Cast(this->mContext->getFrontendObject().createBuffer(
        nullptr, transferBufferMemFlag, transferConfig.getSize(),
        const_cast<void *>(transferConfig.getHostPtr())));
    if (transferBufferHandle == nullptr)
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }
    HostTransferEntry transferEntry{transferConfig, cl::MemoryPtr{transferBufferHandle}};
    mCommandsStateMap.addHostTransferEntry(mComputePassCommands->getQueueSerial(), transferEntry);

    // Release initialization reference, lifetime controlled by RefPointer.
    transferBufferHandle->release();

    // We need an execution barrier if buffer can be written to by kernel
    if (!mComputePassCommands->getCommandBuffer().empty() && srcBuffer->isWritable())
    {
        // TODO(aannestrand): Look into combining these kernel execution barriers
        // http://anglebug.com/377545840
        VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                         VK_ACCESS_SHADER_WRITE_BIT,
                                         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
        mComputePassCommands->getCommandBuffer().pipelineBarrier(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
            &memoryBarrier, 0, nullptr, 0, nullptr);
    }

    // Enqueue blit/transfer cmd
    VkPipelineStageFlags srcStageMask  = {};
    VkPipelineStageFlags dstStageMask  = {};
    VkMemoryBarrier memBarrier         = {};
    memBarrier.sType                   = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    CLBufferVk &transferBufferHandleVk = transferBufferHandle->getImpl<CLBufferVk>();
    switch (transferConfig.getType())
    {
        case CL_COMMAND_WRITE_BUFFER:
        {
            VkBufferCopy copyRegion = {0, transferConfig.getOffset(), transferConfig.getSize()};
            copyRegion.srcOffset += transferBufferHandleVk.getOffset();
            copyRegion.dstOffset += srcBuffer->getOffset();
            mComputePassCommands->getCommandBuffer().copyBuffer(
                transferBufferHandleVk.getBuffer().getBuffer(), srcBuffer->getBuffer().getBuffer(),
                1, &copyRegion);

            srcStageMask             = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStageMask             = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            memBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            break;
        }
        case CL_COMMAND_WRITE_BUFFER_RECT:
        {
            for (VkBufferCopy &copyRegion : cl_vk::CalculateRectCopyRegions(
                     transferConfig.getHostRect(), transferConfig.getBufferRect()))
            {
                copyRegion.srcOffset += transferBufferHandleVk.getOffset();
                copyRegion.dstOffset += srcBuffer->getOffset();
                mComputePassCommands->getCommandBuffer().copyBuffer(
                    transferBufferHandleVk.getBuffer().getBuffer(),
                    srcBuffer->getBuffer().getBuffer(), 1, &copyRegion);
            }

            // Config transfer barrier
            srcStageMask             = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStageMask             = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            memBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            break;
        }
        case CL_COMMAND_READ_BUFFER:
        {
            VkBufferCopy copyRegion = {transferConfig.getOffset(), 0, transferConfig.getSize()};
            copyRegion.srcOffset += srcBuffer->getOffset();
            copyRegion.dstOffset += transferBufferHandleVk.getOffset();
            mComputePassCommands->getCommandBuffer().copyBuffer(
                srcBuffer->getBuffer().getBuffer(), transferBufferHandleVk.getBuffer().getBuffer(),
                1, &copyRegion);

            srcStageMask             = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            dstStageMask             = VK_PIPELINE_STAGE_HOST_BIT;
            memBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            break;
        }
        case CL_COMMAND_READ_BUFFER_RECT:
        {
            for (VkBufferCopy &copyRegion : cl_vk::CalculateRectCopyRegions(
                     transferConfig.getBufferRect(), transferConfig.getHostRect()))
            {
                copyRegion.srcOffset += srcBuffer->getOffset();
                copyRegion.dstOffset += transferBufferHandleVk.getOffset();
                mComputePassCommands->getCommandBuffer().copyBuffer(
                    srcBuffer->getBuffer().getBuffer(),
                    transferBufferHandleVk.getBuffer().getBuffer(), 1, &copyRegion);
            }

            // Config transfer barrier
            srcStageMask             = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            dstStageMask             = VK_PIPELINE_STAGE_HOST_BIT;
            memBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            break;
        }
        case CL_COMMAND_FILL_BUFFER:
        {
            // Fill the staging buffer with the pattern and then insert a copy command from staging
            // buffer to buffer
            ANGLE_TRY(transferBufferHandleVk.fillWithPattern(transferConfig.getHostPtr(),
                                                             transferConfig.getPatternSize(), 0,
                                                             transferConfig.getSize()));
            VkBufferCopy copyRegion = {
                transferBufferHandleVk.getOffset(),  // source is the staging buffer
                transferConfig.getOffset() + srcBuffer->getOffset(), transferConfig.getSize()};
            mComputePassCommands->getCommandBuffer().copyBuffer(
                transferBufferHandleVk.getBuffer().getBuffer(), srcBuffer->getBuffer().getBuffer(),
                1, &copyRegion);

            // Config transfer barrier
            srcStageMask             = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStageMask             = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            memBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            break;
        }
        default:
            UNIMPLEMENTED();
            break;
    }

    // TODO(aannestrand): Look into combining these transfer barriers
    // http://anglebug.com/377545840
    mComputePassCommands->getCommandBuffer().pipelineBarrier(srcStageMask, dstStageMask, 0, 1,
                                                             &memBarrier, 0, nullptr, 0, nullptr);

    return angle::Result::Continue;
}

template <typename T>
angle::Result CLCommandQueueVk::addToHostTransferList(CLImageVk *srcImage,
                                                      HostTransferConfig<T> transferConfig)
{
    // TODO(aannestrand): Flush here if we reach some max-transfer-buffer heuristic
    // http://anglebug.com/377545840

    cl::Memory *transferBufferHandle   = nullptr;
    cl::MemFlags transferBufferMemFlag = cl::MemFlags(CL_MEM_READ_WRITE);

    // We insert an appropriate copy command in the command stream. For the host ptr, we create CL
    // buffer with UHP flags to reflect the contents on the host side.
    switch (transferConfig.getType())
    {
        case CL_COMMAND_WRITE_IMAGE:
        case CL_COMMAND_READ_IMAGE:
            transferBufferMemFlag.set(CL_MEM_USE_HOST_PTR);
            break;
        default:
            break;
    }

    transferBufferHandle = cl::Buffer::Cast(this->mContext->getFrontendObject().createBuffer(
        nullptr, transferBufferMemFlag, transferConfig.getSize(),
        const_cast<void *>(transferConfig.getHostPtr())));
    if (transferBufferHandle == nullptr)
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    HostTransferEntry transferEntry{transferConfig, cl::MemoryPtr{transferBufferHandle}};
    mCommandsStateMap.addHostTransferEntry(mComputePassCommands->getQueueSerial(), transferEntry);

    // Release initialization reference, lifetime controlled by RefPointer.
    transferBufferHandle->release();

    CLBufferVk &transferBufferHandleVk = transferBufferHandle->getImpl<CLBufferVk>();
    ImageBufferCopyDirection direction = ImageBufferCopyDirection::ToBuffer;

    switch (transferConfig.getType())
    {
        case CL_COMMAND_WRITE_IMAGE:
            direction = ImageBufferCopyDirection::ToImage;
            break;
        case CL_COMMAND_READ_IMAGE:
            direction = ImageBufferCopyDirection::ToBuffer;
            break;
        default:
            UNREACHABLE();
            ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    VkBufferImageCopy copyRegion = CalculateBufferImageCopyRegion(
        0, static_cast<uint32_t>(transferConfig.getRowPitch()),
        static_cast<uint32_t>(transferConfig.getSlicePitch()), transferConfig.getOrigin(),
        transferConfig.getRegion(), srcImage);

    return copyImageToFromBuffer(*srcImage, transferBufferHandleVk, copyRegion, direction);
}

angle::Result CLCommandQueueVk::enqueueReadImage(const cl::Image &image,
                                                 bool blocking,
                                                 const cl::Offset &origin,
                                                 const cl::Extents &region,
                                                 size_t rowPitch,
                                                 size_t slicePitch,
                                                 void *ptr,
                                                 const cl::EventPtrs &waitEvents,
                                                 cl::EventPtr &event)

{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(
        event, blocking ? cl::ExecutionStatus::Complete : cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    CLImageVk &imageVk = image.getImpl<CLImageVk>();
    cl::BufferRect ptrRect{cl::kOffsetZero, region, rowPitch, slicePitch, imageVk.getElementSize()};

    if (imageVk.getParentType() == cl::MemObjectType::Buffer)
    {
        // TODO: implement this later
        // http://anglebug.com/444481344
        UNIMPLEMENTED();
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    // Create a transfer buffer and push it in update list
    HostReadTransferConfig transferConfig(CL_COMMAND_READ_IMAGE, ptrRect.getRectSize(), ptr,
                                          rowPitch, slicePitch, imageVk.getElementSize(), origin,
                                          region);

    ANGLE_TRY(addToHostTransferList(&imageVk, transferConfig));

    if (blocking)
    {
        ANGLE_TRY(finishInternal());
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueWriteImage(const cl::Image &image,
                                                  bool blocking,
                                                  const cl::Offset &origin,
                                                  const cl::Extents &region,
                                                  size_t inputRowPitch,
                                                  size_t inputSlicePitch,
                                                  const void *ptr,
                                                  const cl::EventPtrs &waitEvents,
                                                  cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(
        event, blocking ? cl::ExecutionStatus::Complete : cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    CLImageVk &imageVk = image.getImpl<CLImageVk>();
    cl::BufferRect ptrRect{cl::kOffsetZero, region, inputRowPitch, inputSlicePitch,
                           imageVk.getElementSize()};

    if (imageVk.getParentType() == cl::MemObjectType::Buffer)
    {
        // TODO: implement this later
        // http://anglebug.com/444481344
        UNIMPLEMENTED();
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    // Create a transfer buffer and push it in update list
    HostWriteTransferConfig transferConfig(CL_COMMAND_WRITE_IMAGE, ptrRect.getRectSize(),
                                           const_cast<void *>(ptr), inputRowPitch, inputSlicePitch,
                                           imageVk.getElementSize(), origin, region);

    ANGLE_TRY(addToHostTransferList(&imageVk, transferConfig));

    if (blocking)
    {
        ANGLE_TRY(finishInternal());
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueCopyImage(const cl::Image &srcImage,
                                                 const cl::Image &dstImage,
                                                 const cl::Offset &srcOrigin,
                                                 const cl::Offset &dstOrigin,
                                                 const cl::Extents &region,
                                                 const cl::EventPtrs &waitEvents,
                                                 cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    auto srcImageVk = &srcImage.getImpl<CLImageVk>();
    auto dstImageVk = &dstImage.getImpl<CLImageVk>();

    vk::CommandResources resources;
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    VkImageAspectFlags dstAspectFlags = srcImageVk->getImage().getAspectFlags();
    VkImageAspectFlags srcAspectFlags = dstImageVk->getImage().getAspectFlags();
    resources.onImageTransferWrite(gl::LevelIndex(0), 1, 0, 1, dstAspectFlags,
                                   &dstImageVk->getImage());
    resources.onImageTransferRead(srcAspectFlags, &srcImageVk->getImage());
    ANGLE_TRY(getCommandBuffer(resources, &commandBuffer));

    VkImageCopy copyRegion    = {};
    copyRegion.extent         = cl_vk::GetExtent(srcImageVk->getExtentForCopy(region));
    copyRegion.srcOffset      = cl_vk::GetOffset(srcImageVk->getOffsetForCopy(srcOrigin));
    copyRegion.dstOffset      = cl_vk::GetOffset(dstImageVk->getOffsetForCopy(dstOrigin));
    copyRegion.srcSubresource = srcImageVk->getSubresourceLayersForCopy(
        srcOrigin, region, dstImageVk->getType(), ImageCopyWith::Image);
    copyRegion.dstSubresource = dstImageVk->getSubresourceLayersForCopy(
        dstOrigin, region, srcImageVk->getType(), ImageCopyWith::Image);
    if (srcImageVk->isWritable() || dstImageVk->isWritable())
    {
        // We need an execution barrier if buffer can be written to by kernel
        ANGLE_TRY(insertBarrier());
    }

    vk::Renderer *renderer = mContext->getRenderer();
    commandBuffer->copyImage(srcImageVk->getImage().getImage(),
                             srcImageVk->getImage().getCurrentLayout(renderer),
                             dstImageVk->getImage().getImage(),
                             dstImageVk->getImage().getCurrentLayout(renderer), 1, &copyRegion);

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueFillImage(const cl::Image &image,
                                                 const void *fillColor,
                                                 const cl::Offset &origin,
                                                 const cl::Extents &region,
                                                 const cl::EventPtrs &waitEvents,
                                                 cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    CLImageVk &imageVk = image.getImpl<CLImageVk>();
    PixelColor packedColor;
    cl::Extents extent = imageVk.getImageExtent();

    imageVk.packPixels(fillColor, &packedColor);

    CLBufferVk *stagingBuffer = nullptr;
    ANGLE_TRY(imageVk.getOrCreateStagingBuffer(&stagingBuffer));
    ASSERT(stagingBuffer);

    VkBufferImageCopy copyRegion =
        CalculateBufferImageCopyRegion(0, 0, 0, cl::kOffsetZero, extent, &imageVk);
    ANGLE_TRY(copyImageToFromBuffer(imageVk, *stagingBuffer, copyRegion,
                                    ImageBufferCopyDirection::ToBuffer));
    ANGLE_TRY(finishInternal());

    ANGLE_TRY(imageVk.fillImageWithColor(origin, region, &packedColor));

    copyRegion = CalculateBufferImageCopyRegion(0, 0, 0, cl::kOffsetZero, extent, &imageVk);
    ANGLE_TRY(copyImageToFromBuffer(imageVk, *stagingBuffer, copyRegion,
                                    ImageBufferCopyDirection::ToImage));

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueCopyImageToBuffer(const cl::Image &srcImage,
                                                         const cl::Buffer &dstBuffer,
                                                         const cl::Offset &srcOrigin,
                                                         const cl::Extents &region,
                                                         size_t dstOffset,
                                                         const cl::EventPtrs &waitEvents,
                                                         cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    CLImageVk &srcImageVk   = srcImage.getImpl<CLImageVk>();
    CLBufferVk &dstBufferVk = dstBuffer.getImpl<CLBufferVk>();
    VkBufferImageCopy copyRegion =
        CalculateBufferImageCopyRegion(dstOffset, 0, 0, srcOrigin, region, &srcImageVk);
    ANGLE_TRY(copyImageToFromBuffer(srcImageVk, dstBufferVk, copyRegion,
                                    ImageBufferCopyDirection::ToBuffer));

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueCopyBufferToImage(const cl::Buffer &srcBuffer,
                                                         const cl::Image &dstImage,
                                                         size_t srcOffset,
                                                         const cl::Offset &dstOrigin,
                                                         const cl::Extents &region,
                                                         const cl::EventPtrs &waitEvents,
                                                         cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    CLBufferVk &srcBufferVk = srcBuffer.getImpl<CLBufferVk>();
    CLImageVk &dstImageVk   = dstImage.getImpl<CLImageVk>();
    VkBufferImageCopy copyRegion =
        CalculateBufferImageCopyRegion(srcOffset, 0, 0, dstOrigin, region, &dstImageVk);
    ANGLE_TRY(copyImageToFromBuffer(dstImageVk, srcBufferVk, copyRegion,
                                    ImageBufferCopyDirection::ToImage));

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueMapImage(const cl::Image &image,
                                                bool blocking,
                                                cl::MapFlags mapFlags,
                                                const cl::Offset &origin,
                                                const cl::Extents &region,
                                                size_t *imageRowPitch,
                                                size_t *imageSlicePitch,
                                                const cl::EventPtrs &waitEvents,
                                                cl::EventPtr &event,
                                                void *&mapPtr)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Complete));
    ANGLE_TRY(processWaitlist(waitEvents));

    CLImageVk *imageVk = &image.getImpl<CLImageVk>();
    cl::Extents extent = imageVk->getImageExtent();
    size_t elementSize = imageVk->getElementSize();
    size_t rowPitch    = imageVk->getRowPitch();
    size_t offset =
        (origin.x * elementSize) + (origin.y * rowPitch) + (origin.z * extent.height * rowPitch);
    size_t size = (region.width * region.height * region.depth * elementSize);

    mComputePassCommands->imageRead(mContext, imageVk->getImage().getAspectFlags(),
                                    vk::ImageAccess::TransferSrc, &imageVk->getImage());

    CLBufferVk *stagingBuffer = nullptr;
    ANGLE_TRY(imageVk->getOrCreateStagingBuffer(&stagingBuffer));

    VkBufferImageCopy copyRegion =
        CalculateBufferImageCopyRegion(0, 0, 0, cl::kOffsetZero, extent, imageVk);
    ANGLE_TRY(copyImageToFromBuffer(*imageVk, *stagingBuffer, copyRegion,
                                    ImageBufferCopyDirection::ToBuffer));

    if (blocking)
    {
        ANGLE_TRY(finishInternal());
    }

    uint8_t *mapPointer = nullptr;
    ANGLE_TRY(imageVk->map(mapPointer, offset));
    mapPtr = mapPointer;

    if (image.getFlags().intersects(CL_MEM_USE_HOST_PTR))
    {
        ANGLE_TRY(imageVk->copyTo(mapPointer, offset, size));
    }

    // The staging buffer is tightly packed, with no rowpitch and slicepitch. And in UHP case, row
    // and slice are always zero.
    *imageRowPitch = extent.width * elementSize;
    switch (imageVk->getDescriptor().type)
    {
        case cl::MemObjectType::Image1D:
        case cl::MemObjectType::Image1D_Buffer:
        case cl::MemObjectType::Image2D:
            if (imageSlicePitch != nullptr)
            {
                *imageSlicePitch = 0;
            }
            break;
        case cl::MemObjectType::Image2D_Array:
        case cl::MemObjectType::Image3D:
            *imageSlicePitch = (extent.height * (*imageRowPitch));
            break;
        case cl::MemObjectType::Image1D_Array:
            *imageSlicePitch = *imageRowPitch;
            break;
        default:
            UNREACHABLE();
            break;
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueUnmapMemObject(const cl::Memory &memory,
                                                      void *mappedPtr,
                                                      const cl::EventPtrs &waitEvents,
                                                      cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    if (!event)
    {
        ANGLE_TRY(finishInternal());
    }

    if (memory.getType() == cl::MemObjectType::Buffer)
    {
        CLBufferVk &bufferVk = memory.getImpl<CLBufferVk>();
        if (memory.getFlags().intersects(CL_MEM_USE_HOST_PTR))
        {
            ANGLE_TRY(finishInternal());
            ANGLE_TRY(bufferVk.copyFrom(memory.getHostPtr(), 0, bufferVk.getSize()));
        }
    }
    else if (memory.getType() != cl::MemObjectType::Pipe)
    {
        // of image type
        CLImageVk &imageVk = memory.getImpl<CLImageVk>();
        if (memory.getFlags().intersects(CL_MEM_USE_HOST_PTR))
        {
            uint8_t *mapPointer = static_cast<uint8_t *>(memory.getHostPtr());
            ANGLE_TRY(imageVk.copyStagingFrom(mapPointer, 0, imageVk.getSize()));
        }
        cl::Extents extent        = imageVk.getImageExtent();
        CLBufferVk *stagingBuffer = nullptr;
        ANGLE_TRY(imageVk.getOrCreateStagingBuffer(&stagingBuffer));
        ASSERT(stagingBuffer);

        VkBufferImageCopy copyRegion =
            CalculateBufferImageCopyRegion(0, 0, 0, cl::kOffsetZero, extent, &imageVk);
        ANGLE_TRY(copyImageToFromBuffer(imageVk, *stagingBuffer, copyRegion,
                                        ImageBufferCopyDirection::ToImage));

        ANGLE_TRY(finishInternal());
    }
    else
    {
        // mem object type pipe is not supported and creation of such an object should have
        // failed
        UNREACHABLE();
    }

    memory.getImpl<CLMemoryVk>().unmap();

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueMigrateMemObjects(const cl::MemoryPtrs &memObjects,
                                                         cl::MemMigrationFlags flags,
                                                         const cl::EventPtrs &waitEvents,
                                                         cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Complete));
    ANGLE_TRY(processWaitlist(waitEvents));

    if (mCommandQueue.getContext().getDevices().size() > 1)
    {
        // TODO(aannestrand): Later implement support to allow migration of mem objects across
        // different devices. http://anglebug.com/377942759
        UNIMPLEMENTED();
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueNDRangeKernel(const cl::Kernel &kernel,
                                                     const cl::NDRange &ndrange,
                                                     const cl::EventPtrs &waitEvents,
                                                     cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    vk::PipelineCacheAccess pipelineCache;
    vk::PipelineHelper *pipelineHelper = nullptr;
    CLKernelVk &kernelImpl             = kernel.getImpl<CLKernelVk>();
    const CLProgramVk::DeviceProgramData *devProgramData =
        kernelImpl.getProgram()->getDeviceProgramData(mCommandQueue.getDevice().getNative());
    ASSERT(devProgramData != nullptr);
    cl::NDRange enqueueNDRange(ndrange);

    // Start with Workgroup size (WGS) from kernel attribute (if available)
    cl::WorkgroupSize workgroupSize =
        devProgramData->getCompiledWorkgroupSize(kernelImpl.getKernelName());
    if (workgroupSize != cl::WorkgroupSize{0, 0, 0})
    {
        // Local work size (LWS) was valid, use that as WGS
        enqueueNDRange.localWorkSize = workgroupSize;
    }
    else
    {
        if (enqueueNDRange.nullLocalWorkSize)
        {
            // NULL value was passed, in which case the OpenCL implementation will determine
            // how to be break the global work-items into appropriate work-group instances.
            enqueueNDRange.localWorkSize =
                mCommandQueue.getDevice().getImpl<CLDeviceVk>().selectWorkGroupSize(enqueueNDRange);
        }
        // At this point, we should have a non-zero Workgroup size
        ASSERT((enqueueNDRange.localWorkSize != cl::WorkgroupSize{0, 0, 0}));
    }

    // Printf storage is setup for single time usage. So drive any existing usage to completion if
    // the kernel uses printf.
    if (kernelImpl.usesPrintf() && mNeedPrintfHandling)
    {
        ANGLE_TRY(finishInternal());
    }

    // Fetch or create compute pipeline (if we miss in cache)
    ANGLE_CL_IMPL_TRY_ERROR(mContext->getRenderer()->getPipelineCache(mContext, &pipelineCache),
                            CL_OUT_OF_RESOURCES);

    ANGLE_TRY(processKernelResources(kernelImpl));
    ANGLE_TRY(processGlobalPushConstants(kernelImpl, enqueueNDRange));

    // Create uniform dispatch region(s) based on VkLimits for WorkgroupCount
    const uint32_t *maxComputeWorkGroupCount =
        mContext->getRenderer()->getPhysicalDeviceProperties().limits.maxComputeWorkGroupCount;
    for (cl::NDRange &uniformRegion : enqueueNDRange.createUniformRegions(
             {maxComputeWorkGroupCount[0], maxComputeWorkGroupCount[1],
              maxComputeWorkGroupCount[2]}))
    {
        cl::WorkgroupCount uniformRegionWorkgroupCount = uniformRegion.getWorkgroupCount();
        const VkPushConstantRange *pushConstantRegionOffset =
            devProgramData->getRegionOffsetRange();
        if (pushConstantRegionOffset != nullptr)
        {
            // The sum of the global ID offset into the NDRange for this uniform region and
            // the global offset of the NDRange
            // https://github.com/google/clspv/blob/main/docs/OpenCLCOnVulkan.md#module-scope-push-constants
            uint32_t regionOffsets[3] = {
                enqueueNDRange.globalWorkOffset[0] + uniformRegion.globalWorkOffset[0],
                enqueueNDRange.globalWorkOffset[1] + uniformRegion.globalWorkOffset[1],
                enqueueNDRange.globalWorkOffset[2] + uniformRegion.globalWorkOffset[2]};
            mComputePassCommands->getCommandBuffer().pushConstants(
                kernelImpl.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                pushConstantRegionOffset->offset, pushConstantRegionOffset->size, &regionOffsets);
        }
        const VkPushConstantRange *pushConstantRegionGroupOffset =
            devProgramData->getRegionGroupOffsetRange();
        if (pushConstantRegionGroupOffset != nullptr)
        {
            // The 3D group ID offset into the NDRange for this region
            // https://github.com/google/clspv/blob/main/docs/OpenCLCOnVulkan.md#module-scope-push-constants
            ASSERT(enqueueNDRange.localWorkSize[0] > 0 && enqueueNDRange.localWorkSize[1] > 0 &&
                   enqueueNDRange.localWorkSize[2] > 0);
            ASSERT(uniformRegion.globalWorkOffset[0] % enqueueNDRange.localWorkSize[0] == 0 &&
                   uniformRegion.globalWorkOffset[1] % enqueueNDRange.localWorkSize[1] == 0 &&
                   uniformRegion.globalWorkOffset[2] % enqueueNDRange.localWorkSize[2] == 0);
            uint32_t regionGroupOffsets[3] = {
                uniformRegion.globalWorkOffset[0] / enqueueNDRange.localWorkSize[0],
                uniformRegion.globalWorkOffset[1] / enqueueNDRange.localWorkSize[1],
                uniformRegion.globalWorkOffset[2] / enqueueNDRange.localWorkSize[2]};
            mComputePassCommands->getCommandBuffer().pushConstants(
                kernelImpl.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                pushConstantRegionGroupOffset->offset, pushConstantRegionGroupOffset->size,
                &regionGroupOffsets);
        }

        ANGLE_TRY(kernelImpl.getOrCreateComputePipeline(
            &pipelineCache, uniformRegion, mCommandQueue.getDevice(), &pipelineHelper));
        mComputePassCommands->retainResource(pipelineHelper);
        mComputePassCommands->getCommandBuffer().bindComputePipeline(pipelineHelper->getPipeline());
        mComputePassCommands->getCommandBuffer().dispatch(uniformRegionWorkgroupCount[0],
                                                          uniformRegionWorkgroupCount[1],
                                                          uniformRegionWorkgroupCount[2]);
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueTask(const cl::Kernel &kernel,
                                            const cl::EventPtrs &waitEvents,
                                            cl::EventPtr &event)
{
    constexpr size_t globalWorkSize[3] = {1, 0, 0};
    constexpr size_t localWorkSize[3]  = {1, 0, 0};
    cl::NDRange ndrange(1, nullptr, globalWorkSize, localWorkSize);
    return enqueueNDRangeKernel(kernel, ndrange, waitEvents, event);
}

angle::Result CLCommandQueueVk::enqueueNativeKernel(cl::UserFunc userFunc,
                                                    void *args,
                                                    size_t cbArgs,
                                                    const cl::BufferPtrs &buffers,
                                                    const std::vector<size_t> &bufferPtrOffsets,
                                                    const cl::EventPtrs &waitEvents,
                                                    cl::EventPtr &event)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueMarkerWithWaitList(const cl::EventPtrs &waitEvents,
                                                          cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));
    ANGLE_TRY(processWaitlist(waitEvents));

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueMarker(cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));

    // This deprecated API is essentially a super-set of clEnqueueBarrier, where we also return
    // an event object (i.e. marker) since clEnqueueBarrier does not provide this
    ANGLE_TRY(insertBarrier());

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueWaitForEvents(const cl::EventPtrs &events)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    // Unlike clWaitForEvents, this routine is non-blocking
    ANGLE_TRY(processWaitlist(events));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueBarrierWithWaitList(const cl::EventPtrs &waitEvents,
                                                           cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Queued));

    // The barrier command either waits for a list of events to complete, or if the list is
    // empty it waits for all commands previously enqueued in command_queue to complete before
    // it completes
    if (waitEvents.empty())
    {
        ANGLE_TRY(insertBarrier());
    }
    else
    {
        ANGLE_TRY(processWaitlist(waitEvents));
    }

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::insertBarrier()
{
    VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                     VK_ACCESS_SHADER_WRITE_BIT,
                                     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
    mComputePassCommands->getCommandBuffer().pipelineBarrier(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
        &memoryBarrier, 0, nullptr, 0, nullptr);

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueBarrier()
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(insertBarrier());

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::flush()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::flush");

    QueueSerial lastSubmittedQueueSerial;
    {
        std::unique_lock<std::mutex> ul(mCommandQueueMutex);

        ANGLE_TRY(flushInternal());
        lastSubmittedQueueSerial = mLastSubmittedQueueSerial;
    }

    return mFinishHandler.notify(lastSubmittedQueueSerial);
}

angle::Result CLCommandQueueVk::finish()
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::finish");

    // Blocking finish
    return finishInternal();
}

angle::Result CLCommandQueueVk::enqueueAcquireExternalMemObjectsKHR(
    const cl::MemoryPtrs &memObjects,
    const cl::EventPtrs &waitEvents,
    cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    // for Vulkan imported memory, Vulkan driver already acquired ownership during buffer/image
    // create with properties, so nothing left to do here other than event processing
    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Complete));
    ANGLE_TRY(processWaitlist(waitEvents));

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::enqueueReleaseExternalMemObjectsKHR(
    const cl::MemoryPtrs &memObjects,
    const cl::EventPtrs &waitEvents,
    cl::EventPtr &event)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    // since we dup'ed the fd during buffer/image create with properties, there is no "releasing"
    // back to user (unlike VkImportMemoryFdInfoKHR), thus nothing left to do here except for
    // event processing
    ANGLE_TRY(preEnqueueOps(event, cl::ExecutionStatus::Complete));
    ANGLE_TRY(processWaitlist(waitEvents));

    return postEnqueueOps(event);
}

angle::Result CLCommandQueueVk::addMemoryDependencies(const CLKernelArgument *arg)
{
    if (IsCLKernelArgumentReadonly(*arg))
    {
        return addMemoryDependencies(GetCLKernelArgumentMemoryHandle(*arg),
                                     MemoryHandleAccess::ReadOnly);
    }
    else
    {
        return addMemoryDependencies(GetCLKernelArgumentMemoryHandle(*arg),
                                     MemoryHandleAccess::Writeable);
    }
}

angle::Result CLCommandQueueVk::addMemoryDependencies(cl::Memory *clMem, MemoryHandleAccess access)
{
    bool isWritable       = access == MemoryHandleAccess::Writeable;
    cl::Memory *parentMem = clMem->getParent().get();

    // Take an usage count
    mCommandsStateMap.addMemory(mComputePassCommands->getQueueSerial(), clMem);

    // Handle possible resource hazards
    bool needsBarrier = false;
    // A barrier is needed in the following cases
    //  - Presence of a pending write, irrespective of the current usage
    //  - A write usage with a pending read
    if (mWriteDependencyTracker.contains(clMem) || mWriteDependencyTracker.contains(parentMem) ||
        mWriteDependencyTracker.size() == kMaxDependencyTrackerSize)
    {
        needsBarrier = true;
    }
    else if (isWritable && (mReadDependencyTracker.contains(clMem) ||
                            mReadDependencyTracker.contains(parentMem) ||
                            mReadDependencyTracker.size() == kMaxDependencyTrackerSize))
    {
        needsBarrier = true;
    }

    // If a barrier is inserted with the current usage, we can safely clear existing dependencies as
    // this barrier signalling ensures their completion.
    if (needsBarrier)
    {
        mReadDependencyTracker.clear();
        mWriteDependencyTracker.clear();
    }
    // Add the current mem object, to the appropriate dependency list
    if (isWritable)
    {
        mWriteDependencyTracker.insert(clMem);
        if (parentMem)
        {
            mWriteDependencyTracker.insert(parentMem);
        }
    }
    else
    {
        mReadDependencyTracker.insert(clMem);
        if (parentMem)
        {
            mReadDependencyTracker.insert(parentMem);
        }
    }

    // Insert a layout transition for images
    if (cl::IsImageType(clMem->getType()))
    {
        CLImageVk &vkMem = clMem->getImpl<CLImageVk>();
        mComputePassCommands->imageWrite(mContext, gl::LevelIndex(0), 0, 1,
                                         vkMem.getImage().getAspectFlags(),
                                         vk::ImageAccess::ComputeShaderWrite, &vkMem.getImage());
    }
    if (needsBarrier)
    {
        ANGLE_TRY(insertBarrier());
    }

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::processKernelResources(CLKernelVk &kernelVk)
{
    vk::Renderer *renderer             = mContext->getRenderer();
    bool podBufferPresent              = false;
    uint32_t podBinding                = 0;
    VkDescriptorType podDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    const CLProgramVk::DeviceProgramData *devProgramData =
        kernelVk.getProgram()->getDeviceProgramData(mCommandQueue.getDevice().getNative());
    ASSERT(devProgramData != nullptr);

    // Set the descriptor set layouts and allocate descriptor sets
    // The descriptor set layouts are setup in the order of their appearance, as Vulkan requires
    // them to point to valid handles.
    angle::EnumIterator<DescriptorSetIndex> layoutIndex(DescriptorSetIndex::LiteralSampler);
    for (DescriptorSetIndex index : angle::AllEnums<DescriptorSetIndex>())
    {
        if (!kernelVk.getDescriptorSetLayoutDesc(index).empty())
        {
            // Setup the descriptor layout
            ANGLE_CL_IMPL_TRY_ERROR(mContext->getDescriptorSetLayoutCache()->getDescriptorSetLayout(
                                        mContext, kernelVk.getDescriptorSetLayoutDesc(index),
                                        &kernelVk.getDescriptorSetLayouts()[*layoutIndex]),
                                    CL_INVALID_OPERATION);
            ASSERT(kernelVk.getDescriptorSetLayouts()[*layoutIndex]->valid());

            // Allocate descriptor set
            ANGLE_TRY(mContext->allocateDescriptorSet(&kernelVk, index, layoutIndex,
                                                      mComputePassCommands));
            ++layoutIndex;
        }
    }

    // Setup the pipeline layout
    ANGLE_CL_IMPL_TRY_ERROR(kernelVk.initPipelineLayout(), CL_INVALID_OPERATION);

    // Retain kernel object until we finish executing it later
    mCommandsStateMap.addKernel(mComputePassCommands->getQueueSerial(),
                                &kernelVk.getFrontendObject());

    // Process descriptor sets used by the kernel
    vk::DescriptorSetArray<UpdateDescriptorSetsBuilder> updateDescriptorSetsBuilders;

    UpdateDescriptorSetsBuilder &literalSamplerDescSetBuilder =
        updateDescriptorSetsBuilders[DescriptorSetIndex::LiteralSampler];

    // Create/Setup Literal Sampler
    for (const ClspvLiteralSampler &literalSampler : devProgramData->reflectionData.literalSamplers)
    {
        cl::SamplerPtr clLiteralSampler =
            cl::SamplerPtr(cl::Sampler::Cast(this->mContext->getFrontendObject().createSampler(
                literalSampler.normalizedCoords, literalSampler.addressingMode,
                literalSampler.filterMode)));

        // Release immediately to ensure correct refcount
        clLiteralSampler->release();
        ASSERT(clLiteralSampler != nullptr);
        CLSamplerVk &vkLiteralSampler = clLiteralSampler->getImpl<CLSamplerVk>();

        VkDescriptorImageInfo &samplerInfo =
            literalSamplerDescSetBuilder.allocDescriptorImageInfo();
        samplerInfo.sampler     = vkLiteralSampler.getSamplerHelper().get().getHandle();
        samplerInfo.imageView   = VK_NULL_HANDLE;
        samplerInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkWriteDescriptorSet &writeDescriptorSet =
            literalSamplerDescSetBuilder.allocWriteDescriptorSet();
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        writeDescriptorSet.pImageInfo      = &samplerInfo;
        writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = kernelVk.getDescriptorSet(DescriptorSetIndex::LiteralSampler);
        writeDescriptorSet.dstBinding = literalSampler.binding;

        mCommandsStateMap.addSampler(mComputePassCommands->getQueueSerial(), clLiteralSampler);
    }

    CLKernelArguments args = kernelVk.getArgs();
    UpdateDescriptorSetsBuilder &kernelArgDescSetBuilder =
        updateDescriptorSetsBuilders[DescriptorSetIndex::KernelArguments];
    for (size_t index = 0; index < args.size(); index++)
    {
        const auto &arg = args.at(index);
        switch (arg.type)
        {
            case NonSemanticClspvReflectionArgumentUniform:
            case NonSemanticClspvReflectionArgumentStorageBuffer:
            {
                cl::Memory *clMem = GetCLKernelArgumentMemoryHandle(arg);
                ASSERT(clMem);
                CLBufferVk &vkMem = clMem->getImpl<CLBufferVk>();

                ANGLE_TRY(addMemoryDependencies(&arg));

                // Update buffer/descriptor info
                VkDescriptorBufferInfo &bufferInfo =
                    kernelArgDescSetBuilder.allocDescriptorBufferInfo();
                bufferInfo.range  = clMem->getSize();
                bufferInfo.offset = clMem->getOffset();
                bufferInfo.buffer = vkMem.getBuffer().getBuffer().getHandle();
                VkWriteDescriptorSet &writeDescriptorSet =
                    kernelArgDescSetBuilder.allocWriteDescriptorSet();
                writeDescriptorSet.descriptorCount = 1;
                writeDescriptorSet.descriptorType =
                    arg.type == NonSemanticClspvReflectionArgumentUniform
                        ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                        : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                writeDescriptorSet.pBufferInfo = &bufferInfo;
                writeDescriptorSet.sType       = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet =
                    kernelVk.getDescriptorSet(DescriptorSetIndex::KernelArguments);
                writeDescriptorSet.dstBinding = arg.descriptorBinding;
                break;
            }
            case NonSemanticClspvReflectionArgumentPodPushConstant:
            {
                ASSERT(!podBufferPresent);

                // Spec requires the size and offset to be multiple of 4, round up for size and
                // round down for offset to ensure this
                uint32_t offset = roundDownPow2(arg.pushConstOffset, 4u);
                uint32_t size =
                    roundUpPow2(arg.pushConstOffset + arg.pushConstantSize, 4u) - offset;
                ASSERT(offset + size <= kernelVk.getPodArgumentPushConstantsData().size());
                mComputePassCommands->getCommandBuffer().pushConstants(
                    kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, offset, size,
                    &kernelVk.getPodArgumentPushConstantsData()[offset]);
                break;
            }
            case NonSemanticClspvReflectionArgumentWorkgroup:
            {
                // Nothing to do here (this is already taken care of during clSetKernelArg)
                break;
            }
            case NonSemanticClspvReflectionArgumentSampler:
            {
                cl::Sampler *clSampler =
                    cl::Sampler::Cast(*static_cast<const cl_sampler *>(arg.handle));
                CLSamplerVk &vkSampler = clSampler->getImpl<CLSamplerVk>();
                VkDescriptorImageInfo &samplerInfo =
                    kernelArgDescSetBuilder.allocDescriptorImageInfo();
                samplerInfo.sampler = vkSampler.getSamplerHelper().get().getHandle();
                VkWriteDescriptorSet &writeDescriptorSet =
                    kernelArgDescSetBuilder.allocWriteDescriptorSet();
                writeDescriptorSet.descriptorCount = 1;
                writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
                writeDescriptorSet.pImageInfo      = &samplerInfo;
                writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet =
                    kernelVk.getDescriptorSet(DescriptorSetIndex::KernelArguments);
                writeDescriptorSet.dstBinding = arg.descriptorBinding;

                const VkPushConstantRange *samplerMaskRange =
                    devProgramData->getNormalizedSamplerMaskRange(index);
                if (samplerMaskRange != nullptr)
                {
                    if (clSampler->getNormalizedCoords() == false)
                    {
                        ANGLE_TRY(vkSampler.createNormalized());
                        samplerInfo.sampler =
                            vkSampler.getSamplerHelperNormalized().get().getHandle();
                    }
                    uint32_t mask = vkSampler.getSamplerMask();
                    mComputePassCommands->getCommandBuffer().pushConstants(
                        kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                        samplerMaskRange->offset, samplerMaskRange->size, &mask);
                }
                break;
            }
            case NonSemanticClspvReflectionArgumentStorageImage:
            case NonSemanticClspvReflectionArgumentSampledImage:
            {
                cl::Memory *clMem = GetCLKernelArgumentMemoryHandle(arg);
                ASSERT(clMem);
                CLImageVk &vkMem = clMem->getImpl<CLImageVk>();

                ANGLE_TRY(addMemoryDependencies(&arg));

                cl_image_format imageFormat = vkMem.getFormat();
                const VkPushConstantRange *imageDataChannelOrderRange =
                    devProgramData->getImageDataChannelOrderRange(index);
                if (imageDataChannelOrderRange != nullptr)
                {
                    mComputePassCommands->getCommandBuffer().pushConstants(
                        kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                        imageDataChannelOrderRange->offset, imageDataChannelOrderRange->size,
                        &imageFormat.image_channel_order);
                }

                const VkPushConstantRange *imageDataChannelDataTypeRange =
                    devProgramData->getImageDataChannelDataTypeRange(index);
                if (imageDataChannelDataTypeRange != nullptr)
                {
                    mComputePassCommands->getCommandBuffer().pushConstants(
                        kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                        imageDataChannelDataTypeRange->offset, imageDataChannelDataTypeRange->size,
                        &imageFormat.image_channel_data_type);
                }

                // Update image/descriptor info
                VkDescriptorImageInfo &imageInfo =
                    kernelArgDescSetBuilder.allocDescriptorImageInfo();
                // TODO: Can't this always be vkMem.getImage().getCurrentLayout(renderer)?
                imageInfo.imageLayout = arg.type == NonSemanticClspvReflectionArgumentStorageImage
                                            ? VK_IMAGE_LAYOUT_GENERAL
                                            : vkMem.getImage().getCurrentLayout(renderer);
                imageInfo.imageView   = vkMem.getImageView().getHandle();
                imageInfo.sampler     = VK_NULL_HANDLE;
                VkWriteDescriptorSet &writeDescriptorSet =
                    kernelArgDescSetBuilder.allocWriteDescriptorSet();
                writeDescriptorSet.descriptorCount = 1;
                writeDescriptorSet.descriptorType =
                    arg.type == NonSemanticClspvReflectionArgumentStorageImage
                        ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                        : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                writeDescriptorSet.pImageInfo = &imageInfo;
                writeDescriptorSet.sType      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet =
                    kernelVk.getDescriptorSet(DescriptorSetIndex::KernelArguments);
                writeDescriptorSet.dstBinding = arg.descriptorBinding;
                break;
            }
            case NonSemanticClspvReflectionArgumentUniformTexelBuffer:
            case NonSemanticClspvReflectionArgumentStorageTexelBuffer:
            {
                cl::Memory *clMem = GetCLKernelArgumentMemoryHandle(arg);
                CLImageVk &vkMem  = clMem->getImpl<CLImageVk>();

                ANGLE_TRY(addMemoryDependencies(&arg));

                VkBufferView &bufferView           = kernelArgDescSetBuilder.allocBufferView();
                const vk::BufferView *vkBufferView = nullptr;
                ANGLE_TRY(vkMem.getBufferView(&vkBufferView));
                bufferView = vkBufferView->getHandle();

                VkWriteDescriptorSet &writeDescriptorSet =
                    kernelArgDescSetBuilder.allocWriteDescriptorSet();
                writeDescriptorSet.descriptorCount = 1;
                writeDescriptorSet.descriptorType =
                    arg.type == NonSemanticClspvReflectionArgumentStorageTexelBuffer
                        ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
                        : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                writeDescriptorSet.pImageInfo = nullptr;
                writeDescriptorSet.sType      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet =
                    kernelVk.getDescriptorSet(DescriptorSetIndex::KernelArguments);
                writeDescriptorSet.dstBinding       = arg.descriptorBinding;
                writeDescriptorSet.pTexelBufferView = &bufferView;

                break;
            }
            case NonSemanticClspvReflectionArgumentPodUniform:
            case NonSemanticClspvReflectionArgumentPodStorageBuffer:
            {
                if (!podBufferPresent)
                {
                    podBufferPresent  = true;
                    podBinding        = arg.descriptorBinding;
                    podDescriptorType = arg.type == NonSemanticClspvReflectionArgumentPodUniform
                                            ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                            : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                }
                break;
            }
            case NonSemanticClspvReflectionArgumentPointerPushConstant:
            {
                unsigned char *argPushConstOrigin =
                    &(kernelVk.getPodArgumentPushConstantsData()[arg.pushConstOffset]);
                if (static_cast<const cl_mem>(arg.handle) == NULL)
                {
                    // If the argument is a buffer object, the arg_value pointer can be NULL or
                    // point to a NULL value in which case a NULL value will be used as the value
                    // for the argument declared as a pointer to global or constant memory in the
                    // kernel.
                    uint64_t null = 0;
                    std::memcpy(argPushConstOrigin, &null, arg.handleSize);
                }
                else
                {
                    cl::Memory *clMem = cl::Buffer::Cast(static_cast<const cl_mem>(arg.handle));
                    CLBufferVk &vkMem = clMem->getImpl<CLBufferVk>();

                    ANGLE_TRY(addMemoryDependencies(&arg));

                    uint64_t devAddr =
                        vkMem.getBuffer().getDeviceAddress(mContext) + vkMem.getOffset();
                    std::memcpy(argPushConstOrigin, &devAddr, arg.handleSize);
                }

                mComputePassCommands->getCommandBuffer().pushConstants(
                    kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                    roundDownPow2(arg.pushConstOffset, 4u), roundUpPow2(arg.pushConstantSize, 4u),
                    argPushConstOrigin);

                break;
            }
            case NonSemanticClspvReflectionArgumentPointerUniform:
            {
                ASSERT(kernelVk.getPodBuffer()->getSize() >= arg.handleSize + arg.podUniformOffset);
                if (static_cast<const cl_mem>(arg.handle) == NULL)
                {
                    // If the argument is a buffer object, the arg_value pointer can be NULL or
                    // point to a NULL value in which case a NULL value will be used as the value
                    // for the argument declared as a pointer to global or constant memory in the
                    // kernel.
                    uint64_t null = 0;
                    ANGLE_TRY(kernelVk.getPodBuffer()->getImpl<CLBufferVk>().copyFrom(
                        &null, arg.podStorageBufferOffset, arg.handleSize));
                }
                else
                {
                    cl::Memory *clMem = cl::Buffer::Cast(static_cast<const cl_mem>(arg.handle));
                    CLBufferVk &vkMem = clMem->getImpl<CLBufferVk>();
                    ANGLE_TRY(addMemoryDependencies(&arg));
                    uint64_t devAddr =
                        vkMem.getBuffer().getDeviceAddress(mContext) + vkMem.getOffset();
                    ANGLE_TRY(kernelVk.getPodBuffer()->getImpl<CLBufferVk>().copyFrom(
                        &devAddr, arg.podStorageBufferOffset, arg.handleSize));
                }

                if (!podBufferPresent)
                {
                    podBufferPresent  = true;
                    podBinding        = arg.descriptorBinding;
                    podDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                }
                break;
            }
            default:
            {
                UNIMPLEMENTED();
                break;
            }
        }
    }
    if (podBufferPresent)
    {
        // POD arguments exceeded the push constant size and are packaged in a storage buffer. Setup
        // commands and dependencies accordingly.
        cl::MemoryPtr clMem = kernelVk.getPodBuffer();
        ASSERT(clMem != nullptr);
        CLBufferVk &vkMem = clMem->getImpl<CLBufferVk>();

        VkDescriptorBufferInfo &bufferInfo = kernelArgDescSetBuilder.allocDescriptorBufferInfo();
        bufferInfo.range                   = clMem->getSize();
        bufferInfo.offset                  = clMem->getOffset();
        bufferInfo.buffer                  = vkMem.getBuffer().getBuffer().getHandle();

        if (clMem->getFlags().intersects(CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY))
        {
            ANGLE_TRY(addMemoryDependencies(clMem.get(), MemoryHandleAccess::Writeable));
        }
        else
        {
            ANGLE_TRY(addMemoryDependencies(clMem.get(), MemoryHandleAccess::ReadOnly));
        }

        VkWriteDescriptorSet &writeDescriptorSet =
            kernelArgDescSetBuilder.allocWriteDescriptorSet();
        writeDescriptorSet.sType  = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.pNext  = nullptr;
        writeDescriptorSet.dstSet = kernelVk.getDescriptorSet(DescriptorSetIndex::KernelArguments);
        writeDescriptorSet.dstBinding      = podBinding;
        writeDescriptorSet.dstArrayElement = 0;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.descriptorType  = podDescriptorType;
        writeDescriptorSet.pImageInfo      = nullptr;
        writeDescriptorSet.pBufferInfo     = &bufferInfo;
    }

    // Create Module Constant Data Buffer
    if (devProgramData->reflectionData.pushConstants.contains(
            NonSemanticClspvReflectionConstantDataPointerPushConstant))
    {
        cl::MemoryPtr clMem =
            kernelVk.getProgram()->getOrCreateModuleConstantDataBuffer(kernelVk.getKernelName());
        CLBufferVk &vkMem = clMem->getImpl<CLBufferVk>();
        uint64_t devAddr  = vkMem.getBuffer().getDeviceAddress(mContext) + vkMem.getOffset();

        if (clMem->getFlags().intersects(CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY))
        {
            ANGLE_TRY(addMemoryDependencies(clMem.get(), MemoryHandleAccess::Writeable));
        }
        else
        {
            ANGLE_TRY(addMemoryDependencies(clMem.get(), MemoryHandleAccess::ReadOnly));
        }

        const VkPushConstantRange *pushConstantRangePtr =
            &devProgramData->reflectionData.pushConstants.at(
                NonSemanticClspvReflectionConstantDataPointerPushConstant);
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, pushConstantRangePtr->offset,
            pushConstantRangePtr->size, &devAddr);
    }

    // process the printf storage buffer
    if (kernelVk.usesPrintf())
    {
        UpdateDescriptorSetsBuilder &printfDescSetBuilder =
            updateDescriptorSetsBuilders[DescriptorSetIndex::Printf];

        cl::MemoryPtr clMem = getOrCreatePrintfBuffer();
        CLBufferVk &vkMem   = clMem->getImpl<CLBufferVk>();
        uint8_t *mapPointer = nullptr;
        ANGLE_TRY(vkMem.map(mapPointer, 0));
        // The spec calls out *The first 4 bytes of the buffer should be zero-initialized.*
        memset(mapPointer, 0, 4);

        if (kernelVk.usesPrintfBufferPointerPushConstant())
        {
            const VkPushConstantRange *pushConstantRangePtr =
                &devProgramData->reflectionData.pushConstants.at(
                    NonSemanticClspvReflectionPrintfBufferPointerPushConstant);
            uint64_t devAddr = vkMem.getBuffer().getDeviceAddress(mContext) + vkMem.getOffset();
            // Push printf push-constant to command buffer
            mComputePassCommands->getCommandBuffer().pushConstants(
                kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                pushConstantRangePtr->offset, pushConstantRangePtr->size, &devAddr);
        }
        else
        {
            auto &bufferInfo  = printfDescSetBuilder.allocDescriptorBufferInfo();
            bufferInfo.range  = clMem->getSize();
            bufferInfo.offset = clMem->getOffset();
            bufferInfo.buffer = vkMem.getBuffer().getBuffer().getHandle();

            auto &writeDescriptorSet           = printfDescSetBuilder.allocWriteDescriptorSet();
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeDescriptorSet.pBufferInfo     = &bufferInfo;
            writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet = kernelVk.getDescriptorSet(DescriptorSetIndex::Printf);
            writeDescriptorSet.dstBinding =
                kernelVk.getProgram()
                    ->getDeviceProgramData(kernelVk.getKernelName().c_str())
                    ->reflectionData.printfBufferStorage.binding;
        }

        mNeedPrintfHandling = true;
    }

    angle::EnumIterator<DescriptorSetIndex> descriptorSetIndex(DescriptorSetIndex::LiteralSampler);
    for (DescriptorSetIndex index : angle::AllEnums<DescriptorSetIndex>())
    {
        if (!kernelVk.getDescriptorSetLayoutDesc(index).empty())
        {
            mContext->getPerfCounters().writeDescriptorSets =
                updateDescriptorSetsBuilders[index].flushDescriptorSetUpdates(
                    renderer->getDevice());

            VkDescriptorSet descriptorSet = kernelVk.getDescriptorSet(index);
            mComputePassCommands->getCommandBuffer().bindDescriptorSets(
                kernelVk.getPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, *descriptorSetIndex,
                1, &descriptorSet, 0, nullptr);

            ++descriptorSetIndex;
        }
    }

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::processGlobalPushConstants(CLKernelVk &kernelVk,
                                                           const cl::NDRange &ndrange)
{
    const CLProgramVk::DeviceProgramData *devProgramData =
        kernelVk.getProgram()->getDeviceProgramData(mCommandQueue.getDevice().getNative());
    ASSERT(devProgramData != nullptr);

    const VkPushConstantRange *globalOffsetRange = devProgramData->getGlobalOffsetRange();
    if (globalOffsetRange != nullptr)
    {
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, globalOffsetRange->offset,
            globalOffsetRange->size, ndrange.globalWorkOffset.data());
    }

    const VkPushConstantRange *globalSizeRange = devProgramData->getGlobalSizeRange();
    if (globalSizeRange != nullptr)
    {
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, globalSizeRange->offset,
            globalSizeRange->size, ndrange.globalWorkSize.data());
    }

    const VkPushConstantRange *enqueuedLocalSizeRange = devProgramData->getEnqueuedLocalSizeRange();
    if (enqueuedLocalSizeRange != nullptr)
    {
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
            enqueuedLocalSizeRange->offset, enqueuedLocalSizeRange->size,
            ndrange.localWorkSize.data());
    }

    const VkPushConstantRange *numWorkgroupsRange = devProgramData->getNumWorkgroupsRange();
    if (devProgramData->reflectionData.pushConstants.contains(
            NonSemanticClspvReflectionPushConstantNumWorkgroups))
    {
        // We support non-uniform workgroups, thus take the ceil of the quotient
        uint32_t numWorkgroups[3] = {
            UnsignedCeilDivide(ndrange.globalWorkSize[0], ndrange.localWorkSize[0]),
            UnsignedCeilDivide(ndrange.globalWorkSize[1], ndrange.localWorkSize[1]),
            UnsignedCeilDivide(ndrange.globalWorkSize[2], ndrange.localWorkSize[2])};
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, numWorkgroupsRange->offset,
            numWorkgroupsRange->size, &numWorkgroups);
    }

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::flushComputePassCommands()
{
    if (mComputePassCommands->empty())
    {
        return angle::Result::Continue;
    }

    // Flush any host visible buffers by adding appropriate barriers
    if (mComputePassCommands->getAndResetHasHostVisibleBufferWrite())
    {
        // Make sure all writes to host-visible buffers are flushed.
        VkMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT;
        memoryBarrier.dstAccessMask   = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;

        mComputePassCommands->getCommandBuffer().memoryBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT, memoryBarrier);
    }

    if (mContext->getRenderer()->getFeatures().debugClDumpCommandStream.enabled)
    {
        addCommandBufferDiagnostics(mComputePassCommands->getCommandDiagnostics());
    }

    // get hold of the queue serial that is flushed, post the flush the command buffer will be reset
    mLastFlushedQueueSerial = mComputePassCommands->getQueueSerial();
    // Here, we flush our compute cmds to RendererVk's primary command buffer
    ANGLE_TRY(mCommandState.flushOutsideRPCommands(mContext, &mComputePassCommands));

    mContext->getPerfCounters().flushedOutsideRenderPassCommandBuffers++;

    // Generate new serial for next batch of cmds
    mComputePassCommands->setQueueSerial(
        mQueueSerialIndex, mContext->getRenderer()->generateQueueSerial(mQueueSerialIndex));

    return mCommandsStateMap.setEventsWithQueueSerialToState(mLastFlushedQueueSerial,
                                                             cl::ExecutionStatus::Submitted);
}

angle::Result CLCommandQueueVk::processWaitlist(const cl::EventPtrs &waitEvents)
{
    if (!waitEvents.empty())
    {
        bool needsBarrier = false;
        for (const cl::EventPtr &event : waitEvents)
        {
            if (event->isUserEvent() || event->getCommandQueue() != &mCommandQueue)
            {
                // Track the user and external cq events separately
                mExternalEvents.push_back(event);
            }
            if (!event->isUserEvent())
            {
                // At the moment, the vulkan backend is set up with single queue for all the command
                // buffer recording (only if the Vk Queue priorities match).
                // So inserting a barrier (in this case) is enough to ensure dependencies here.
                needsBarrier |=
                    event->getCommandQueue()->getPriority() == mCommandQueue.getPriority();
            }
        }
        if (needsBarrier)
        {
            ANGLE_TRY(insertBarrier());
        }
    }
    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::submitCommands()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::submitCommands()");

    ASSERT(hasCommandsPendingSubmission());

    if (mContext->getRenderer()->getFeatures().debugClDumpCommandStream.enabled)
    {
        mContext->dumpCommandStreamDiagnostics();
    }

    // Kick off renderer submit
    ANGLE_TRY(mContext->getRenderer()->submitCommands(
        mContext, nullptr, nullptr, mLastFlushedQueueSerial, std::move(mCommandState)));

    mLastSubmittedQueueSerial = mLastFlushedQueueSerial;

    // Now that we have submitted commands, some of pending garbage may no longer pending
    // and should be moved to garbage list.
    mContext->getRenderer()->cleanupPendingSubmissionGarbage();

    return mCommandsStateMap.setEventsWithQueueSerialToState(mLastSubmittedQueueSerial,
                                                             cl::ExecutionStatus::Running);
}

angle::Result CLCommandQueueVk::preEnqueueOps(cl::EventPtr &event,
                                              cl::ExecutionStatus initialStatus)
{
    if (event != nullptr)
    {
        ANGLE_TRY(event->initBackend([initialStatus](const cl::Event &event) {
            auto eventVk = new (std::nothrow) CLEventVk(event, initialStatus);
            if (eventVk == nullptr)
            {
                ERR() << "Failed to create cmd event obj!";
                return CLEventImpl::Ptr(nullptr);
            }
            return CLEventImpl::Ptr(eventVk);
        }));
    }

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::postEnqueueOps(const cl::EventPtr &event)
{
    if (event != nullptr)
    {
        ASSERT(event->isBackendInitialized() && "backend event state is invalid!");

        cl_int status;
        CLEventVk &eventVk = event->getImpl<CLEventVk>();
        ANGLE_TRY(eventVk.getCommandExecutionStatus(status));
        if (cl::FromCLenum<cl::ExecutionStatus>(status) == cl::ExecutionStatus::Complete)
        {
            // skip event association if command is already complete
            return angle::Result::Continue;
        }
        eventVk.setQueueSerial(mComputePassCommands->getQueueSerial());
        mCommandsStateMap.addEvent(eventVk.getQueueSerial(), event);
    }

    if (mContext->getRenderer()->getFeatures().clSerializedExecution.enabled)
    {
        ANGLE_TRY(finishInternal());
    }

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::submitEmptyCommand()
{
    // This will be called as part of resetting the command buffer and command buffer has to be
    // empty.
    ASSERT(mComputePassCommands->empty());

    // There is nothing to be flushed, mark it flushed and do a submit to signal the queue serial
    mLastFlushedQueueSerial = mComputePassCommands->getQueueSerial();
    ANGLE_TRY(submitCommands());
    ANGLE_TRY(finishQueueSerialInternal(mLastSubmittedQueueSerial));

    // increment the queue serial for the next command batch
    mComputePassCommands->setQueueSerial(
        mQueueSerialIndex, mContext->getRenderer()->generateQueueSerial(mQueueSerialIndex));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::resetCommandBufferWithError(cl_int errorCode)
{
    // Got an error so reset the command buffer and report back error to all the associated
    // events
    ASSERT(errorCode != CL_SUCCESS);

    QueueSerial currentSerial = mComputePassCommands->getQueueSerial();
    mComputePassCommands->getCommandBuffer().reset();

    ANGLE_TRY(mCommandsStateMap.setEventsWithQueueSerialToState(currentSerial,
                                                                cl::ExecutionStatus::InvalidEnum));
    mCommandsStateMap.erase(currentSerial);
    mExternalEvents.clear();

    // Command buffer has been reset and as such the associated queue serial will not get signaled
    // leading to causality issues. So submit an empty command to keep the queue serials timelines
    // intact.
    ANGLE_TRY(submitEmptyCommand());

    ANGLE_CL_RETURN_ERROR(errorCode);
}

angle::Result CLCommandQueueVk::finishQueueSerialInternal(const QueueSerial queueSerial)
{
    // Queue serial must belong to this queue and work must have been submitted.
    ASSERT(queueSerial.getIndex() == mQueueSerialIndex);
    ASSERT(mContext->getRenderer()->hasQueueSerialSubmitted(queueSerial));

    ANGLE_TRY(mContext->getRenderer()->finishQueueSerial(mContext, queueSerial));

    // Ensure memory  objects are synced back to host CPU
    ANGLE_TRY(mCommandsStateMap.processQueueSerial(queueSerial));

    if (mNeedPrintfHandling)
    {
        mNeedPrintfHandling = false;
    }

    // Events associated with this queue serial and ready to be marked complete
    ANGLE_TRY(mCommandsStateMap.setEventsWithQueueSerialToState(queueSerial,
                                                                cl::ExecutionStatus::Complete));

    mCommandsStateMap.erase(queueSerial);

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::finishQueueSerial(const QueueSerial queueSerial)
{
    ASSERT(queueSerial.getIndex() == getQueueSerialIndex());
    ASSERT(mContext->getRenderer()->hasQueueSerialSubmitted(queueSerial));

    ANGLE_TRY(mContext->getRenderer()->finishQueueSerial(mContext, queueSerial));

    std::lock_guard<std::mutex> sl(mCommandQueueMutex);

    return finishQueueSerialInternal(queueSerial);
}

angle::Result CLCommandQueueVk::flushInternal()
{
    if (!mComputePassCommands->empty())
    {
        // If we still have dependant events, handle them now
        if (!mExternalEvents.empty())
        {
            for (const auto &depEvent : mExternalEvents)
            {
                if (depEvent->getImpl<CLEventVk>().isUserEvent())
                {
                    // We just wait here for user to set the event object
                    cl_int status = CL_QUEUED;
                    ANGLE_TRY(depEvent->getImpl<CLEventVk>().waitForUserEventStatus());
                    ANGLE_TRY(depEvent->getImpl<CLEventVk>().getCommandExecutionStatus(status));
                    if (status < 0)
                    {
                        ERR() << "Invalid dependant user-event (" << depEvent.get()
                              << ") status encountered!";
                        ANGLE_TRY(resetCommandBufferWithError(
                            CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST));
                    }
                }
                else
                {
                    if (depEvent->getCommandQueue()->getPriority() != mCommandQueue.getPriority())
                    {
                        // this implicitly means that different Vk Queues are used between the
                        // dependency event queue and this queue. thus, sync/finish here to ensure
                        // dependencies.
                        // TODO: Look into Vk Semaphores here to track GPU-side only
                        // https://anglebug.com/42267109
                        ANGLE_TRY(depEvent->getCommandQueue()->finish());
                    }
                    else
                    {
                        // We have inserted appropriate pipeline barriers, we just need to flush the
                        // dependent queue before we submit the commands here.
                        ANGLE_TRY(depEvent->getCommandQueue()->flush());
                    }
                }
            }
            mExternalEvents.clear();
        }

        ANGLE_TRY(flushComputePassCommands());
        ANGLE_TRY(submitCommands());
        ASSERT(!hasCommandsPendingSubmission());
    }

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::finishInternal()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::finish");
    ANGLE_TRY(flushInternal());

    return finishQueueSerialInternal(mLastSubmittedQueueSerial);
}

// Helper function to insert appropriate memory barriers before accessing the resources in the
// command buffer.
angle::Result CLCommandQueueVk::onResourceAccess(const vk::CommandResources &resources)
{
    // Buffers
    for (const vk::CommandResourceBuffer &readBuffer : resources.getReadBuffers())
    {
        if (mComputePassCommands->usesBufferForWrite(*readBuffer.buffer))
        {
            // read buffers only need a new command buffer if previously used for write
            ANGLE_TRY(flushInternal());
        }

        mComputePassCommands->bufferRead(mContext, readBuffer.accessType, readBuffer.stage,
                                         readBuffer.buffer);
    }

    for (const vk::CommandResourceBuffer &writeBuffer : resources.getWriteBuffers())
    {
        if (mComputePassCommands->usesBuffer(*writeBuffer.buffer))
        {
            // write buffers always need a new command buffer
            ANGLE_TRY(flushInternal());
        }

        mComputePassCommands->bufferWrite(mContext, writeBuffer.accessType, writeBuffer.stage,
                                          writeBuffer.buffer);
        if (writeBuffer.buffer->isHostVisible())
        {
            // currently all are host visible so nothing to do
        }
    }

    for (const vk::CommandResourceBufferExternalAcquireRelease &bufferAcquireRelease :
         resources.getExternalAcquireReleaseBuffers())
    {
        mComputePassCommands->retainResourceForWrite(bufferAcquireRelease.buffer);
    }

    for (const vk::CommandResourceGeneric &genericResource : resources.getGenericResources())
    {
        mComputePassCommands->retainResource(genericResource.resource);
    }

    return angle::Result::Continue;
}

// A single CL buffer is setup for every command queue of size kPrintfBufferSize. This can be
// expanded later, if more storage is needed.
cl::MemoryPtr CLCommandQueueVk::getOrCreatePrintfBuffer()
{
    if (!mPrintfBuffer)
    {
        mPrintfBuffer = cl::Buffer::Cast(mContext->getFrontendObject().createBuffer(
            nullptr, cl::MemFlags(CL_MEM_READ_WRITE), kPrintfBufferSize, nullptr));
    }
    mCommandsStateMap.addPrintfBuffer(mComputePassCommands->getQueueSerial(), mPrintfBuffer);

    return cl::MemoryPtr(mPrintfBuffer);
}

bool CLCommandQueueVk::hasUserEventDependency() const
{
    return std::any_of(mExternalEvents.begin(), mExternalEvents.end(),
                       [](const cl::EventPtr event) { return event->isUserEvent(); });
}

void CLCommandQueueVk::addCommandBufferDiagnostics(const std::string &commandBufferDiagnostics)
{
    mContext->addCommandBufferDiagnostics(commandBufferDiagnostics);
}

angle::Result CommandsStateMap::setEventsWithQueueSerialToState(const QueueSerial &queueSerial,
                                                                cl::ExecutionStatus executionStatus)
{
    std::unique_lock<angle::SimpleMutex> ul(mMutex);
    cl_int newStatus = executionStatus == cl::ExecutionStatus::InvalidEnum
                           ? CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST
                           : cl::ToCLenum(executionStatus);
    for (const auto &[serial, state] : mCommandsState)
    {
        if (serial <= queueSerial)
        {
            for (cl::EventPtr event : state.mEvents)
            {
                CLEventVk *eventVk   = &event->getImpl<CLEventVk>();
                cl_int currentStatus = CL_QUEUED;
                ANGLE_TRY(eventVk->getCommandExecutionStatus(currentStatus));
                if (!eventVk->isUserEvent() && currentStatus > newStatus)
                {
                    ANGLE_TRY(eventVk->setStatusAndExecuteCallback(newStatus));
                }
            }
        }
    }
    return angle::Result::Continue;
}

angle::Result CommandsStateMap::processQueueSerial(const QueueSerial queueSerial)
{
    std::unique_lock<angle::SimpleMutex> ul(mMutex);
    HostTransferEntries list = mCommandsState[queueSerial].mHostTransferList;
    for (const HostTransferEntry &hostTransferEntry : list)
    {
        ANGLE_TRY(std::visit(HostTransferConfigVisitor(
                                 hostTransferEntry.transferBufferHandle->getImpl<CLBufferVk>()),
                             hostTransferEntry.transferConfig));
    }
    list.clear();

    cl::KernelPtrs kernels = mCommandsState[queueSerial].mKernels;

    for (cl::KernelPtr kernel : kernels)
    {
        CLKernelVk *kernelVk = &kernel->getImpl<CLKernelVk>();

        if (kernelVk->usesPrintf())
        {
            ASSERT(kernels.size() == 1);

            auto printfInfos =
                kernelVk->getProgram()->getPrintfDescriptors(kernelVk->getKernelName());

            CLBufferVk &vkMem = mCommandsState[queueSerial].mPrintfBuffer->getImpl<CLBufferVk>();

            unsigned char *data = nullptr;
            ANGLE_TRY(vkMem.map(data, 0));
            ANGLE_TRY(ClspvProcessPrintfBuffer(data, vkMem.getSize(), printfInfos));
            vkMem.unmap();
        }
    }

    return angle::Result::Continue;
}

}  // namespace rx
