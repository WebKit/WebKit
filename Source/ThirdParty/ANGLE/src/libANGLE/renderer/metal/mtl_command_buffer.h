//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_command_buffer.h:
//    Defines the wrapper classes for Metal's MTLCommandEncoder, MTLCommandQueue and
//    MTLCommandBuffer.
//

#ifndef LIBANGLE_RENDERER_METAL_COMMANDENBUFFERMTL_H_
#define LIBANGLE_RENDERER_METAL_COMMANDENBUFFERMTL_H_

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include "common/FixedVector.h"
#include "common/angleutils.h"
#include "libANGLE/renderer/metal/mtl_common.h"
#include "libANGLE/renderer/metal/mtl_resources.h"
#include "libANGLE/renderer/metal/mtl_state_cache.h"

namespace rx
{
namespace mtl
{

class CommandBuffer;
class CommandEncoder;
class RenderCommandEncoder;

class CommandQueue final : public WrappedObject<id<MTLCommandQueue>>, angle::NonCopyable
{
  public:
    void reset();
    void set(id<MTLCommandQueue> metalQueue);

    void finishAllCommands();

    // This method will ensure that every GPU command buffer using this resource will finish before
    // returning. Note: this doesn't include the "in-progress" command buffer, i.e. the one hasn't
    // been commmitted yet. It's the responsibility of caller to make sure that command buffer is
    // commited/flushed first before calling this method.
    void ensureResourceReadyForCPU(const ResourceRef &resource);
    void ensureResourceReadyForCPU(Resource *resource);

    // Check whether the resource is being used by any command buffer still running on GPU.
    // This must be called before attempting to read the content of resource on CPU side.
    bool isResourceBeingUsedByGPU(const ResourceRef &resource) const
    {
        return isResourceBeingUsedByGPU(resource.get());
    }
    bool isResourceBeingUsedByGPU(const Resource *resource) const;

    CommandQueue &operator=(id<MTLCommandQueue> metalQueue)
    {
        set(metalQueue);
        return *this;
    }

    AutoObjCPtr<id<MTLCommandBuffer>> makeMetalCommandBuffer(uint64_t *queueSerialOut);

  private:
    void onCommandBufferCompleted(id<MTLCommandBuffer> buf, uint64_t serial);
    using ParentClass = WrappedObject<id<MTLCommandQueue>>;

    struct CmdBufferQueueEntry
    {
        AutoObjCPtr<id<MTLCommandBuffer>> buffer;
        uint64_t serial;
    };
    std::deque<CmdBufferQueueEntry> mMetalCmdBuffers;
    std::deque<CmdBufferQueueEntry> mMetalCmdBuffersTmp;

    uint64_t mQueueSerialCounter = 1;
    std::atomic<uint64_t> mCompletedBufferSerial{0};

    mutable std::mutex mLock;
};

class CommandBuffer final : public WrappedObject<id<MTLCommandBuffer>>, angle::NonCopyable
{
  public:
    CommandBuffer(CommandQueue *cmdQueue);
    ~CommandBuffer();

    void restart();

    bool valid() const;
    void commit();
    void finish();

    void present(id<CAMetalDrawable> presentationDrawable);

    void setWriteDependency(const ResourceRef &resource);
    void setReadDependency(const ResourceRef &resource);

    CommandQueue &cmdQueue() { return mCmdQueue; }

    void setActiveCommandEncoder(CommandEncoder *encoder);
    void invalidateActiveCommandEncoder(CommandEncoder *encoder);

  private:
    void set(id<MTLCommandBuffer> metalBuffer);
    void cleanup();

    bool validImpl() const;
    void commitImpl();

    using ParentClass = WrappedObject<id<MTLCommandBuffer>>;

    CommandQueue &mCmdQueue;

    std::atomic<CommandEncoder *> mActiveCommandEncoder{nullptr};

    uint64_t mQueueSerial = 0;

    mutable std::mutex mLock;

    bool mCommitted = false;
};

class CommandEncoder : public WrappedObject<id<MTLCommandEncoder>>, angle::NonCopyable
{
  public:
    enum Type
    {
        RENDER,
        BLIT,
        COMPUTE,
    };

    virtual ~CommandEncoder();

    virtual void endEncoding();

    void reset();
    Type getType() const { return mType; }

    CommandEncoder &markResourceBeingWrittenByGPU(const BufferRef &buffer);
    CommandEncoder &markResourceBeingWrittenByGPU(const TextureRef &texture);

  protected:
    using ParentClass = WrappedObject<id<MTLCommandEncoder>>;

    CommandEncoder(CommandBuffer *cmdBuffer, Type type);

    CommandBuffer &cmdBuffer() { return mCmdBuffer; }
    CommandQueue &cmdQueue() { return mCmdBuffer.cmdQueue(); }

    void set(id<MTLCommandEncoder> metalCmdEncoder);

  private:
    const Type mType;
    CommandBuffer &mCmdBuffer;
};

class RenderCommandEncoder final : public CommandEncoder
{
  public:
    RenderCommandEncoder(CommandBuffer *cmdBuffer);
    ~RenderCommandEncoder() override;

    void endEncoding() override;

    RenderCommandEncoder &restart(const RenderPassDesc &desc);

    RenderCommandEncoder &setRenderPipelineState(id<MTLRenderPipelineState> state);
    RenderCommandEncoder &setTriangleFillMode(MTLTriangleFillMode mode);
    RenderCommandEncoder &setFrontFacingWinding(MTLWinding winding);
    RenderCommandEncoder &setCullMode(MTLCullMode mode);

    RenderCommandEncoder &setDepthStencilState(id<MTLDepthStencilState> state);
    RenderCommandEncoder &setDepthBias(float depthBias, float slopeScale, float clamp);
    RenderCommandEncoder &setStencilRefVals(uint32_t frontRef, uint32_t backRef);
    RenderCommandEncoder &setStencilRefVal(uint32_t ref);

    RenderCommandEncoder &setViewport(const MTLViewport &viewport);
    RenderCommandEncoder &setScissorRect(const MTLScissorRect &rect);

    RenderCommandEncoder &setBlendColor(float r, float g, float b, float a);

    RenderCommandEncoder &setVertexBuffer(const BufferRef &buffer, uint32_t offset, uint32_t index);
    RenderCommandEncoder &setVertexBytes(const uint8_t *bytes, size_t size, uint32_t index);
    template <typename T>
    RenderCommandEncoder &setVertexData(const T &data, uint32_t index)
    {
        return setVertexBytes(reinterpret_cast<const uint8_t *>(&data), sizeof(T), index);
    }
    RenderCommandEncoder &setVertexSamplerState(id<MTLSamplerState> state,
                                                float lodMinClamp,
                                                float lodMaxClamp,
                                                uint32_t index);
    RenderCommandEncoder &setVertexTexture(const TextureRef &texture, uint32_t index);

    RenderCommandEncoder &setFragmentBuffer(const BufferRef &buffer,
                                            uint32_t offset,
                                            uint32_t index);
    RenderCommandEncoder &setFragmentBytes(const uint8_t *bytes, size_t size, uint32_t index);
    template <typename T>
    RenderCommandEncoder &setFragmentData(const T &data, uint32_t index)
    {
        return setFragmentBytes(reinterpret_cast<const uint8_t *>(&data), sizeof(T), index);
    }
    RenderCommandEncoder &setFragmentSamplerState(id<MTLSamplerState> state,
                                                  float lodMinClamp,
                                                  float lodMaxClamp,
                                                  uint32_t index);
    RenderCommandEncoder &setFragmentTexture(const TextureRef &texture, uint32_t index);

    RenderCommandEncoder &draw(MTLPrimitiveType primitiveType,
                               uint32_t vertexStart,
                               uint32_t vertexCount);
    RenderCommandEncoder &drawInstanced(MTLPrimitiveType primitiveType,
                                        uint32_t vertexStart,
                                        uint32_t vertexCount,
                                        uint32_t instances);
    RenderCommandEncoder &drawIndexed(MTLPrimitiveType primitiveType,
                                      uint32_t indexCount,
                                      MTLIndexType indexType,
                                      const BufferRef &indexBuffer,
                                      size_t bufferOffset);
    RenderCommandEncoder &drawIndexedInstanced(MTLPrimitiveType primitiveType,
                                               uint32_t indexCount,
                                               MTLIndexType indexType,
                                               const BufferRef &indexBuffer,
                                               size_t bufferOffset,
                                               uint32_t instances);
    RenderCommandEncoder &drawIndexedInstancedBaseVertex(MTLPrimitiveType primitiveType,
                                                         uint32_t indexCount,
                                                         MTLIndexType indexType,
                                                         const BufferRef &indexBuffer,
                                                         size_t bufferOffset,
                                                         uint32_t instances,
                                                         uint32_t baseVertex);

