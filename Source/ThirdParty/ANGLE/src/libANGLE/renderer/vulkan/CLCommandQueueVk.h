//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLCommandQueueVk.h: Defines the class interface for CLCommandQueueVk,
// implementing CLCommandQueueImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLCOMMANDQUEUEVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLCOMMANDQUEUEVK_H_

#include <vector>

#include "common/PackedCLEnums_autogen.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLEventVk.h"
#include "libANGLE/renderer/vulkan/CLKernelVk.h"
#include "libANGLE/renderer/vulkan/CLMemoryVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/ShareGroupVk.h"
#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/clspv_utils.h"
#include "libANGLE/renderer/vulkan/vk_command_buffer_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_resource.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

#include "libANGLE/renderer/CLCommandQueueImpl.h"

namespace rx
{

static constexpr size_t kPrintfBufferSize = 1024 * 1024;

class CLCommandQueueVk : public CLCommandQueueImpl
{
  public:
    CLCommandQueueVk(const cl::CommandQueue &commandQueue);
    ~CLCommandQueueVk() override;

    angle::Result init();

    angle::Result setProperty(cl::CommandQueueProperties properties, cl_bool enable) override;

    angle::Result enqueueReadBuffer(const cl::Buffer &buffer,
                                    bool blocking,
                                    size_t offset,
                                    size_t size,
                                    void *ptr,
                                    const cl::EventPtrs &waitEvents,
                                    CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueWriteBuffer(const cl::Buffer &buffer,
                                     bool blocking,
                                     size_t offset,
                                     size_t size,
                                     const void *ptr,
                                     const cl::EventPtrs &waitEvents,
                                     CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueReadBufferRect(const cl::Buffer &buffer,
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
                                        CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueWriteBufferRect(const cl::Buffer &buffer,
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
                                         CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueCopyBuffer(const cl::Buffer &srcBuffer,
                                    const cl::Buffer &dstBuffer,
                                    size_t srcOffset,
                                    size_t dstOffset,
                                    size_t size,
                                    const cl::EventPtrs &waitEvents,
                                    CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueCopyBufferRect(const cl::Buffer &srcBuffer,
                                        const cl::Buffer &dstBuffer,
                                        const cl::MemOffsets &srcOrigin,
                                        const cl::MemOffsets &dstOrigin,
                                        const cl::Coordinate &region,
                                        size_t srcRowPitch,
                                        size_t srcSlicePitch,
                                        size_t dstRowPitch,
                                        size_t dstSlicePitch,
                                        const cl::EventPtrs &waitEvents,
                                        CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueFillBuffer(const cl::Buffer &buffer,
                                    const void *pattern,
                                    size_t patternSize,
                                    size_t offset,
                                    size_t size,
                                    const cl::EventPtrs &waitEvents,
                                    CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueMapBuffer(const cl::Buffer &buffer,
                                   bool blocking,
                                   cl::MapFlags mapFlags,
                                   size_t offset,
                                   size_t size,
                                   const cl::EventPtrs &waitEvents,
                                   CLEventImpl::CreateFunc *eventCreateFunc,
                                   void *&mapPtr) override;

    angle::Result enqueueReadImage(const cl::Image &image,
                                   bool blocking,
                                   const cl::MemOffsets &origin,
                                   const cl::Coordinate &region,
                                   size_t rowPitch,
                                   size_t slicePitch,
                                   void *ptr,
                                   const cl::EventPtrs &waitEvents,
                                   CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueWriteImage(const cl::Image &image,
                                    bool blocking,
                                    const cl::MemOffsets &origin,
                                    const cl::Coordinate &region,
                                    size_t inputRowPitch,
                                    size_t inputSlicePitch,
                                    const void *ptr,
                                    const cl::EventPtrs &waitEvents,
                                    CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueCopyImage(const cl::Image &srcImage,
                                   const cl::Image &dstImage,
                                   const cl::MemOffsets &srcOrigin,
                                   const cl::MemOffsets &dstOrigin,
                                   const cl::Coordinate &region,
                                   const cl::EventPtrs &waitEvents,
                                   CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueFillImage(const cl::Image &image,
                                   const void *fillColor,
                                   const cl::MemOffsets &origin,
                                   const cl::Coordinate &region,
                                   const cl::EventPtrs &waitEvents,
                                   CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueCopyImageToBuffer(const cl::Image &srcImage,
                                           const cl::Buffer &dstBuffer,
                                           const cl::MemOffsets &srcOrigin,
                                           const cl::Coordinate &region,
                                           size_t dstOffset,
                                           const cl::EventPtrs &waitEvents,
                                           CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueCopyBufferToImage(const cl::Buffer &srcBuffer,
                                           const cl::Image &dstImage,
                                           size_t srcOffset,
                                           const cl::MemOffsets &dstOrigin,
                                           const cl::Coordinate &region,
                                           const cl::EventPtrs &waitEvents,
                                           CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueMapImage(const cl::Image &image,
                                  bool blocking,
                                  cl::MapFlags mapFlags,
                                  const cl::MemOffsets &origin,
                                  const cl::Coordinate &region,
                                  size_t *imageRowPitch,
                                  size_t *imageSlicePitch,
                                  const cl::EventPtrs &waitEvents,
                                  CLEventImpl::CreateFunc *eventCreateFunc,
                                  void *&mapPtr) override;

    angle::Result enqueueUnmapMemObject(const cl::Memory &memory,
                                        void *mappedPtr,
                                        const cl::EventPtrs &waitEvents,
                                        CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueMigrateMemObjects(const cl::MemoryPtrs &memObjects,
                                           cl::MemMigrationFlags flags,
                                           const cl::EventPtrs &waitEvents,
                                           CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueNDRangeKernel(const cl::Kernel &kernel,
                                       const cl::NDRange &ndrange,
                                       const cl::EventPtrs &waitEvents,
                                       CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueTask(const cl::Kernel &kernel,
                              const cl::EventPtrs &waitEvents,
                              CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueNativeKernel(cl::UserFunc userFunc,
                                      void *args,
                                      size_t cbArgs,
                                      const cl::BufferPtrs &buffers,
                                      const std::vector<size_t> bufferPtrOffsets,
                                      const cl::EventPtrs &waitEvents,
                                      CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueMarkerWithWaitList(const cl::EventPtrs &waitEvents,
                                            CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueMarker(CLEventImpl::CreateFunc &eventCreateFunc) override;

    angle::Result enqueueWaitForEvents(const cl::EventPtrs &events) override;

    angle::Result enqueueBarrierWithWaitList(const cl::EventPtrs &waitEvents,
                                             CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueBarrier() override;

    angle::Result flush() override;

    angle::Result finish() override;

    CLPlatformVk *getPlatform() { return mContext->getPlatform(); }

    cl_mem getOrCreatePrintfBuffer();

  private:
    static constexpr size_t kMaxDependencyTrackerSize    = 64;
    static constexpr size_t kMaxHostBufferUpdateListSize = 16;

    vk::ProtectionType getProtectionType() const { return vk::ProtectionType::Unprotected; }

    // Create-update-bind the kernel's descriptor set, put push-constants in cmd buffer, capture
    // kernel resources, and handle kernel execution dependencies
    angle::Result processKernelResources(CLKernelVk &kernelVk,
                                         const cl::NDRange &ndrange,
                                         const cl::WorkgroupCount &workgroupCount);

    angle::Result submitCommands();
    angle::Result finishInternal();
    angle::Result syncHostBuffers();
    angle::Result flushComputePassCommands();
    angle::Result processWaitlist(const cl::EventPtrs &waitEvents);
    angle::Result createEvent(CLEventImpl::CreateFunc *createFunc,
                              cl::ExecutionStatus initialStatus);

    angle::Result onResourceAccess(const vk::CommandBufferAccess &access);
    angle::Result getCommandBuffer(const vk::CommandBufferAccess &access,
                                   vk::OutsideRenderPassCommandBuffer **commandBufferOut)
    {
        ANGLE_TRY(onResourceAccess(access));
        *commandBufferOut = &mComputePassCommands->getCommandBuffer();
        return angle::Result::Continue;
    }

    angle::Result processPrintfBuffer();
    angle::Result copyImageToFromBuffer(CLImageVk &imageVk,
                                        vk::BufferHelper &buffer,
                                        const cl::MemOffsets &origin,
                                        const cl::Coordinate &region,
                                        size_t bufferOffset,
                                        ImageBufferCopyDirection writeToBuffer);

    bool hasUserEventDependency() const;

    angle::Result insertBarrier();
    angle::Result addMemoryDependencies(cl::Memory *clMem);

    CLContextVk *mContext;
    const CLDeviceVk *mDevice;
    cl::Memory *mPrintfBuffer;

    vk::SecondaryCommandPools mCommandPool;
    vk::OutsideRenderPassCommandBufferHelper *mComputePassCommands;
    vk::SecondaryCommandMemoryAllocator mOutsideRenderPassCommandsAllocator;
    SerialIndex mCurrentQueueSerialIndex;
    QueueSerial mLastSubmittedQueueSerial;
    QueueSerial mLastFlushedQueueSerial;
    std::mutex mCommandQueueMutex;

    // Created event objects associated with this command queue
    cl::EventPtrs mAssociatedEvents;

    // Dependant event(s) that this queue has to wait on
    cl::EventPtrs mDependantEvents;

    // Keep track of kernel resources on prior kernel enqueues
    angle::HashSet<cl::Object *> mDependencyTracker;

    // Resource reference capturing during execution
    cl::MemoryPtrs mMemoryCaptures;
    cl::KernelPtrs mKernelCaptures;

    // Check to see if flush/finish can be skipped
    bool mHasAnyCommandsPendingSubmission;

    // printf handling
    bool mNeedPrintfHandling;
    const angle::HashMap<uint32_t, ClspvPrintfInfo> *mPrintfInfos;

    // Host buffer transferring utility
    struct HostTransferConfig
    {
        cl_command_type type{0};
        size_t size            = 0;
        size_t offset          = 0;
        void *dstHostPtr       = nullptr;
        const void *srcHostPtr = nullptr;
        cl::MemOffsets origin;
        cl::Coordinate region;
    };
    struct HostTransferEntry
    {
        HostTransferConfig transferConfig;
        cl::MemoryPtr transferBufferHandle;
    };
    using HostTransferEntries = std::vector<HostTransferEntry>;
    HostTransferEntries mHostTransferList;
    angle::Result addToHostTransferList(CLBufferVk *srcBuffer, HostTransferConfig transferEntry);
    angle::Result addToHostTransferList(CLImageVk *srcImage, HostTransferConfig transferEntry);
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLCOMMANDQUEUEVK_H_
