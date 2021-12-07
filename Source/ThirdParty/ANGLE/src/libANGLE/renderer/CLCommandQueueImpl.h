//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLCommandQueueImpl.h: Defines the abstract rx::CLCommandQueueImpl class.

#ifndef LIBANGLE_RENDERER_CLCOMMANDQUEUEIMPL_H_
#define LIBANGLE_RENDERER_CLCOMMANDQUEUEIMPL_H_

#include "libANGLE/renderer/CLEventImpl.h"

namespace rx
{

class CLCommandQueueImpl : angle::NonCopyable
{
  public:
    using Ptr = std::unique_ptr<CLCommandQueueImpl>;

    CLCommandQueueImpl(const cl::CommandQueue &commandQueue);
    virtual ~CLCommandQueueImpl();

    virtual cl_int setProperty(cl::CommandQueueProperties properties, cl_bool enable) = 0;

    virtual cl_int enqueueReadBuffer(const cl::Buffer &buffer,
                                     bool blocking,
                                     size_t offset,
                                     size_t size,
                                     void *ptr,
                                     const cl::EventPtrs &waitEvents,
                                     CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueWriteBuffer(const cl::Buffer &buffer,
                                      bool blocking,
                                      size_t offset,
                                      size_t size,
                                      const void *ptr,
                                      const cl::EventPtrs &waitEvents,
                                      CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueReadBufferRect(const cl::Buffer &buffer,
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
                                         CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueWriteBufferRect(const cl::Buffer &buffer,
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
                                          CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueCopyBuffer(const cl::Buffer &srcBuffer,
                                     const cl::Buffer &dstBuffer,
                                     size_t srcOffset,
                                     size_t dstOffset,
                                     size_t size,
                                     const cl::EventPtrs &waitEvents,
                                     CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueCopyBufferRect(const cl::Buffer &srcBuffer,
                                         const cl::Buffer &dstBuffer,
                                         const size_t srcOrigin[3],
                                         const size_t dstOrigin[3],
                                         const size_t region[3],
                                         size_t srcRowPitch,
                                         size_t srcSlicePitch,
                                         size_t dstRowPitch,
                                         size_t dstSlicePitch,
                                         const cl::EventPtrs &waitEvents,
                                         CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueFillBuffer(const cl::Buffer &buffer,
                                     const void *pattern,
                                     size_t patternSize,
                                     size_t offset,
                                     size_t size,
                                     const cl::EventPtrs &waitEvents,
                                     CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual void *enqueueMapBuffer(const cl::Buffer &buffer,
                                   bool blocking,
                                   cl::MapFlags mapFlags,
                                   size_t offset,
                                   size_t size,
                                   const cl::EventPtrs &waitEvents,
                                   CLEventImpl::CreateFunc *eventCreateFunc,
                                   cl_int &errorCode) = 0;

    virtual cl_int enqueueReadImage(const cl::Image &image,
                                    bool blocking,
                                    const size_t origin[3],
                                    const size_t region[3],
                                    size_t rowPitch,
                                    size_t slicePitch,
                                    void *ptr,
                                    const cl::EventPtrs &waitEvents,
                                    CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueWriteImage(const cl::Image &image,
                                     bool blocking,
                                     const size_t origin[3],
                                     const size_t region[3],
                                     size_t inputRowPitch,
                                     size_t inputSlicePitch,
                                     const void *ptr,
                                     const cl::EventPtrs &waitEvents,
                                     CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueCopyImage(const cl::Image &srcImage,
                                    const cl::Image &dstImage,
                                    const size_t srcOrigin[3],
                                    const size_t dstOrigin[3],
                                    const size_t region[3],
                                    const cl::EventPtrs &waitEvents,
                                    CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueFillImage(const cl::Image &image,
                                    const void *fillColor,
                                    const size_t origin[3],
                                    const size_t region[3],
                                    const cl::EventPtrs &waitEvents,
                                    CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueCopyImageToBuffer(const cl::Image &srcImage,
                                            const cl::Buffer &dstBuffer,
                                            const size_t srcOrigin[3],
                                            const size_t region[3],
                                            size_t dstOffset,
                                            const cl::EventPtrs &waitEvents,
                                            CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueCopyBufferToImage(const cl::Buffer &srcBuffer,
                                            const cl::Image &dstImage,
                                            size_t srcOffset,
                                            const size_t dstOrigin[3],
                                            const size_t region[3],
                                            const cl::EventPtrs &waitEvents,
                                            CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual void *enqueueMapImage(const cl::Image &image,
                                  bool blocking,
                                  cl::MapFlags mapFlags,
                                  const size_t origin[3],
                                  const size_t region[3],
                                  size_t *imageRowPitch,
                                  size_t *imageSlicePitch,
                                  const cl::EventPtrs &waitEvents,
                                  CLEventImpl::CreateFunc *eventCreateFunc,
                                  cl_int &errorCode) = 0;

    virtual cl_int enqueueUnmapMemObject(const cl::Memory &memory,
                                         void *mappedPtr,
                                         const cl::EventPtrs &waitEvents,
                                         CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueMigrateMemObjects(const cl::MemoryPtrs &memObjects,
                                            cl::MemMigrationFlags flags,
                                            const cl::EventPtrs &waitEvents,
                                            CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueNDRangeKernel(const cl::Kernel &kernel,
                                        cl_uint workDim,
                                        const size_t *globalWorkOffset,
                                        const size_t *globalWorkSize,
                                        const size_t *localWorkSize,
                                        const cl::EventPtrs &waitEvents,
                                        CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueTask(const cl::Kernel &kernel,
                               const cl::EventPtrs &waitEvents,
                               CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueNativeKernel(cl::UserFunc userFunc,
                                       void *args,
                                       size_t cbArgs,
                                       const cl::BufferPtrs &buffers,
                                       const std::vector<size_t> bufferPtrOffsets,
                                       const cl::EventPtrs &waitEvents,
                                       CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueMarkerWithWaitList(const cl::EventPtrs &waitEvents,
                                             CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueMarker(CLEventImpl::CreateFunc &eventCreateFunc) = 0;

    virtual cl_int enqueueWaitForEvents(const cl::EventPtrs &events) = 0;

    virtual cl_int enqueueBarrierWithWaitList(const cl::EventPtrs &waitEvents,
                                              CLEventImpl::CreateFunc *eventCreateFunc) = 0;

    virtual cl_int enqueueBarrier() = 0;

    virtual cl_int flush()  = 0;
    virtual cl_int finish() = 0;

  protected:
    const cl::CommandQueue &mCommandQueue;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_CLCOMMANDQUEUEIMPL_H_
