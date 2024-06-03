//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLCommandQueueVk.cpp: Implements the class methods for CLCommandQueueVk.

#include "libANGLE/renderer/vulkan/CLCommandQueueVk.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLDeviceVk.h"
#include "libANGLE/renderer/vulkan/CLKernelVk.h"
#include "libANGLE/renderer/vulkan/CLMemoryVk.h"
#include "libANGLE/renderer/vulkan/CLProgramVk.h"
#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLCommandQueue.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLEvent.h"
#include "libANGLE/CLKernel.h"
#include "libANGLE/cl_utils.h"

#include "spirv/unified1/NonSemanticClspvReflection.h"

namespace rx
{

class CLAsyncFinishTask : public angle::Closure
{
  public:
    CLAsyncFinishTask(CLCommandQueueVk *queueVk) : mQueueVk(queueVk) {}

    void operator()() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::finish (async)");
        if (IsError(mQueueVk->finish()))
        {
            ERR() << "Async finish (clFlush) failed for queue (" << mQueueVk << ")!";
        }
    }

  private:
    CLCommandQueueVk *mQueueVk;
};

CLCommandQueueVk::CLCommandQueueVk(const cl::CommandQueue &commandQueue)
    : CLCommandQueueImpl(commandQueue),
      mContext(&commandQueue.getContext().getImpl<CLContextVk>()),
      mDevice(&commandQueue.getDevice().getImpl<CLDeviceVk>()),
      mComputePassCommands(nullptr),
      mCurrentQueueSerialIndex(kInvalidQueueSerialIndex),
      mHasAnyCommandsPendingSubmission(false)
{}

angle::Result CLCommandQueueVk::init()
{
    ANGLE_CL_IMPL_TRY_ERROR(
        vk::OutsideRenderPassCommandBuffer::InitializeCommandPool(
            mContext, &mCommandPool.outsideRenderPassPool,
            mContext->getRenderer()->getQueueFamilyIndex(), getProtectionType()),
        CL_OUT_OF_RESOURCES);

    ANGLE_CL_IMPL_TRY_ERROR(mContext->getRenderer()->getOutsideRenderPassCommandBufferHelper(
                                mContext, &mCommandPool.outsideRenderPassPool,
                                &mOutsideRenderPassCommandsAllocator, &mComputePassCommands),
                            CL_OUT_OF_RESOURCES);

    // Generate initial QueueSerial for command buffer helper
    ANGLE_CL_IMPL_TRY_ERROR(
        mContext->getRenderer()->allocateQueueSerialIndex(&mCurrentQueueSerialIndex),
        CL_OUT_OF_RESOURCES);
    mComputePassCommands->setQueueSerial(
        mCurrentQueueSerialIndex,
        mContext->getRenderer()->generateQueueSerial(mCurrentQueueSerialIndex));

    // Initialize serials to be valid but appear submitted and finished.
    mLastFlushedQueueSerial   = QueueSerial(mCurrentQueueSerialIndex, Serial());
    mLastSubmittedQueueSerial = mLastFlushedQueueSerial;

    return angle::Result::Continue;
}

