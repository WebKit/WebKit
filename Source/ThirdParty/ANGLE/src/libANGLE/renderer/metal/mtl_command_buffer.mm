//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_command_buffer.mm:
//      Implementations of Metal framework's MTLCommandBuffer, MTLCommandQueue,
//      MTLCommandEncoder's wrappers.
//

#include "libANGLE/renderer/metal/mtl_command_buffer.h"

#include <cassert>

#include "common/debug.h"
#include "libANGLE/renderer/metal/mtl_resources.h"

namespace rx
{
namespace mtl
{

// CommandQueue implementation
void CommandQueue::reset()
{
    finishAllCommands();
    ParentClass::reset();
}

void CommandQueue::set(id<MTLCommandQueue> metalQueue)
{
    finishAllCommands();

    ParentClass::set(metalQueue);
}

void CommandQueue::finishAllCommands()
{
    {
        // Copy to temp list
        std::lock_guard<std::mutex> lg(mLock);

        for (CmdBufferQueueEntry &metalBufferEntry : mMetalCmdBuffers)
        {
            mMetalCmdBuffersTmp.push_back(metalBufferEntry);
        }

        mMetalCmdBuffers.clear();
    }

    // Wait for command buffers to finish
    for (CmdBufferQueueEntry &metalBufferEntry : mMetalCmdBuffersTmp)
    {
        [metalBufferEntry.buffer waitUntilCompleted];
    }
    mMetalCmdBuffersTmp.clear();
}

void CommandQueue::ensureResourceReadyForCPU(const ResourceRef &resource)
{
    if (!resource)
    {
        return;
    }

    ensureResourceReadyForCPU(resource.get());
}

void CommandQueue::ensureResourceReadyForCPU(Resource *resource)
{
    mLock.lock();
    while (isResourceBeingUsedByGPU(resource) && !mMetalCmdBuffers.empty())
    {
        CmdBufferQueueEntry metalBufferEntry = mMetalCmdBuffers.front();
        mMetalCmdBuffers.pop_front();
        mLock.unlock();

        ANGLE_MTL_LOG("Waiting for MTLCommandBuffer %llu:%p", metalBufferEntry.serial,
                      metalBufferEntry.buffer.get());
        [metalBufferEntry.buffer waitUntilCompleted];

        mLock.lock();
    }
    mLock.unlock();

    // This can happen if the resource is read then write in the same command buffer.
    // So it is the responsitibily of outer code to ensure the command buffer is commit before
    // the resource can be read or written again
    ASSERT(!isResourceBeingUsedByGPU(resource));
}

bool CommandQueue::isResourceBeingUsedByGPU(const Resource *resource) const
{
    if (!resource)
    {
        return false;
    }

    return mCompletedBufferSerial.load(std::memory_order_relaxed) <
           resource->getCommandBufferQueueSerial().load(std::memory_order_relaxed);
}

AutoObjCPtr<id<MTLCommandBuffer>> CommandQueue::makeMetalCommandBuffer(uint64_t *queueSerialOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        AutoObjCPtr<id<MTLCommandBuffer>> metalCmdBuffer = [get() commandBuffer];

        std::lock_guard<std::mutex> lg(mLock);

        uint64_t serial = mQueueSerialCounter++;

        mMetalCmdBuffers.push_back({metalCmdBuffer, serial});

        ANGLE_MTL_LOG("Created MTLCommandBuffer %llu:%p", serial, metalCmdBuffer.get());

        [metalCmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> buf) {
          onCommandBufferCompleted(buf, serial);
        }];

        [metalCmdBuffer enqueue];

        ASSERT(metalCmdBuffer);

        *queueSerialOut = serial;

        return metalCmdBuffer;
    }
}

