/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if !__has_feature(objc_arc)
#error This header requires ARC. Include it only from files compiled with -fobjc-arc.
#endif

#import "Instance.h"
#import <CoreVideo/CVPixelBuffer.h>
#import <Metal/Metal.h>
#import <WebGPU/WebGPUCpp.h>
#import <atomic>
#import <memory>
#import <wtf/CompletionHandler.h>
#import <wtf/FastMalloc.h>
#import <wtf/Function.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/MonotonicTime.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/Ref.h>
#import <wtf/SwiftBridging.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/ThreadSafeWeakPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakPtr.h>

IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")


namespace WebGPU::Metal {

class Buffer;
class CommandBuffer;
class CommandEncoder;
class Device;
class Texture;
class TextureView;

// A frame carries primaries of its own, which are usually neither of the two color spaces WebGPU
// names, so both importExternalTexture() and copyExternalImageToTexture() have to convert them.
// Row-major and applied to linear-light values; std::nullopt when the frame's primaries already are
// the destination's, which has to stay a no-op rather than a transfer function round trip.
std::optional<std::array<float, 9>> primariesConversionMatrixForPixelBuffer(CVPixelBufferRef, WebGPU::PredefinedColorSpace destination);

// The HDR transfer functions which importExternalTexture() and copyExternalImageToTexture() decode
// and tone map to SDR. Anything else is treated as SDR. The values are what the external texture
// shader finds in the fourth column of its primaries conversion matrix, so they must not change.
enum class HDRTransferFunction : uint8_t {
    None = 0,
    PQ = 1, // NOLINT
    HLG = 2, // NOLINT
};
HDRTransferFunction hdrTransferFunctionForPixelBuffer(CVPixelBufferRef);

// https://gpuweb.github.io/gpuweb/#gpuqueue
// A device owns its default queue, not the other way around.
class Queue final : public WebGPU::Queue {
    WTF_MAKE_TZONE_ALLOCATED(Queue);
public:
    static Ref<Queue> create(id<MTLCommandQueue> commandQueue, Adapter& adapter, Device& device)
    {
        return adoptRef(*new Queue(commandQueue, adapter, device));
    }
    static Ref<Queue> createInvalid(Adapter& adapter, Device& device)
    {
        return adoptRef(*new Queue(adapter, device));
    }

    ~Queue();

    void onSubmittedWorkDone(CompletionHandler<void(bool)>&&) final;
    void submit(Vector<Ref<WebGPU::Metal::CommandBuffer>>&& commands);
    void writeBuffer(Buffer&, uint64_t bufferOffset, std::span<uint8_t> data);
    void writeBuffer(id<MTLBuffer>, uint64_t bufferOffset, std::span<uint8_t> data);
    void clearBuffer(id<MTLBuffer>, NSUInteger offset = 0, NSUInteger size = NSUIntegerMax);
    void copyBuffer(id<MTLBuffer> source, uint64_t sourceOffset, id<MTLBuffer> destination, uint64_t destinationOffset, uint64_t size);
    void writeTexture(const WebGPU::TexelCopyTextureInfo& destination, std::span<uint8_t> data, const WebGPU::TexelCopyBufferLayout&, const WebGPU::Extent3D& writeSize, bool skipValidation = false);
    void copyExternalImageToTexture(const WebGPU::ImageCopyExternalImage& source, const WebGPU::ImageCopyTextureTagged& destination, const WebGPU::Extent3D& copySize) final;
    void setLabel(String&&) final;

    void submit(Vector<Ref<WebGPU::CommandBuffer>>&&) final;
    void writeBuffer(WebGPU::Buffer&, uint64_t bufferOffset, std::span<const uint8_t> data) final;
    void writeTexture(const WebGPU::TexelCopyTextureInfo& destination, std::span<const uint8_t> data, const WebGPU::TexelCopyBufferLayout&, const WebGPU::Extent3D& writeSize) final;

    void onSubmittedWorkScheduled(Function<void()>&&);

    bool isValid() const final { return m_commandQueue; }
    void makeInvalid();
    void setCommittedSignalEvent(id<MTLSharedEvent>, size_t frameIndex);

    RefPtr<WebGPU::Device> device() const final;
    void clearTextureIfNeeded(const WebGPU::TexelCopyTextureInfo&, NSUInteger);
    id<MTLCommandBuffer> _Nullable commandBufferWithDescriptor(MTLCommandBufferDescriptor*);
    void commitMTLCommandBuffer(id<MTLCommandBuffer>);
    void removeMTLCommandBuffer(id<MTLCommandBuffer>);
    void setEncoderForBuffer(id<MTLCommandBuffer>, id<MTLCommandEncoder>);
    id<MTLCommandEncoder> _Nullable encoderForBuffer(id<MTLCommandBuffer>) const;
    void clearTextureViewIfNeeded(TextureView&);
    void clearTextureViewIfNeeded(Texture&);
    NODELETE static bool writeWillCompletelyClear(WebGPU::TextureDimension, uint32_t widthForMetal, uint32_t logicalSizeWidth, uint32_t heightForMetal, uint32_t logicalSizeHeight, uint32_t depthForMetal, uint32_t logicalSizeDepthOrArrayLayers);
    void endEncoding(id<MTLCommandEncoder>, id<MTLCommandBuffer>) const;