CLCommandQueueVk::~CLCommandQueueVk()
{
    VkDevice vkDevice = mContext->getDevice();

    if (mCurrentQueueSerialIndex != kInvalidQueueSerialIndex)
    {
        mContext->getRenderer()->releaseQueueSerialIndex(mCurrentQueueSerialIndex);
        mCurrentQueueSerialIndex = kInvalidQueueSerialIndex;
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
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));

    if (blocking)
    {
        ANGLE_TRY(finishInternal());
        auto bufferVk = &buffer.getImpl<CLBufferVk>();
        ANGLE_TRY(bufferVk->copyTo(ptr, offset, size));
    }
    else
    {
        CLBufferVk &bufferVk = buffer.getImpl<CLBufferVk>();

        // Reached transfer buffer creation limit/heuristic, finish this current batch
        if (mHostBufferUpdateList.size() >= kMaxHostBufferUpdateListSize)
        {
            ANGLE_TRY(finishInternal());
        }

        // Create a transfer buffer and push it in update list
        mHostBufferUpdateList.emplace_back(
            cl::Buffer::Cast(this->mContext->getFrontendObject().createBuffer(
                nullptr, cl::MemFlags{buffer.getFlags().get() | CL_MEM_USE_HOST_PTR},
                buffer.getSize(), ptr)));
        if (mHostBufferUpdateList.back() == nullptr)
        {
            ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
        }
        CLBufferVk &transferBufferVk = mHostBufferUpdateList.back()->getImpl<CLBufferVk>();
        // Release initialization reference, lifetime controlled by RefPointer.
        mHostBufferUpdateList.back()->release();

        const VkBufferCopy copyRegion = {offset, offset, size};

        // We need an execution barrier if buffer can be written to by kernel
        if (!mComputePassCommands->getCommandBuffer().empty() && bufferVk.isWritable())
        {
            VkMemoryBarrier memoryBarrier = {
                VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
            mComputePassCommands->getCommandBuffer().pipelineBarrier(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                &memoryBarrier, 0, nullptr, 0, nullptr);
        }

        mComputePassCommands->getCommandBuffer().copyBuffer(
            bufferVk.getBuffer().getBuffer(), transferBufferVk.getBuffer().getBuffer(), 1,
            &copyRegion);
    }

    ANGLE_TRY(createEvent(eventCreateFunc));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueWriteBuffer(const cl::Buffer &buffer,
                                                   bool blocking,
                                                   size_t offset,
                                                   size_t size,
                                                   const void *ptr,
                                                   const cl::EventPtrs &waitEvents,
                                                   CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));

    auto bufferVk = &buffer.getImpl<CLBufferVk>();
    ANGLE_TRY(bufferVk->copyFrom(ptr, offset, size));
    if (blocking)
    {
        ANGLE_TRY(finishInternal());
    }

    ANGLE_TRY(createEvent(eventCreateFunc));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueReadBufferRect(const cl::Buffer &buffer,
                                                      bool blocking,
                                                      const cl::MemOffsets &bufferOrigin,
                                                      const cl::MemOffsets &hostOrigin,
                                                      const cl::Coordinate &region,
                                                      size_t bufferRowPitch,
                                                      size_t bufferSlicePitch,
                                                      size_t hostRowPitch,
                                                      size_t hostSlicePitch,
                                                      void *ptr,
                                                      const cl::EventPtrs &waitEvents,
                                                      CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueWriteBufferRect(const cl::Buffer &buffer,
                                                       bool blocking,
                                                       const cl::MemOffsets &bufferOrigin,
                                                       const cl::MemOffsets &hostOrigin,
                                                       const cl::Coordinate &region,
                                                       size_t bufferRowPitch,
                                                       size_t bufferSlicePitch,
                                                       size_t hostRowPitch,
                                                       size_t hostSlicePitch,
                                                       const void *ptr,
                                                       const cl::EventPtrs &waitEvents,
                                                       CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueCopyBuffer(const cl::Buffer &srcBuffer,
                                                  const cl::Buffer &dstBuffer,
                                                  size_t srcOffset,
                                                  size_t dstOffset,
                                                  size_t size,
                                                  const cl::EventPtrs &waitEvents,
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueCopyBufferRect(const cl::Buffer &srcBuffer,
                                                      const cl::Buffer &dstBuffer,
                                                      const cl::MemOffsets &srcOrigin,
                                                      const cl::MemOffsets &dstOrigin,
                                                      const cl::Coordinate &region,
                                                      size_t srcRowPitch,
                                                      size_t srcSlicePitch,
                                                      size_t dstRowPitch,
                                                      size_t dstSlicePitch,
                                                      const cl::EventPtrs &waitEvents,
                                                      CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueFillBuffer(const cl::Buffer &buffer,
                                                  const void *pattern,
                                                  size_t patternSize,
                                                  size_t offset,
                                                  size_t size,
                                                  const cl::EventPtrs &waitEvents,
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueMapBuffer(const cl::Buffer &buffer,
                                                 bool blocking,
                                                 cl::MapFlags mapFlags,
                                                 size_t offset,
                                                 size_t size,
                                                 const cl::EventPtrs &waitEvents,
                                                 CLEventImpl::CreateFunc *eventCreateFunc,
                                                 void *&mapPtr)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueReadImage(const cl::Image &image,
                                                 bool blocking,
                                                 const cl::MemOffsets &origin,
                                                 const cl::Coordinate &region,
                                                 size_t rowPitch,
                                                 size_t slicePitch,
                                                 void *ptr,
                                                 const cl::EventPtrs &waitEvents,
                                                 CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueWriteImage(const cl::Image &image,
                                                  bool blocking,
                                                  const cl::MemOffsets &origin,
                                                  const cl::Coordinate &region,
                                                  size_t inputRowPitch,
                                                  size_t inputSlicePitch,
                                                  const void *ptr,
                                                  const cl::EventPtrs &waitEvents,
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueCopyImage(const cl::Image &srcImage,
                                                 const cl::Image &dstImage,
                                                 const cl::MemOffsets &srcOrigin,
                                                 const cl::MemOffsets &dstOrigin,
                                                 const cl::Coordinate &region,
                                                 const cl::EventPtrs &waitEvents,
                                                 CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueFillImage(const cl::Image &image,
                                                 const void *fillColor,
                                                 const cl::MemOffsets &origin,
                                                 const cl::Coordinate &region,
                                                 const cl::EventPtrs &waitEvents,
                                                 CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueCopyImageToBuffer(const cl::Image &srcImage,
                                                         const cl::Buffer &dstBuffer,
                                                         const cl::MemOffsets &srcOrigin,
                                                         const cl::Coordinate &region,
                                                         size_t dstOffset,
                                                         const cl::EventPtrs &waitEvents,
                                                         CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueCopyBufferToImage(const cl::Buffer &srcBuffer,
                                                         const cl::Image &dstImage,
                                                         size_t srcOffset,
                                                         const cl::MemOffsets &dstOrigin,
                                                         const cl::Coordinate &region,
                                                         const cl::EventPtrs &waitEvents,
                                                         CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueMapImage(const cl::Image &image,
                                                bool blocking,
                                                cl::MapFlags mapFlags,
                                                const cl::MemOffsets &origin,
                                                const cl::Coordinate &region,
                                                size_t *imageRowPitch,
                                                size_t *imageSlicePitch,
                                                const cl::EventPtrs &waitEvents,
                                                CLEventImpl::CreateFunc *eventCreateFunc,
                                                void *&mapPtr)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueUnmapMemObject(const cl::Memory &memory,
                                                      void *mappedPtr,
                                                      const cl::EventPtrs &waitEvents,
                                                      CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueMigrateMemObjects(const cl::MemoryPtrs &memObjects,
                                                         cl::MemMigrationFlags flags,
                                                         const cl::EventPtrs &waitEvents,
                                                         CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueNDRangeKernel(const cl::Kernel &kernel,
                                                     const cl::NDRange &ndrange,
                                                     const cl::EventPtrs &waitEvents,
                                                     CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));

    cl::WorkgroupCount workgroupCount;
    vk::PipelineCacheAccess pipelineCache;
    vk::PipelineHelper *pipelineHelper = nullptr;
    CLKernelVk &kernelImpl             = kernel.getImpl<CLKernelVk>();

    ANGLE_TRY(processKernelResources(kernelImpl, ndrange));

    // Fetch or create compute pipeline (if we miss in cache)
    ANGLE_CL_IMPL_TRY_ERROR(mContext->getRenderer()->getPipelineCache(mContext, &pipelineCache),
                            CL_OUT_OF_RESOURCES);
    ANGLE_TRY(kernelImpl.getOrCreateComputePipeline(
        &pipelineCache, ndrange, mCommandQueue.getDevice(), &pipelineHelper, &workgroupCount));

    mComputePassCommands->retainResource(pipelineHelper);
    mComputePassCommands->getCommandBuffer().bindComputePipeline(pipelineHelper->getPipeline());
    mComputePassCommands->getCommandBuffer().dispatch(workgroupCount[0], workgroupCount[1],
                                                      workgroupCount[2]);

    ANGLE_TRY(createEvent(eventCreateFunc));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueTask(const cl::Kernel &kernel,
                                            const cl::EventPtrs &waitEvents,
                                            CLEventImpl::CreateFunc *eventCreateFunc)
{
    constexpr size_t globalWorkSize[3] = {1, 0, 0};
    constexpr size_t localWorkSize[3]  = {1, 0, 0};
    cl::NDRange ndrange(1, nullptr, globalWorkSize, localWorkSize);
    return enqueueNDRangeKernel(kernel, ndrange, waitEvents, eventCreateFunc);
}

angle::Result CLCommandQueueVk::enqueueNativeKernel(cl::UserFunc userFunc,
                                                    void *args,
                                                    size_t cbArgs,
                                                    const cl::BufferPtrs &buffers,
                                                    const std::vector<size_t> bufferPtrOffsets,
                                                    const cl::EventPtrs &waitEvents,
                                                    CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueMarkerWithWaitList(const cl::EventPtrs &waitEvents,
                                                          CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));
    ANGLE_TRY(createEvent(eventCreateFunc));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueMarker(CLEventImpl::CreateFunc &eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    // This deprecated API is essentially a super-set of clEnqueueBarrier, where we also return an
    // event object (i.e. marker) since clEnqueueBarrier does not provide this
    VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                     VK_ACCESS_SHADER_WRITE_BIT,
                                     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
    mComputePassCommands->getCommandBuffer().pipelineBarrier(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
        &memoryBarrier, 0, nullptr, 0, nullptr);

    ANGLE_TRY(createEvent(&eventCreateFunc));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueWaitForEvents(const cl::EventPtrs &events)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    // Unlike clWaitForEvents, this routine is non-blocking
    ANGLE_TRY(processWaitlist(events));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueBarrierWithWaitList(const cl::EventPtrs &waitEvents,
                                                           CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    // The barrier command either waits for a list of events to complete, or if the list is empty it
    // waits for all commands previously enqueued in command_queue to complete before it completes
    if (waitEvents.empty())
    {
        VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                         VK_ACCESS_SHADER_WRITE_BIT,
                                         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
        mComputePassCommands->getCommandBuffer().pipelineBarrier(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
            &memoryBarrier, 0, nullptr, 0, nullptr);
    }
    else
    {
        ANGLE_TRY(processWaitlist(waitEvents));
    }

    ANGLE_TRY(createEvent(eventCreateFunc));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueBarrier()
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                     VK_ACCESS_SHADER_WRITE_BIT,
                                     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
    mComputePassCommands->getCommandBuffer().pipelineBarrier(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
        &memoryBarrier, 0, nullptr, 0, nullptr);

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::flush()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::flush");

    // Non-blocking finish
    // TODO: Ideally we should try to find better impl. to avoid spawning a submit-thread/Task here
    // https://anglebug.com/8669
    std::shared_ptr<angle::WaitableEvent> asyncEvent =
        getPlatform()->postMultiThreadWorkerTask(std::make_shared<CLAsyncFinishTask>(this));
    ASSERT(asyncEvent != nullptr);

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::finish()
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::finish");

    // Blocking finish
    return finishInternal();
}

angle::Result CLCommandQueueVk::syncHostBuffers()
{
    for (const cl::MemoryPtr &memoryPtr : mHostBufferUpdateList)
    {
        ASSERT(memoryPtr->getHostPtr() != nullptr);
        CLBufferVk &bufferVk = memoryPtr->getImpl<CLBufferVk>();
        ANGLE_TRY(
            bufferVk.copyTo(memoryPtr->getHostPtr(), memoryPtr->getOffset(), memoryPtr->getSize()));
    }
    mHostBufferUpdateList.clear();

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::processKernelResources(CLKernelVk &kernelVk,
                                                       const cl::NDRange &ndrange)
{
    bool needsBarrier = false;
    UpdateDescriptorSetsBuilder updateDescriptorSetsBuilder;
    const CLProgramVk::DeviceProgramData *devProgramData =
        kernelVk.getProgram()->getDeviceProgramData(mCommandQueue.getDevice().getNative());
    ASSERT(devProgramData != nullptr);

    // Allocate descriptor set
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    ANGLE_TRY(kernelVk.getProgram()->allocateDescriptorSet(
        kernelVk.getDescriptorSetLayouts()[DescriptorSetIndex::ShaderResource].get(),
        &descriptorSet));

    // Push global offset data
    const VkPushConstantRange *globalOffsetRange = devProgramData->getGlobalOffsetRange();
    if (globalOffsetRange != nullptr)
    {
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout().get(), VK_SHADER_STAGE_COMPUTE_BIT,
            globalOffsetRange->offset, globalOffsetRange->size, ndrange.globalWorkOffset.data());
    }

    // Push global size data
    const VkPushConstantRange *globalSizeRange = devProgramData->getGlobalSizeRange();
    if (globalSizeRange != nullptr)
    {
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout().get(), VK_SHADER_STAGE_COMPUTE_BIT,
            globalSizeRange->offset, globalSizeRange->size, ndrange.globalWorkSize.data());
    }

    // Process each kernel argument/resource
    for (const auto &arg : kernelVk.getArgs())
    {
        switch (arg.type)
        {
            case NonSemanticClspvReflectionArgumentUniform:
            case NonSemanticClspvReflectionArgumentStorageBuffer:
            {
                cl::Memory *clMem = cl::Buffer::Cast(*static_cast<const cl_mem *>(arg.handle));
                CLBufferVk &vkMem = clMem->getImpl<CLBufferVk>();

                // Retain this resource until its associated dispatch completes
                mMemoryCaptures.emplace_back(clMem);

                // Handle possible resource RAW hazard
                if (arg.type != NonSemanticClspvReflectionArgumentUniform)
                {
                    if (mDependencyTracker.contains(clMem) ||
                        mDependencyTracker.size() == kMaxDependencyTrackerSize)
                    {
                        needsBarrier = true;
                        mDependencyTracker.clear();
                    }
                    mDependencyTracker.insert(clMem);
                }

                // Update buffer/descriptor info
                VkDescriptorBufferInfo &bufferInfo =
                    updateDescriptorSetsBuilder.allocDescriptorBufferInfo();
                bufferInfo.range  = clMem->getSize();
                bufferInfo.offset = clMem->getOffset();
                bufferInfo.buffer = vkMem.isSubBuffer()
                                        ? vkMem.getParent()->getBuffer().getBuffer().getHandle()
                                        : vkMem.getBuffer().getBuffer().getHandle();
                VkWriteDescriptorSet &writeDescriptorSet =
                    updateDescriptorSetsBuilder.allocWriteDescriptorSet();
                writeDescriptorSet.descriptorCount = 1;
                writeDescriptorSet.descriptorType =
                    arg.type == NonSemanticClspvReflectionArgumentUniform
                        ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                        : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                writeDescriptorSet.pBufferInfo = &bufferInfo;
                writeDescriptorSet.sType       = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet      = descriptorSet;
                writeDescriptorSet.dstBinding  = arg.descriptorBinding;
                break;
            }
            case NonSemanticClspvReflectionArgumentPodPushConstant:
            {
                mComputePassCommands->getCommandBuffer().pushConstants(
                    kernelVk.getPipelineLayout().get(), VK_SHADER_STAGE_COMPUTE_BIT,
                    arg.pushConstOffset, arg.pushConstantSize, arg.handle);
                break;
            }
            case NonSemanticClspvReflectionArgumentSampler:
            case NonSemanticClspvReflectionArgumentPodUniform:
            case NonSemanticClspvReflectionArgumentStorageImage:
            case NonSemanticClspvReflectionArgumentSampledImage:
            case NonSemanticClspvReflectionArgumentPointerUniform:
            case NonSemanticClspvReflectionArgumentPodStorageBuffer:
            case NonSemanticClspvReflectionArgumentUniformTexelBuffer:
            case NonSemanticClspvReflectionArgumentStorageTexelBuffer:
            case NonSemanticClspvReflectionArgumentPointerPushConstant:
            default:
            {
                UNIMPLEMENTED();
                break;
            }
        }
    }

    if (needsBarrier)
    {
        VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                         VK_ACCESS_SHADER_WRITE_BIT,
                                         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
        mComputePassCommands->getCommandBuffer().pipelineBarrier(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
            &memoryBarrier, 0, nullptr, 0, nullptr);
    }

    mContext->getPerfCounters().writeDescriptorSets =
        updateDescriptorSetsBuilder.flushDescriptorSetUpdates(mContext->getRenderer()->getDevice());

    mComputePassCommands->getCommandBuffer().bindDescriptorSets(
        kernelVk.getPipelineLayout().get(), VK_PIPELINE_BIND_POINT_COMPUTE,
        DescriptorSetIndex::Internal, 1, &descriptorSet, 0, nullptr);

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::flushComputePassCommands()
{
    mLastFlushedQueueSerial = mComputePassCommands->getQueueSerial();

    // Here, we flush our compute cmds to RendererVk's primary command buffer
    ANGLE_TRY(mContext->getRenderer()->flushOutsideRPCommands(
        mContext, getProtectionType(), egl::ContextPriority::Medium, &mComputePassCommands));

    mHasAnyCommandsPendingSubmission = true;

    mContext->getPerfCounters().flushedOutsideRenderPassCommandBuffers++;

    // Generate new serial for next batch of cmds
    mComputePassCommands->setQueueSerial(
        mCurrentQueueSerialIndex,
        mContext->getRenderer()->generateQueueSerial(mCurrentQueueSerialIndex));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::processWaitlist(const cl::EventPtrs &waitEvents)
{
    if (!waitEvents.empty())
    {
        bool insertedBarrier = false;
        for (const cl::EventPtr &event : waitEvents)
        {
            if (event->getImpl<CLEventVk>().isUserEvent() ||
                event->getCommandQueue() != &mCommandQueue)
            {
                // We cannot use a barrier in these cases, therefore defer the event
                // handling till submission time
                // TODO: Perhaps we could utilize VkEvents here instead and have GPU wait(s)
                // https://anglebug.com/8670
                mDependantEvents.push_back(event);
            }
            else if (event->getCommandQueue() == &mCommandQueue && !insertedBarrier)
            {
                // As long as there is at least one dependant command in same queue,
                // we just need to insert one execution barrier
                VkMemoryBarrier memoryBarrier = {
                    VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
                mComputePassCommands->getCommandBuffer().pipelineBarrier(
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                    1, &memoryBarrier, 0, nullptr, 0, nullptr);

                insertedBarrier = true;
            }
        }
    }
    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::submitCommands()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::submitCommands()");

    // Kick off renderer submit
    ANGLE_TRY(mContext->getRenderer()->submitCommands(mContext, getProtectionType(),
                                                      egl::ContextPriority::Medium, nullptr,
                                                      nullptr, mLastFlushedQueueSerial));

    mLastSubmittedQueueSerial = mLastFlushedQueueSerial;

    // Now that we have submitted commands, some of pending garbage may no longer pending
    // and should be moved to garbage list.
    mContext->getRenderer()->cleanupPendingSubmissionGarbage();

    mHasAnyCommandsPendingSubmission = false;

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::createEvent(CLEventImpl::CreateFunc *createFunc)
{
    if (createFunc != nullptr)
    {
        *createFunc = [this](const cl::Event &event) {
            auto eventVk = new (std::nothrow) CLEventVk(event);
            if (eventVk == nullptr)
            {
                ERR() << "Failed to create event obj!";
                ANGLE_CL_SET_ERROR(CL_OUT_OF_HOST_MEMORY);
                return CLEventImpl::Ptr(nullptr);
            }
            eventVk->setQueueSerial(mComputePassCommands->getQueueSerial());

            // Save a reference to this event
            mAssociatedEvents.push_back(cl::EventPtr{&eventVk->getFrontendObject()});

            return CLEventImpl::Ptr(eventVk);
        };
    }
    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::finishInternal()
{
    for (cl::EventPtr event : mAssociatedEvents)
    {
        ANGLE_TRY(event->getImpl<CLEventVk>().setStatusAndExecuteCallback(CL_SUBMITTED));
    }

    if (!mComputePassCommands->empty())
    {
        // If we still have dependant events, handle them now
        if (!mDependantEvents.empty())
        {
            for (const auto &depEvent : mDependantEvents)
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
                        mComputePassCommands->getCommandBuffer().reset();
                        ANGLE_CL_RETURN_ERROR(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
                    }
                }
                else
                {
                    // Otherwise, we just need to submit/finish for dependant event queues
                    // here that are not associated with this queue
                    ANGLE_TRY(depEvent->getCommandQueue()->finish());
                }
            }
            mDependantEvents.clear();
        }

        ANGLE_TRY(flushComputePassCommands());
    }

    for (cl::EventPtr event : mAssociatedEvents)
    {
        ANGLE_TRY(event->getImpl<CLEventVk>().setStatusAndExecuteCallback(CL_RUNNING));
    }

    if (mHasAnyCommandsPendingSubmission)
    {
        // Submit and wait for fence
        ANGLE_TRY(submitCommands());
        ANGLE_TRY(mContext->getRenderer()->finishQueueSerial(mContext, mLastSubmittedQueueSerial));

        // Ensure any resources are synced back to host on GPU completion
        ANGLE_TRY(syncHostBuffers());
    }

    for (cl::EventPtr event : mAssociatedEvents)
    {
        ANGLE_TRY(event->getImpl<CLEventVk>().setStatusAndExecuteCallback(CL_COMPLETE));
    }

    mMemoryCaptures.clear();
    mAssociatedEvents.clear();
    mDependencyTracker.clear();

    return angle::Result::Continue;
}

}  // namespace rx