void CommandQueue::onCommandBufferCompleted(id<MTLCommandBuffer> buf, uint64_t serial)
{
    std::lock_guard<std::mutex> lg(mLock);

    ANGLE_MTL_LOG("Completed MTLCommandBuffer %llu:%p", serial, buf);

    if (mCompletedBufferSerial >= serial)
    {
        // Already handled.
        return;
    }

    while (!mMetalCmdBuffers.empty() && mMetalCmdBuffers.front().serial <= serial)
    {
        CmdBufferQueueEntry metalBufferEntry = mMetalCmdBuffers.front();
        ANGLE_UNUSED_VARIABLE(metalBufferEntry);
        ANGLE_MTL_LOG("Popped MTLCommandBuffer %llu:%p", metalBufferEntry.serial,
                      metalBufferEntry.buffer.get());

        mMetalCmdBuffers.pop_front();
    }

    mCompletedBufferSerial.store(
        std::max(mCompletedBufferSerial.load(std::memory_order_relaxed), serial),
        std::memory_order_relaxed);
}

// CommandBuffer implementation
CommandBuffer::CommandBuffer(CommandQueue *cmdQueue) : mCmdQueue(*cmdQueue) {}

CommandBuffer::~CommandBuffer()
{
    finish();
    cleanup();
}

bool CommandBuffer::valid() const
{
    std::lock_guard<std::mutex> lg(mLock);

    return validImpl();
}

void CommandBuffer::commit()
{
    std::lock_guard<std::mutex> lg(mLock);
    commitImpl();
}

void CommandBuffer::finish()
{
    commit();
    [get() waitUntilCompleted];
}

void CommandBuffer::present(id<CAMetalDrawable> presentationDrawable)
{
    [get() presentDrawable:presentationDrawable];
}

void CommandBuffer::setWriteDependency(const ResourceRef &resource)
{
    if (!resource)
    {
        return;
    }

    std::lock_guard<std::mutex> lg(mLock);

    if (!validImpl())
    {
        return;
    }

    resource->setUsedByCommandBufferWithQueueSerial(mQueueSerial, true);
}

void CommandBuffer::setReadDependency(const ResourceRef &resource)
{
    if (!resource)
    {
        return;
    }

    std::lock_guard<std::mutex> lg(mLock);

    if (!validImpl())
    {
        return;
    }

    resource->setUsedByCommandBufferWithQueueSerial(mQueueSerial, false);
}

void CommandBuffer::restart()
{
    uint64_t serial     = 0;
    auto metalCmdBuffer = mCmdQueue.makeMetalCommandBuffer(&serial);

    std::lock_guard<std::mutex> lg(mLock);

    set(metalCmdBuffer);
    mQueueSerial = serial;
    mCommitted   = false;

    ASSERT(metalCmdBuffer);
}

/** private use only */
void CommandBuffer::set(id<MTLCommandBuffer> metalBuffer)
{
    ParentClass::set(metalBuffer);
}

void CommandBuffer::setActiveCommandEncoder(CommandEncoder *encoder)
{
    mActiveCommandEncoder = encoder;
}

void CommandBuffer::invalidateActiveCommandEncoder(CommandEncoder *encoder)
{
    mActiveCommandEncoder.compare_exchange_strong(encoder, nullptr);
}

void CommandBuffer::cleanup()
{
    mActiveCommandEncoder = nullptr;

    ParentClass::set(nil);
}

bool CommandBuffer::validImpl() const
{
    if (!ParentClass::valid())
    {
        return false;
    }

    return !mCommitted;
}

void CommandBuffer::commitImpl()
{
    if (!validImpl())
    {
        return;
    }

    // End the current encoder
    if (mActiveCommandEncoder.load(std::memory_order_relaxed))
    {
        mActiveCommandEncoder.load(std::memory_order_relaxed)->endEncoding();
        mActiveCommandEncoder = nullptr;
    }

    // Do the actual commit
    [get() commit];

    ANGLE_MTL_LOG("Committed MTLCommandBuffer %llu:%p", mQueueSerial, get());

    mCommitted = true;
}

// CommandEncoder implementation
CommandEncoder::CommandEncoder(CommandBuffer *cmdBuffer, Type type)
    : mType(type), mCmdBuffer(*cmdBuffer)
{}

CommandEncoder::~CommandEncoder()
{
    reset();
}

void CommandEncoder::endEncoding()
{
    [get() endEncoding];
    reset();
}

void CommandEncoder::reset()
{
    ParentClass::reset();

    mCmdBuffer.invalidateActiveCommandEncoder(this);
}

