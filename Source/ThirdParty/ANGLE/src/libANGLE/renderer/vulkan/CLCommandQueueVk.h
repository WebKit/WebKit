//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLCommandQueueVk.h: Defines the class interface for CLCommandQueueVk,
// implementing CLCommandQueueImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLCOMMANDQUEUEVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLCOMMANDQUEUEVK_H_

#include <condition_variable>
#include <vector>

#include "common/PackedCLEnums_autogen.h"
#include "common/hash_containers.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLEventVk.h"
#include "libANGLE/renderer/vulkan/CLKernelVk.h"
#include "libANGLE/renderer/vulkan/CLMemoryVk.h"
#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/clspv_utils.h"
#include "libANGLE/renderer/vulkan/vk_command_buffer_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

#include "libANGLE/renderer/CLCommandQueueImpl.h"
#include "libANGLE/renderer/serial_utils.h"

#include "libANGLE/CLKernel.h"
#include "libANGLE/CLMemory.h"
#include "libANGLE/cl_types.h"

namespace std
{
// Hash function for QueueSerial so that it can serve as a key for angle::HashMap
template <>
struct hash<rx::QueueSerial>
{
    size_t operator()(const rx::QueueSerial &queueSerial) const
    {
        size_t hash = 0;
        angle::HashCombine(hash, queueSerial.getSerial().getValue());
        angle::HashCombine(hash, queueSerial.getIndex());
        return hash;
    }
};
}  // namespace std

namespace rx
{

static constexpr size_t kPrintfBufferSize = 1024 * 1024;
class CLCommandQueueVk;

namespace
{
template <typename T>
class HostTransferConfig
{
  public:
    // HostTransferConfig is only relevant for certain commands that has host ptr involved, its
    // state is only setup for those cases.
    HostTransferConfig(cl_command_type type, size_t size, size_t offset, T *ptr)
        : mType(type),
          mSize(size),
          mOffset(offset),
          mHostPtr(ptr),
          mBufferRect(cl::Offset{}, cl::Extents{}, 0, 0, 0),
          mHostRect(cl::Offset{}, cl::Extents{}, 0, 0, 0)
    {
        ASSERT(type == CL_COMMAND_READ_BUFFER || type == CL_COMMAND_WRITE_BUFFER);
    }
    HostTransferConfig(cl_command_type type,
                       size_t size,
                       T *ptr,
                       cl::BufferRect bufferRect,
                       cl::BufferRect hostRect)
        : mType(type), mSize(size), mHostPtr(ptr), mBufferRect(bufferRect), mHostRect(hostRect)
    {
        ASSERT(type == CL_COMMAND_READ_BUFFER_RECT || type == CL_COMMAND_WRITE_BUFFER_RECT);
    }
    HostTransferConfig(cl_command_type type,
                       size_t size,
                       size_t offset,
                       T *pattern,
                       size_t patternSize)
        : mType(type),
          mSize(size),
          mOffset(offset),
          mHostPtr(pattern),
          mPatternSize(patternSize),
          mBufferRect(cl::Offset{}, cl::Extents{}, 0, 0, 0),
          mHostRect(cl::Offset{}, cl::Extents{}, 0, 0, 0)
    {
        ASSERT(type == CL_COMMAND_FILL_BUFFER);
    }
    HostTransferConfig(cl_command_type type,
                       size_t size,
                       void *ptr,
                       size_t rowPitch,
                       size_t slicePitch,
                       size_t elementSize,
                       cl::MemOffsets origin,
                       cl::Coordinate region)
        : mType(type),
          mSize(size),
          mHostPtr(ptr),
          mRowPitch(rowPitch),
          mSlicePitch(slicePitch),
          mElementSize(elementSize),
          mOrigin(origin),
          mRegion(region),
          mBufferRect(cl::Offset{}, cl::Extents{}, 0, 0, 0),
          mHostRect(cl::Offset{}, cl::Extents{}, 0, 0, 0)
    {
        ASSERT(type == CL_COMMAND_READ_IMAGE);
    }

    cl_command_type getType() const { return mType; }
    size_t getSize() const { return mSize; }
    size_t getOffset() const { return mOffset; }
    T *getHostPtr() const { return mHostPtr; }
    size_t getPatternSize() const { return mPatternSize; }
    size_t getRowPitch() const { return mRowPitch; }
    size_t getSlicePitch() const { return mSlicePitch; }
    size_t getElementSize() const { return mElementSize; }
    const cl::MemOffsets &getMemOffsets() const { return mOrigin; }
    const cl::Coordinate &getRegion() const { return mRegion; }
    const cl::BufferRect &getBufferRect() const { return mBufferRect; }
    const cl::BufferRect &getHostRect() const { return mHostRect; }

  private:
    cl_command_type mType{0};
    size_t mSize   = 0;
    size_t mOffset = 0;

    T *mHostPtr = nullptr;

