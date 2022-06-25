//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLCommandQueue.h: Defines the cl::CommandQueue class, which can be used to queue a set of OpenCL
// operations.

#ifndef LIBANGLE_CLCOMMANDQUEUE_H_
#define LIBANGLE_CLCOMMANDQUEUE_H_

#include "libANGLE/CLObject.h"
#include "libANGLE/renderer/CLCommandQueueImpl.h"

#include "common/SynchronizedValue.h"

#include <limits>

namespace cl
{

class CommandQueue final : public _cl_command_queue, public Object
{
  public:
    // Front end entry functions, only called from OpenCL entry points

    cl_int getInfo(CommandQueueInfo name,
                   size_t valueSize,
                   void *value,
                   size_t *valueSizeRet) const;

    cl_int setProperty(CommandQueueProperties properties,
                       cl_bool enable,
                       cl_command_queue_properties *oldProperties);

    cl_int enqueueReadBuffer(cl_mem buffer,
                             cl_bool blockingRead,
                             size_t offset,
                             size_t size,
                             void *ptr,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event);

    cl_int enqueueWriteBuffer(cl_mem buffer,
                              cl_bool blockingWrite,
                              size_t offset,
                              size_t size,
                              const void *ptr,
                              cl_uint numEventsInWaitList,
                              const cl_event *eventWaitList,
                              cl_event *event);

    cl_int enqueueReadBufferRect(cl_mem buffer,
                                 cl_bool blockingRead,
                                 const size_t *bufferOrigin,
                                 const size_t *hostOrigin,
                                 const size_t *region,
                                 size_t bufferRowPitch,
                                 size_t bufferSlicePitch,
                                 size_t hostRowPitch,
                                 size_t hostSlicePitch,
                                 void *ptr,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event);

    cl_int enqueueWriteBufferRect(cl_mem buffer,
                                  cl_bool blockingWrite,
                                  const size_t *bufferOrigin,
                                  const size_t *hostOrigin,
                                  const size_t *region,
                                  size_t bufferRowPitch,
                                  size_t bufferSlicePitch,
                                  size_t hostRowPitch,
                                  size_t hostSlicePitch,
                                  const void *ptr,
                                  cl_uint numEventsInWaitList,
                                  const cl_event *eventWaitList,
                                  cl_event *event);

    cl_int enqueueCopyBuffer(cl_mem srcBuffer,
                             cl_mem dstBuffer,
                             size_t srcOffset,
                             size_t dstOffset,
                             size_t size,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event);

    cl_int enqueueCopyBufferRect(cl_mem srcBuffer,
                                 cl_mem dstBuffer,
                                 const size_t *srcOrigin,
                                 const size_t *dstOrigin,
                                 const size_t *region,
                                 size_t srcRowPitch,
                                 size_t srcSlicePitch,
                                 size_t dstRowPitch,
                                 size_t dstSlicePitch,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event);

    cl_int enqueueFillBuffer(cl_mem buffer,
                             const void *pattern,
                             size_t patternSize,
                             size_t offset,
                             size_t size,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event);

    void *enqueueMapBuffer(cl_mem buffer,
                           cl_bool blockingMap,
                           MapFlags mapFlags,
                           size_t offset,
                           size_t size,
                           cl_uint numEventsInWaitList,
                           const cl_event *eventWaitList,
                           cl_event *event,
                           cl_int &errorCode);

    cl_int enqueueReadImage(cl_mem image,
                            cl_bool blockingRead,
                            const size_t *origin,
                            const size_t *region,
                            size_t rowPitch,
                            size_t slicePitch,
                            void *ptr,
                            cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList,
                            cl_event *event);

    cl_int enqueueWriteImage(cl_mem image,
                             cl_bool blockingWrite,
                             const size_t *origin,
                             const size_t *region,
                             size_t inputRowPitch,
                             size_t inputSlicePitch,
                             const void *ptr,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event);

    cl_int enqueueCopyImage(cl_mem srcImage,
                            cl_mem dstImage,
                            const size_t *srcOrigin,
                            const size_t *dstOrigin,
                            const size_t *region,
                            cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList,
                            cl_event *event);

    cl_int enqueueFillImage(cl_mem image,
                            const void *fillColor,
                            const size_t *origin,
                            const size_t *region,
                            cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList,
                            cl_event *event);

    cl_int enqueueCopyImageToBuffer(cl_mem srcImage,
                                    cl_mem dstBuffer,
                                    const size_t *srcOrigin,
                                    const size_t *region,
                                    size_t dstOffset,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event);

    cl_int enqueueCopyBufferToImage(cl_mem srcBuffer,
                                    cl_mem dstImage,
                                    size_t srcOffset,
                                    const size_t *dstOrigin,
                                    const size_t *region,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event);

    void *enqueueMapImage(cl_mem image,
                          cl_bool blockingMap,
                          MapFlags mapFlags,
                          const size_t *origin,
                          const size_t *region,
                          size_t *imageRowPitch,
                          size_t *imageSlicePitch,
                          cl_uint numEventsInWaitList,
                          const cl_event *eventWaitList,
                          cl_event *event,
                          cl_int &errorCode);

    cl_int enqueueUnmapMemObject(cl_mem memobj,
                                 void *mappedPtr,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event);

    cl_int enqueueMigrateMemObjects(cl_uint numMemObjects,
                                    const cl_mem *memObjects,
                                    MemMigrationFlags flags,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event);

    cl_int enqueueNDRangeKernel(cl_kernel kernel,
                                cl_uint workDim,
                                const size_t *globalWorkOffset,
                                const size_t *globalWorkSize,
                                const size_t *localWorkSize,
                                cl_uint numEventsInWaitList,
                                const cl_event *eventWaitList,
                                cl_event *event);

    cl_int enqueueTask(cl_kernel kernel,
                       cl_uint numEventsInWaitList,
                       const cl_event *eventWaitList,
                       cl_event *event);

    cl_int enqueueNativeKernel(UserFunc userFunc,
                               void *args,
                               size_t cbArgs,
                               cl_uint numMemObjects,
                               const cl_mem *memList,
                               const void **argsMemLoc,
                               cl_uint numEventsInWaitList,
                               const cl_event *eventWaitList,
                               cl_event *event);

    cl_int enqueueMarkerWithWaitList(cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event);

    cl_int enqueueMarker(cl_event *event);

    cl_int enqueueWaitForEvents(cl_uint numEvents, const cl_event *eventList);

    cl_int enqueueBarrierWithWaitList(cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event);

    cl_int enqueueBarrier();

    cl_int flush();
    cl_int finish();

  public:
    using PropArray = std::vector<cl_queue_properties>;

    static constexpr cl_uint kNoSize = std::numeric_limits<cl_uint>::max();

    ~CommandQueue() override;

    Context &getContext();
    const Context &getContext() const;
    const Device &getDevice() const;

    // Get index of device in the context.
    size_t getDeviceIndex() const;

    CommandQueueProperties getProperties() const;
    bool isOnHost() const;
    bool isOnDevice() const;

    bool hasSize() const;
    cl_uint getSize() const;

    template <typename T = rx::CLCommandQueueImpl>
    T &getImpl() const;

  private:
    CommandQueue(Context &context,
                 Device &device,
                 PropArray &&propArray,
                 CommandQueueProperties properties,
                 cl_uint size,
                 cl_int &errorCode);

    CommandQueue(Context &context,
                 Device &device,
                 CommandQueueProperties properties,
                 cl_int &errorCode);

    const ContextPtr mContext;
    const DevicePtr mDevice;
    const PropArray mPropArray;
    angle::SynchronizedValue<CommandQueueProperties> mProperties;
    const cl_uint mSize = kNoSize;
    const rx::CLCommandQueueImpl::Ptr mImpl;

    friend class Object;
};

inline Context &CommandQueue::getContext()
{
    return *mContext;
}

inline const Context &CommandQueue::getContext() const
{
    return *mContext;
}

inline const Device &CommandQueue::getDevice() const
{
    return *mDevice;
}

inline CommandQueueProperties CommandQueue::getProperties() const
{
    return *mProperties;
}

inline bool CommandQueue::isOnHost() const
{
    return mProperties->isNotSet(CL_QUEUE_ON_DEVICE);
}

inline bool CommandQueue::isOnDevice() const
{
    return mProperties->isSet(CL_QUEUE_ON_DEVICE);
}

inline bool CommandQueue::hasSize() const
{
    return mSize != kNoSize;
}

inline cl_uint CommandQueue::getSize() const
{
    return mSize;
}

template <typename T>
inline T &CommandQueue::getImpl() const
{
    return static_cast<T &>(*mImpl);
}

}  // namespace cl

#endif  // LIBANGLE_CLCOMMANDQUEUE_H_