void CommandEncoder::set(id<MTLCommandEncoder> metalCmdEncoder)
{
    ParentClass::set(metalCmdEncoder);

    // Set this as active encoder
    cmdBuffer().setActiveCommandEncoder(this);
}

CommandEncoder &CommandEncoder::markResourceBeingWrittenByGPU(const BufferRef &buffer)
{
    cmdBuffer().setWriteDependency(buffer);
    return *this;
}

CommandEncoder &CommandEncoder::markResourceBeingWrittenByGPU(const TextureRef &texture)
{
    cmdBuffer().setWriteDependency(texture);
    return *this;
}

// RenderCommandEncoder implemtation
RenderCommandEncoder::RenderCommandEncoder(CommandBuffer *cmdBuffer)
    : CommandEncoder(cmdBuffer, RENDER)
{}
RenderCommandEncoder::~RenderCommandEncoder() {}

void RenderCommandEncoder::endEncoding()
{
    if (!valid())
        return;

    // Now is the time to do the actual store option setting.
    auto metalEncoder = get();
    for (uint32_t i = 0; i < mRenderPassDesc.numColorAttachments; ++i)
    {
        if (mRenderPassDesc.colorAttachments[i].storeAction == MTLStoreActionUnknown)
        {
            // If storeAction hasn't been set for this attachment, we set to dontcare.
            mRenderPassDesc.colorAttachments[i].storeAction = MTLStoreActionDontCare;
        }

        // Only initial unknown store action can change the value now.
        if (mColorInitialStoreActions[i] == MTLStoreActionUnknown)
        {
            [metalEncoder setColorStoreAction:mRenderPassDesc.colorAttachments[i].storeAction
                                      atIndex:i];
        }
    }

    if (mRenderPassDesc.depthAttachment.storeAction == MTLStoreActionUnknown)
    {
        // If storeAction hasn't been set for this attachment, we set to dontcare.
        mRenderPassDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
    }
    if (mDepthInitialStoreAction == MTLStoreActionUnknown)
    {
        [metalEncoder setDepthStoreAction:mRenderPassDesc.depthAttachment.storeAction];
    }

    if (mRenderPassDesc.stencilAttachment.storeAction == MTLStoreActionUnknown)
    {
        // If storeAction hasn't been set for this attachment, we set to dontcare.
        mRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
    }
    if (mStencilInitialStoreAction == MTLStoreActionUnknown)
    {
        [metalEncoder setStencilStoreAction:mRenderPassDesc.stencilAttachment.storeAction];
    }

    CommandEncoder::endEncoding();

    // reset state
    mRenderPassDesc = RenderPassDesc();
}

inline void RenderCommandEncoder::initWriteDependencyAndStoreAction(const TextureRef &texture,
                                                                    MTLStoreAction *storeActionOut)
{
    if (texture)
    {
        cmdBuffer().setWriteDependency(texture);
        // Set initial store action to unknown so that we can change it later when the encoder ends.
        *storeActionOut = MTLStoreActionUnknown;
    }
    else
    {
        // Texture is invalid, use don'tcare store action
        *storeActionOut = MTLStoreActionDontCare;
    }
}

RenderCommandEncoder &RenderCommandEncoder::restart(const RenderPassDesc &desc)
{
    if (valid())
    {
        if (mRenderPassDesc == desc)
        {
            // no change, skip
            return *this;
        }

        // finish current encoder
        endEncoding();
    }

    if (!cmdBuffer().valid())
    {
        reset();
        return *this;
    }

    mRenderPassDesc = desc;

    ANGLE_MTL_OBJC_SCOPE
    {
        // mask writing dependency
        for (uint32_t i = 0; i < mRenderPassDesc.numColorAttachments; ++i)
        {
            initWriteDependencyAndStoreAction(mRenderPassDesc.colorAttachments[i].texture,
                                              &mRenderPassDesc.colorAttachments[i].storeAction);
            mColorInitialStoreActions[i] = mRenderPassDesc.colorAttachments[i].storeAction;
        }

        initWriteDependencyAndStoreAction(mRenderPassDesc.depthAttachment.texture,
                                          &mRenderPassDesc.depthAttachment.storeAction);
        mDepthInitialStoreAction = mRenderPassDesc.depthAttachment.storeAction;

        initWriteDependencyAndStoreAction(mRenderPassDesc.stencilAttachment.texture,
                                          &mRenderPassDesc.stencilAttachment.storeAction);
        mStencilInitialStoreAction = mRenderPassDesc.stencilAttachment.storeAction;

        // Create objective C object
        mtl::AutoObjCObj<MTLRenderPassDescriptor> objCDesc = ToMetalObj(mRenderPassDesc);

        ANGLE_MTL_LOG("Creating new render command encoder with desc: %@", objCDesc.get());

        id<MTLRenderCommandEncoder> metalCmdEncoder =
            [cmdBuffer().get() renderCommandEncoderWithDescriptor:objCDesc];

        set(metalCmdEncoder);

        // Set the actual store action
        for (uint32_t i = 0; i < desc.numColorAttachments; ++i)
        {
            setColorStoreAction(desc.colorAttachments[i].storeAction, i);
        }

        setDepthStencilStoreAction(desc.depthAttachment.storeAction,
                                   desc.stencilAttachment.storeAction);

        // Verify that it was created successfully
        ASSERT(get());
    }  // ANGLE_MTL_OBJC_SCOPE

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setRenderPipelineState(id<MTLRenderPipelineState> state)
{
    [get() setRenderPipelineState:state];

    return *this;
}
RenderCommandEncoder &RenderCommandEncoder::setTriangleFillMode(MTLTriangleFillMode mode)
{
    [get() setTriangleFillMode:mode];

    return *this;
}
RenderCommandEncoder &RenderCommandEncoder::setFrontFacingWinding(MTLWinding winding)
{
    [get() setFrontFacingWinding:winding];

    return *this;
}
RenderCommandEncoder &RenderCommandEncoder::setCullMode(MTLCullMode mode)
{
    [get() setCullMode:mode];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setDepthStencilState(id<MTLDepthStencilState> state)
{
    [get() setDepthStencilState:state];

    return *this;
}
RenderCommandEncoder &RenderCommandEncoder::setDepthBias(float depthBias,
                                                         float slopeScale,
                                                         float clamp)
{
    [get() setDepthBias:depthBias slopeScale:slopeScale clamp:clamp];

    return *this;
}
RenderCommandEncoder &RenderCommandEncoder::setStencilRefVals(uint32_t frontRef, uint32_t backRef)
{
    // Metal has some bugs when reference values are larger than 0xff
    ASSERT(frontRef == (frontRef & kStencilMaskAll));
    ASSERT(backRef == (backRef & kStencilMaskAll));
    [get() setStencilFrontReferenceValue:frontRef backReferenceValue:backRef];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setStencilRefVal(uint32_t ref)
{
    return setStencilRefVals(ref, ref);
}

RenderCommandEncoder &RenderCommandEncoder::setViewport(const MTLViewport &viewport)
{
    [get() setViewport:viewport];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setScissorRect(const MTLScissorRect &rect)
{
    [get() setScissorRect:rect];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setBlendColor(float r, float g, float b, float a)
{
    [get() setBlendColorRed:r green:g blue:b alpha:a];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setVertexBuffer(const BufferRef &buffer,
                                                            uint32_t offset,
                                                            uint32_t index)
{
    if (index >= kMaxShaderBuffers)
    {
        return *this;
    }

    cmdBuffer().setReadDependency(buffer);

    [get() setVertexBuffer:(buffer ? buffer->get() : nil) offset:offset atIndex:index];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setVertexBytes(const uint8_t *bytes,
                                                           size_t size,
                                                           uint32_t index)
{
    if (index >= kMaxShaderBuffers)
    {
        return *this;
    }

    [get() setVertexBytes:bytes length:size atIndex:index];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setVertexSamplerState(id<MTLSamplerState> state,
                                                                  float lodMinClamp,
                                                                  float lodMaxClamp,
                                                                  uint32_t index)
{
    if (index >= kMaxShaderSamplers)
    {
        return *this;
    }

    [get() setVertexSamplerState:state
                     lodMinClamp:lodMinClamp
                     lodMaxClamp:lodMaxClamp
                         atIndex:index];

    return *this;
}
RenderCommandEncoder &RenderCommandEncoder::setVertexTexture(const TextureRef &texture,
                                                             uint32_t index)
{
    if (index >= kMaxShaderSamplers)
    {
        return *this;
    }

    cmdBuffer().setReadDependency(texture);
    [get() setVertexTexture:(texture ? texture->get() : nil) atIndex:index];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setFragmentBuffer(const BufferRef &buffer,
                                                              uint32_t offset,
                                                              uint32_t index)
{
    if (index >= kMaxShaderBuffers)
    {
        return *this;
    }

    cmdBuffer().setReadDependency(buffer);

    [get() setFragmentBuffer:(buffer ? buffer->get() : nil) offset:offset atIndex:index];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setFragmentBytes(const uint8_t *bytes,
                                                             size_t size,
                                                             uint32_t index)
{
    if (index >= kMaxShaderBuffers)
    {
        return *this;
    }

    [get() setFragmentBytes:bytes length:size atIndex:index];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setFragmentSamplerState(id<MTLSamplerState> state,
                                                                    float lodMinClamp,
                                                                    float lodMaxClamp,
                                                                    uint32_t index)
{
    if (index >= kMaxShaderSamplers)
    {
        return *this;
    }

    [get() setFragmentSamplerState:state
                       lodMinClamp:lodMinClamp
                       lodMaxClamp:lodMaxClamp
                           atIndex:index];

    return *this;
}
RenderCommandEncoder &RenderCommandEncoder::setFragmentTexture(const TextureRef &texture,
                                                               uint32_t index)
{
    if (index >= kMaxShaderSamplers)
    {
        return *this;
    }

    cmdBuffer().setReadDependency(texture);
    [get() setFragmentTexture:(texture ? texture->get() : nil) atIndex:index];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::draw(MTLPrimitiveType primitiveType,
                                                 uint32_t vertexStart,
                                                 uint32_t vertexCount)
{
    [get() drawPrimitives:primitiveType vertexStart:vertexStart vertexCount:vertexCount];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::drawInstanced(MTLPrimitiveType primitiveType,
                                                          uint32_t vertexStart,
                                                          uint32_t vertexCount,
                                                          uint32_t instances)
{
    [get() drawPrimitives:primitiveType
              vertexStart:vertexStart
              vertexCount:vertexCount
            instanceCount:instances];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::drawIndexed(MTLPrimitiveType primitiveType,
                                                        uint32_t indexCount,
                                                        MTLIndexType indexType,
                                                        const BufferRef &indexBuffer,
                                                        size_t bufferOffset)
{
    if (!indexBuffer)
    {
        return *this;
    }

    cmdBuffer().setReadDependency(indexBuffer);
    [get() drawIndexedPrimitives:primitiveType
                      indexCount:indexCount
                       indexType:indexType
                     indexBuffer:indexBuffer->get()
               indexBufferOffset:bufferOffset];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::drawIndexedInstanced(MTLPrimitiveType primitiveType,
                                                                 uint32_t indexCount,
                                                                 MTLIndexType indexType,
                                                                 const BufferRef &indexBuffer,
                                                                 size_t bufferOffset,
                                                                 uint32_t instances)
{
    if (!indexBuffer)
    {
        return *this;
    }

    cmdBuffer().setReadDependency(indexBuffer);
    [get() drawIndexedPrimitives:primitiveType
                      indexCount:indexCount
                       indexType:indexType
                     indexBuffer:indexBuffer->get()
               indexBufferOffset:bufferOffset
                   instanceCount:instances];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::drawIndexedInstancedBaseVertex(
    MTLPrimitiveType primitiveType,
    uint32_t indexCount,
    MTLIndexType indexType,
    const BufferRef &indexBuffer,
    size_t bufferOffset,
    uint32_t instances,
    uint32_t baseVertex)
{
    if (!indexBuffer)
    {
        return *this;
    }

    cmdBuffer().setReadDependency(indexBuffer);
    [get() drawIndexedPrimitives:primitiveType
                      indexCount:indexCount
                       indexType:indexType
                     indexBuffer:indexBuffer->get()
               indexBufferOffset:bufferOffset
                   instanceCount:instances
                      baseVertex:baseVertex
                    baseInstance:0];

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setColorStoreAction(MTLStoreAction action,
                                                                uint32_t colorAttachmentIndex)
{
    if (colorAttachmentIndex >= mRenderPassDesc.numColorAttachments)
    {
        return *this;
    }

    // We only store the options, will defer the actual setting until the encoder finishes
    mRenderPassDesc.colorAttachments[colorAttachmentIndex].storeAction = action;

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setColorStoreAction(MTLStoreAction action)
{
    for (uint32_t i = 0; i < mRenderPassDesc.numColorAttachments; ++i)
    {
        setColorStoreAction(action, i);
    }
    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setDepthStencilStoreAction(
    MTLStoreAction depthStoreAction,
    MTLStoreAction stencilStoreAction)
{
    // We only store the options, will defer the actual setting until the encoder finishes
    mRenderPassDesc.depthAttachment.storeAction   = depthStoreAction;
    mRenderPassDesc.stencilAttachment.storeAction = stencilStoreAction;

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setDepthStoreAction(MTLStoreAction action)
{
    // We only store the options, will defer the actual setting until the encoder finishes
    mRenderPassDesc.depthAttachment.storeAction = action;

    return *this;
}

RenderCommandEncoder &RenderCommandEncoder::setStencilStoreAction(MTLStoreAction action)
{
    // We only store the options, will defer the actual setting until the encoder finishes
    mRenderPassDesc.stencilAttachment.storeAction = action;

    return *this;
}

// BlitCommandEncoder
BlitCommandEncoder::BlitCommandEncoder(CommandBuffer *cmdBuffer) : CommandEncoder(cmdBuffer, BLIT)
{}

BlitCommandEncoder::~BlitCommandEncoder() {}

BlitCommandEncoder &BlitCommandEncoder::restart()
{
    ANGLE_MTL_OBJC_SCOPE
    {
        if (valid())
        {
            // no change, skip
            return *this;
        }

        if (!cmdBuffer().valid())
        {
            reset();
            return *this;
        }

        // Create objective C object
        set([cmdBuffer().get() blitCommandEncoder]);

        // Verify that it was created successfully
        ASSERT(get());

        return *this;
    }
}

BlitCommandEncoder &BlitCommandEncoder::copyBufferToTexture(const BufferRef &src,
                                                            size_t srcOffset,
                                                            size_t srcBytesPerRow,
                                                            size_t srcBytesPerImage,
                                                            MTLSize srcSize,
                                                            const TextureRef &dst,
                                                            uint32_t dstSlice,
                                                            uint32_t dstLevel,
                                                            MTLOrigin dstOrigin,
                                                            MTLBlitOption blitOption)
{
    if (!src || !dst)
    {
        return *this;
    }

    cmdBuffer().setReadDependency(src);
    cmdBuffer().setWriteDependency(dst);

    [get() copyFromBuffer:src->get()
               sourceOffset:srcOffset
          sourceBytesPerRow:srcBytesPerRow
        sourceBytesPerImage:srcBytesPerImage
                 sourceSize:srcSize
                  toTexture:dst->get()
           destinationSlice:dstSlice
           destinationLevel:dstLevel
          destinationOrigin:dstOrigin
                    options:blitOption];

    return *this;
}

BlitCommandEncoder &BlitCommandEncoder::copyTexture(const TextureRef &dst,
                                                    uint32_t dstSlice,
                                                    uint32_t dstLevel,
                                                    MTLOrigin dstOrigin,
                                                    MTLSize dstSize,
                                                    const TextureRef &src,
                                                    uint32_t srcSlice,
                                                    uint32_t srcLevel,
                                                    MTLOrigin srcOrigin)
{
    if (!src || !dst)
    {
        return *this;
    }

    cmdBuffer().setReadDependency(src);
    cmdBuffer().setWriteDependency(dst);
    [get() copyFromTexture:src->get()
               sourceSlice:srcSlice
               sourceLevel:srcLevel
              sourceOrigin:srcOrigin
                sourceSize:dstSize
                 toTexture:dst->get()
          destinationSlice:dstSlice
          destinationLevel:dstLevel
         destinationOrigin:dstOrigin];

    return *this;
}

BlitCommandEncoder &BlitCommandEncoder::generateMipmapsForTexture(const TextureRef &texture)
{
    if (!texture)
    {
        return *this;
    }

    cmdBuffer().setWriteDependency(texture);
    [get() generateMipmapsForTexture:texture->get()];

    return *this;
}
BlitCommandEncoder &BlitCommandEncoder::synchronizeResource(const TextureRef &texture)
{
    if (!texture)
    {
        return *this;
    }

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    // Only MacOS has separated storage for resource on CPU and GPU and needs explicit
    // synchronization
    cmdBuffer().setWriteDependency(texture);
    [get() synchronizeResource:texture->get()];
#endif
    return *this;
}

// ComputeCommandEncoder implementation
ComputeCommandEncoder::ComputeCommandEncoder(CommandBuffer *cmdBuffer)
    : CommandEncoder(cmdBuffer, COMPUTE)
{}
ComputeCommandEncoder::~ComputeCommandEncoder() {}

ComputeCommandEncoder &ComputeCommandEncoder::restart()
{
    ANGLE_MTL_OBJC_SCOPE
    {
        if (valid())
        {
            // no change, skip
            return *this;
        }

        if (!cmdBuffer().valid())
        {
            reset();
            return *this;
        }

        // Create objective C object
        set([cmdBuffer().get() computeCommandEncoder]);

        // Verify that it was created successfully
        ASSERT(get());

        return *this;
    }
}

ComputeCommandEncoder &ComputeCommandEncoder::setComputePipelineState(
    id<MTLComputePipelineState> state)
{
    [get() setComputePipelineState:state];
    return *this;
}

ComputeCommandEncoder &ComputeCommandEncoder::setBuffer(const BufferRef &buffer,
                                                        uint32_t offset,
                                                        uint32_t index)
{
    if (index >= kMaxShaderBuffers)
    {
        return *this;
    }

    // NOTE(hqle): Assume compute shader both reads and writes to this buffer for now.
    cmdBuffer().setReadDependency(buffer);
    cmdBuffer().setWriteDependency(buffer);

    [get() setBuffer:(buffer ? buffer->get() : nil) offset:offset atIndex:index];

    return *this;
}

ComputeCommandEncoder &ComputeCommandEncoder::setBytes(const uint8_t *bytes,
                                                       size_t size,
                                                       uint32_t index)
{
    if (index >= kMaxShaderBuffers)
    {
        return *this;
    }

    [get() setBytes:bytes length:size atIndex:index];

    return *this;
}

ComputeCommandEncoder &ComputeCommandEncoder::setSamplerState(id<MTLSamplerState> state,
                                                              float lodMinClamp,
                                                              float lodMaxClamp,
                                                              uint32_t index)
{
    if (index >= kMaxShaderSamplers)
    {
        return *this;
    }

    [get() setSamplerState:state lodMinClamp:lodMinClamp lodMaxClamp:lodMaxClamp atIndex:index];

    return *this;
}
ComputeCommandEncoder &ComputeCommandEncoder::setTexture(const TextureRef &texture, uint32_t index)
{
    if (index >= kMaxShaderSamplers)
    {
        return *this;
    }

    // NOTE(hqle): Assume compute shader both reads and writes to this texture for now.
    cmdBuffer().setReadDependency(texture);
    cmdBuffer().setWriteDependency(texture);
    [get() setTexture:(texture ? texture->get() : nil) atIndex:index];

    return *this;
}

ComputeCommandEncoder &ComputeCommandEncoder::dispatch(MTLSize threadGroupsPerGrid,
                                                       MTLSize threadsPerGroup)
{
    [get() dispatchThreadgroups:threadGroupsPerGrid threadsPerThreadgroup:threadsPerGroup];
    return *this;
}

ComputeCommandEncoder &ComputeCommandEncoder::dispatchNonUniform(MTLSize threadsPerGrid,
                                                                 MTLSize threadsPerGroup)
{
    [get() dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerGroup];
    return *this;
}

}
}