    RenderCommandEncoder &setColorStoreAction(MTLStoreAction action, uint32_t colorAttachmentIndex);
    // Set store action for every color attachment.
    RenderCommandEncoder &setColorStoreAction(MTLStoreAction action);

    RenderCommandEncoder &setDepthStencilStoreAction(MTLStoreAction depthStoreAction,
                                                     MTLStoreAction stencilStoreAction);
    RenderCommandEncoder &setDepthStoreAction(MTLStoreAction action);
    RenderCommandEncoder &setStencilStoreAction(MTLStoreAction action);

    const RenderPassDesc &renderPassDesc() const { return mRenderPassDesc; }

  private:
    id<MTLRenderCommandEncoder> get()
    {
        return static_cast<id<MTLRenderCommandEncoder>>(CommandEncoder::get());
    }
    inline void initWriteDependencyAndStoreAction(const TextureRef &texture,
                                                  MTLStoreAction *storeActionOut);

    RenderPassDesc mRenderPassDesc;
    MTLStoreAction mColorInitialStoreActions[kMaxRenderTargets];
    MTLStoreAction mDepthInitialStoreAction;
    MTLStoreAction mStencilInitialStoreAction;
};

class BlitCommandEncoder final : public CommandEncoder
{
  public:
    BlitCommandEncoder(CommandBuffer *cmdBuffer);
    ~BlitCommandEncoder() override;

    BlitCommandEncoder &restart();

    BlitCommandEncoder &copyBufferToTexture(const BufferRef &src,
                                            size_t srcOffset,
                                            size_t srcBytesPerRow,
                                            size_t srcBytesPerImage,
                                            MTLSize srcSize,
                                            const TextureRef &dst,
                                            uint32_t dstSlice,
                                            uint32_t dstLevel,
                                            MTLOrigin dstOrigin,
                                            MTLBlitOption blitOption);

    BlitCommandEncoder &copyTexture(const TextureRef &dst,
                                    uint32_t dstSlice,
                                    uint32_t dstLevel,
                                    MTLOrigin dstOrigin,
                                    MTLSize dstSize,
                                    const TextureRef &src,
                                    uint32_t srcSlice,
                                    uint32_t srcLevel,
                                    MTLOrigin srcOrigin);

    BlitCommandEncoder &generateMipmapsForTexture(const TextureRef &texture);
    BlitCommandEncoder &synchronizeResource(const TextureRef &texture);

  private:
    id<MTLBlitCommandEncoder> get()
    {
        return static_cast<id<MTLBlitCommandEncoder>>(CommandEncoder::get());
    }
};

class ComputeCommandEncoder final : public CommandEncoder
{
  public:
    ComputeCommandEncoder(CommandBuffer *cmdBuffer);
    ~ComputeCommandEncoder() override;

    ComputeCommandEncoder &restart();

    ComputeCommandEncoder &setComputePipelineState(id<MTLComputePipelineState> state);

    ComputeCommandEncoder &setBuffer(const BufferRef &buffer, uint32_t offset, uint32_t index);
    ComputeCommandEncoder &setBytes(const uint8_t *bytes, size_t size, uint32_t index);
    template <typename T>
    ComputeCommandEncoder &setData(const T &data, uint32_t index)
    {
        return setBytes(reinterpret_cast<const uint8_t *>(&data), sizeof(T), index);
    }
    ComputeCommandEncoder &setSamplerState(id<MTLSamplerState> state,
                                           float lodMinClamp,
                                           float lodMaxClamp,
                                           uint32_t index);
    ComputeCommandEncoder &setTexture(const TextureRef &texture, uint32_t index);

    ComputeCommandEncoder &dispatch(MTLSize threadGroupsPerGrid, MTLSize threadsPerGroup);

    ComputeCommandEncoder &dispatchNonUniform(MTLSize threadsPerGrid, MTLSize threadsPerGroup);

  private:
    id<MTLComputeCommandEncoder> get()
    {
        return static_cast<id<MTLComputeCommandEncoder>>(CommandEncoder::get());
    }
};

}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_COMMANDENBUFFERMTL_H_ */