    id<MTLBlitCommandEncoder> ensureBlitCommandEncoder();
    void finalizeBlitCommandEncoder();

    void addProvisionalTextureClear(Texture&, uint32_t mipLevel, uint32_t slice, uint64_t commandEncoderId);
    void removeProvisionalTextureClears(const Texture&, uint32_t mipLevel, uint32_t slice);
    void discardProvisionalTextureClears(uint64_t commandEncoderId);
    void resolveProvisionalTextureClears(std::optional<uint64_t> committingCommandEncoderId = std::nullopt);

    // This can be called on a background thread.
    void scheduleWork(Instance::WorkItem&&);
    [[nodiscard]] uint64_t retainCounterSampleBuffer(CommandEncoder&);
    void releaseCounterSampleBuffer(uint64_t);
    void retainTimestampsForOneUpdate(NSMutableSet<id<MTLCounterSampleBuffer>> *);
    void waitForAllCommitedWorkToComplete();
    void synchronizeResourceAndWait(id<MTLBuffer>);
    bool canDeferPass() const;
    void registerDeferredPassReplay(id<MTLCommandBuffer>, Function<void(id<MTLCommandBuffer>)>&&);
    void flushDeferredCommits();
    id<MTLIndirectCommandBuffer> trimICB(id<MTLIndirectCommandBuffer> dest, id<MTLIndirectCommandBuffer> src, NSUInteger newSize);
    id<MTLDevice> _Nullable metalDevice() const;
    std::pair<id<MTLBuffer>, uint64_t> newTemporaryBufferWithBytes(std::span<uint8_t> data, bool noCopy);
    void stageBufferWrite(id<MTLBuffer>, uint64_t bufferOffset, std::span<uint8_t> data);
    void encodeStagedCopy(id<MTLBuffer> temporaryBuffer, uint64_t temporaryBufferOffset, id<MTLBuffer>, uint64_t bufferOffset, uint64_t size, bool finalizeAfterCopy);
    uint64_t submittedCommandBufferCount() const { return m_submittedCommandBufferCount; }
    uint64_t completedCommandBufferCount() const { return m_completedCommandBufferCount; }
    bool hasUncommittedQueueWork() const { return m_commandBuffer != nil; }

private:
    Queue(id<MTLCommandQueue>, Adapter&, Device&);
    Queue(Adapter&, Device&);

    NSString * _Nullable errorValidatingSubmit(const Vector<Ref<WebGPU::Metal::CommandBuffer>>&) const;
    bool validateWriteBuffer(const Buffer&, uint64_t bufferOffset, size_t) const;


    NODELETE bool isIdle() const;
    bool isSchedulingIdle() const { return m_submittedCommandBufferCount == m_scheduledCommandBufferCount; }
    id<MTLComputeCommandEncoder> _Nullable ensureStagedCopyEncoder();
    id<MTLComputePipelineState> _Nullable stagedCopyPipelineState();
    std::pair<id<MTLBuffer>, uint64_t> reserveTemporaryBuffer(size_t);
    void removeMTLCommandBufferInternal(id<MTLCommandBuffer>);
    void scheduleWatchdogTimerLocked(Seconds) WTF_REQUIRES_LOCK(m_committedNotCompletedBuffersLock);
    void clearTextureIfNeeded(Texture&, uint32_t mipLevelCount, uint32_t arrayLayerCount, uint32_t baseMipLevel, uint32_t baseArrayLayer);

    NSString * _Nullable errorValidatingWriteTexture(const WebGPU::TexelCopyTextureInfo&, const WebGPU::TexelCopyBufferLayout&, const WebGPU::Extent3D&, size_t, const Texture&) const;
    NSString * _Nullable errorValidatingCopyExternalImageToTexture(const WebGPU::ImageCopyTextureTagged&, const WebGPU::Extent3D&, const Texture&, const Device&) const;
    // Renders one source texel per destination texel, so the pipeline only varies by the destination's
    // pixel format. Cached per format, because a copy per animation frame is the expected usage.
    id<MTLRenderPipelineState> _Nullable copyExternalImagePipelineState(MTLPixelFormat);

    struct ProvisionalTextureClear {
        ThreadSafeWeakPtr<Texture> texture;
        uint32_t mipLevel { 0 };
        uint32_t slice { 0 };
        uint64_t commandEncoderId { 0 };
    };

    id<MTLCommandQueue> _Nullable m_commandQueue { nil };
    id<MTLCommandBuffer> _Nullable m_commandBuffer { nil };
    id<MTLBlitCommandEncoder> _Nullable m_blitCommandEncoder { nil };
    id<MTLComputeCommandEncoder> _Nullable m_stagedCopyEncoder { nil };
    id<MTLComputePipelineState> _Nullable m_stagedCopyPipelineState { nil };
    bool m_stagedCopyPipelineCreationFailed { false };
    NSMutableDictionary<NSNumber *, id<MTLRenderPipelineState>> * _Nullable m_copyExternalImagePipelineStates { nil };
    bool m_copyExternalImagePipelineCreationFailed { false };
    ThreadSafeWeakPtr<Device> m_device; // The only kind of queues that exist right now are default queues, which are owned by Devices.
    uint64_t m_submittedCommandBufferCount { 0 };
    uint64_t m_completedCommandBufferCount { 0 };
    uint64_t m_scheduledCommandBufferCount { 0 };
    HashSet<uint64_t> m_completedOutOfOrderSerials;
    HashSet<uint64_t> m_scheduledOutOfOrderSerials;
    using OnSubmittedWorkScheduledCallbacks = Vector<WTF::Function<void()>>;
    HashMap<uint64_t, OnSubmittedWorkScheduledCallbacks> m_onSubmittedWorkScheduledCallbacks;
    using OnSubmittedWorkDoneCallbacks = Vector<WTF::Function<void(bool)>>;
    HashMap<uint64_t, OnSubmittedWorkDoneCallbacks> m_onSubmittedWorkDoneCallbacks;
    NSMutableDictionary<NSNumber*, NSMutableSet<id<MTLCounterSampleBuffer>>*> * _Nullable m_retainedCounterSampleBuffers;
    NSMutableOrderedSet<id<MTLCommandBuffer>> * _Nullable m_createdNotCommittedBuffers { nil };
    NSMutableOrderedSet<id<MTLCommandBuffer>> * _Nullable m_committedNotCompletedBuffers WTF_GUARDED_BY_LOCK(m_committedNotCompletedBuffersLock) { nil };
    HashMap<const void*, MonotonicTime> m_commandBufferCommitTimes WTF_GUARDED_BY_LOCK(m_committedNotCompletedBuffersLock);
    bool m_watchdogScheduled WTF_GUARDED_BY_LOCK(m_committedNotCompletedBuffersLock) { false };
    Lock m_committedNotCompletedBuffersLock;
    NSMapTable<id<MTLCommandBuffer>, id<MTLCommandEncoder>> * _Nullable m_openCommandEncoders;
    const ThreadSafeWeakPtr<Instance> m_instance;
    id<MTLBuffer> _Nullable m_temporaryBuffer;
    uint64_t m_temporaryBufferOffset;
    struct ExternalImageStagingTexture : ThreadSafeRefCounted<ExternalImageStagingTexture> {
        static Ref<ExternalImageStagingTexture> create(MTLPixelFormat pixelFormat, uint32_t width, uint32_t height, id<MTLTexture> texture)
        {
            return adoptRef(*new ExternalImageStagingTexture(pixelFormat, width, height, texture));
        }

        MTLPixelFormat pixelFormat { MTLPixelFormatInvalid };
        uint32_t width { 0 };
        uint32_t height { 0 };
        std::atomic<bool> inUse { true };
        RetainPtr<id<MTLTexture>> texture;

    private:
        ExternalImageStagingTexture(MTLPixelFormat format, uint32_t w, uint32_t h, id<MTLTexture> tex)
            : pixelFormat(format)
            , width(w)
            , height(h)
            , texture(tex)
        {
        }
    };

    Vector<ProvisionalTextureClear> m_provisionalTextureClears;
    Vector<Ref<ExternalImageStagingTexture>> m_externalImageStagingTextures;
    OSObjectPtr<dispatch_queue_t> m_deferredCommitQueue;
    std::shared_ptr<std::atomic<uint32_t>> m_inFlightDeferredCommits { std::make_shared<std::atomic<uint32_t>>(0) };
    HashMap<const void*, Function<void(id<MTLCommandBuffer>)>> m_deferredPassReplays;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refQueue, derefQueue) SWIFT_PRIVATE_FILEID("WebGPU/Queue.swift");

} // namespace WebGPU::Metal

#if !ENABLE(SWIFT_BASE_CLASS_ANNOTATIONS)
inline void refQueue(WebGPU::Metal::Queue* obj)
{
    obj->ref();
}

inline void derefQueue(WebGPU::Metal::Queue* obj)
{
    obj->deref();
}
#endif

IGNORE_CLANG_WARNINGS_END