    size_t mPatternSize    = 0;
    size_t mRowPitch       = 0;
    size_t mSlicePitch     = 0;
    size_t mElementSize    = 0;
    cl::MemOffsets mOrigin = cl::kMemOffsetsZero;
    cl::Coordinate mRegion = cl::kCoordinateZero;
    cl::BufferRect mBufferRect;
    cl::BufferRect mHostRect;
};

// We use HostTransferConfig for enqueueing a staged op for read/write operations to/from host
// pointers. As such we set up two instances one as const void and other as void.
using HostWriteTransferConfig = HostTransferConfig<const void>;
using HostReadTransferConfig  = HostTransferConfig<void>;
struct HostTransferEntry
{
    std::variant<HostReadTransferConfig, HostWriteTransferConfig> transferConfig;
    cl::MemoryPtr transferBufferHandle;
};
using HostTransferEntries = std::vector<HostTransferEntry>;

// DispatchWorkThread setups a background thread to wait on the work submitted to Vulkan renderer.
class DispatchWorkThread
{
  public:
    DispatchWorkThread(CLCommandQueueVk *commandQueue);
    ~DispatchWorkThread();

    angle::Result init();
    void terminate();

    angle::Result notify(QueueSerial queueSerial);

  private:
    static constexpr size_t kFixedQueueLimit = 4u;

    angle::Result finishLoop();

    CLCommandQueueVk *const mCommandQueue;

    std::mutex mThreadMutex;
    std::condition_variable mHasWorkSubmitted;
    std::condition_variable mHasEmptySlot;
    bool mIsTerminating;
    std::thread mWorkerThread;

    angle::FixedQueue<QueueSerial> mQueueSerials;
    // Queue serial index associated with the CLCommandQueueVk
    SerialIndex mQueueSerialIndex;
};

struct CommandsState
{
    cl::EventPtrs events;
    cl::MemoryPtrs memories;
    cl::KernelPtrs kernels;
    cl::SamplerPtrs samplers;
    HostTransferEntries hostTransferList;
};
using CommandsStateMap = angle::HashMap<QueueSerial, CommandsState>;

}  // namespace

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
                                      const std::vector<size_t> &bufferPtrOffsets,
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

    angle::Result enqueueAcquireExternalMemObjectsKHR(
        const cl::MemoryPtrs &memObjects,
        const cl::EventPtrs &waitEvents,
        CLEventImpl::CreateFunc *eventCreateFunc) override;

    angle::Result enqueueReleaseExternalMemObjectsKHR(
        const cl::MemoryPtrs &memObjects,
        const cl::EventPtrs &waitEvents,
        CLEventImpl::CreateFunc *eventCreateFunc) override;

    CLPlatformVk *getPlatform() { return mContext->getPlatform(); }
    CLContextVk *getContext() { return mContext; }

    cl::MemoryPtr getOrCreatePrintfBuffer();

    angle::Result finishQueueSerial(const QueueSerial queueSerial);

    SerialIndex getQueueSerialIndex() const { return mQueueSerialIndex; }

    bool hasCommandsPendingSubmission() const
    {
        return mLastFlushedQueueSerial != mLastSubmittedQueueSerial;
    }

    void addEventReference(CLEventVk &eventVk);

  private:
    static constexpr size_t kMaxDependencyTrackerSize    = 64;
    static constexpr size_t kMaxHostBufferUpdateListSize = 16;

    angle::Result resetCommandBufferWithError(cl_int errorCode);

    vk::ProtectionType getProtectionType() const { return vk::ProtectionType::Unprotected; }

    // Create-update-bind the kernel's descriptor set, put push-constants in cmd buffer, capture
    // kernel resources, and handle kernel execution dependencies
    angle::Result processKernelResources(CLKernelVk &kernelVk);
    // Updates global push constants for a given CL kernel
    angle::Result processGlobalPushConstants(CLKernelVk &kernelVk, const cl::NDRange &ndrange);

    angle::Result submitCommands();
    angle::Result finishInternal();
    angle::Result flushInternal();
    // Wait for the submitted work to the renderer to finish and perform post-processing such as
    // event status updates etc. This is a blocking call.
    angle::Result finishQueueSerialInternal(const QueueSerial queueSerial);

    angle::Result syncPendingHostTransfers(HostTransferEntries &hostTransferList);
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
    angle::Result addMemoryDependencies(const CLKernelArgument *arg);
    enum class MemoryHandleAccess
    {
        ReadOnly,
        Writeable,
    };
    angle::Result addMemoryDependencies(cl::Memory *mem, MemoryHandleAccess access);

    angle::Result submitEmptyCommand();

    CLContextVk *mContext;
    const CLDeviceVk *mDevice;
    cl::Memory *mPrintfBuffer;

    vk::SecondaryCommandPools mCommandPool;
    vk::OutsideRenderPassCommandBufferHelper *mComputePassCommands;

    // Queue Serials for this command queue
    SerialIndex mQueueSerialIndex;
    QueueSerial mLastSubmittedQueueSerial;
    QueueSerial mLastFlushedQueueSerial;

    std::mutex mCommandQueueMutex;

    // External dependent events that this queue has to wait on
    cl::EventPtrs mExternalEvents;

    // Keep track of kernel resources on prior kernel enqueues
    angle::HashSet<cl::Object *> mWriteDependencyTracker;
    angle::HashSet<cl::Object *> mReadDependencyTracker;

    CommandsStateMap mCommandsStateMap;

    // printf handling
    bool mNeedPrintfHandling;
    const angle::HashMap<uint32_t, ClspvPrintfInfo> *mPrintfInfos;

    // Host buffer transferring routines
    template <class T>
    angle::Result addToHostTransferList(CLBufferVk *srcBuffer, HostTransferConfig<T> transferEntry);
    template <class T>
    angle::Result addToHostTransferList(CLImageVk *srcImage, HostTransferConfig<T> transferEntry);

    DispatchWorkThread mFinishHandler;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLCOMMANDQUEUEVK_H_
