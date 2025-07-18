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

#import "BindableResource.h"
#import "CommandBuffer.h"
#import "CommandsMixin.h"
#import "WebGPU.h"
#import "WebGPUExt.h"
#import <wtf/FastMalloc.h>
#import <wtf/Function.h>
#import <wtf/Ref.h>
#import <wtf/RefCountedAndCanMakeWeakPtr.h>
#import <wtf/RetainReleaseSwift.h>
#import <wtf/SwiftCXXThunk.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/Vector.h>
#import <wtf/WeakPtr.h>

@interface TextureAndClearColor : NSObject
- (instancetype)initWithTexture:(id<MTLTexture>)texture NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@property (nonatomic) id<MTLTexture> texture;
@property (nonatomic) MTLClearColor clearColor;
@property (nonatomic) NSUInteger depthPlane;
@end

struct WGPUCommandEncoderImpl {
};

namespace WebGPU {

class BindGroup;
class Buffer;
class CommandBuffer;
class ComputePassEncoder;
class Device;
class QuerySet;
class RenderPassEncoder;
class Sampler;
class Texture;

// https://gpuweb.github.io/gpuweb/#gpucommandencoder
class CommandEncoder : public CommandsMixin, public RefCountedAndCanMakeWeakPtr<CommandEncoder>, public WGPUCommandEncoderImpl {
    WTF_MAKE_TZONE_ALLOCATED(CommandEncoder);
public:
    static Ref<CommandEncoder> create(id<MTLCommandBuffer> commandBuffer, Device& device, uint64_t uniqueId)
    {
        return adoptRef(*new CommandEncoder(commandBuffer, device, uniqueId));
    }
    static Ref<CommandEncoder> createInvalid(Device& device)
    {
        return adoptRef(*new CommandEncoder(device));
    }
#if ENABLE(WEBGPU_SWIFT)
    inline Ref<CommandBuffer> createCommandBuffer(id<MTLCommandBuffer> commandBuffer, Device& device, id<MTLSharedEvent> sharedEvent, uint64_t sharedEventSignalValue)
    {
        return CommandBuffer::create(commandBuffer, device, sharedEvent, sharedEventSignalValue, WTFMove(m_onCommitHandlers), *this);
    }
#endif

    ~CommandEncoder();

    Ref<ComputePassEncoder> beginComputePass(const WGPUComputePassDescriptor&) HAS_SWIFTCXX_THUNK;
    Ref<RenderPassEncoder> beginRenderPass(const WGPURenderPassDescriptor&) HAS_SWIFTCXX_THUNK;
    void copyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, Buffer& destination, uint64_t destinationOffset, uint64_t size) HAS_SWIFTCXX_THUNK;
    void copyBufferToTexture(const WGPUImageCopyBuffer& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize) HAS_SWIFTCXX_THUNK;
    void copyTextureToBuffer(const WGPUImageCopyTexture& source, const WGPUImageCopyBuffer& destination, const WGPUExtent3D& copySize) HAS_SWIFTCXX_THUNK;
    void copyTextureToTexture(const WGPUImageCopyTexture& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize) HAS_SWIFTCXX_THUNK;
    void runClearEncoder(NSMutableDictionary<NSNumber*, TextureAndClearColor*> *attachmentsToClear, id<MTLTexture> depthStencilAttachmentToClear, bool depthAttachmentToClear, bool stencilAttachmentToClear, float depthClearValue = 0, uint32_t stencilClearValue = 0, id<MTLRenderCommandEncoder> existingEncoder = nil) HAS_SWIFTCXX_THUNK;
    void clearBuffer(Buffer&, uint64_t offset, uint64_t size);
    Ref<CommandBuffer> finish(const WGPUCommandBufferDescriptor&) HAS_SWIFTCXX_THUNK;
    void insertDebugMarker(String&& markerLabel);
    void popDebugGroup();
    void pushDebugGroup(String&& groupLabel);
    void resolveQuerySet(const QuerySet&, uint32_t firstQuery, uint32_t queryCount, Buffer& destination, uint64_t destinationOffset);
    void writeTimestamp(QuerySet&, uint32_t queryIndex);
    void setLabel(String&&);

    Device& device() const { return m_device; }
    Ref<Device> protectedDevice() const SWIFT_RETURNS_INDEPENDENT_VALUE { return m_device; }

    bool isValid() const { return m_commandBuffer; }
    void lock(bool);
    bool isLocked() const { return m_state == EncoderState::Locked; }

    bool isFinished() const { return m_state == EncoderState::Ended; }

#if ENABLE(WEBGPU_SWIFT)
    void setEncoderState(EncoderState state) { m_state = state; }
    EncoderState getEncoderState() { return m_state; }
    NSString * encoderStateNameWrapper() { return encoderStateName(); }
#endif

    id<MTLBlitCommandEncoder> ensureBlitCommandEncoder();
    void finalizeBlitCommandEncoder();
    static void clearTextureIfNeeded(const WGPUImageCopyTexture&, NSUInteger, const Device&, id<MTLBlitCommandEncoder>);
    static void clearTextureIfNeeded(Texture&, NSUInteger, NSUInteger, const Device&, id<MTLBlitCommandEncoder>);
    void clearTextureIfNeeded(const WGPUImageCopyTexture&, NSUInteger) HAS_SWIFTCXX_THUNK;
    void makeInvalid(NSString*);
    void makeSubmitInvalid(NSString* = nil);
    void incrementBufferMapCount();
    void decrementBufferMapCount();
    void endEncoding(id<MTLCommandEncoder>);
    void setLastError(NSString*);
    bool waitForCommandBufferCompletion();
    bool encoderIsCurrent(id<MTLCommandEncoder>) const;
    bool submitWillBeInvalid() const { return m_makeSubmitInvalid; }
    void addBuffer(id<MTLBuffer>);
    void addBuffer(id<MTLCounterSampleBuffer>);
    void addICB(id<MTLIndirectCommandBuffer>);
    void addTexture(id<MTLTexture>);
    void addTexture(const Texture&);
    void addSampler(const Sampler&);
    id<MTLCommandBuffer> commandBuffer() const;
    void setExistingEncoder(id<MTLCommandEncoder>);
    void generateInvalidEncoderStateError();
    bool validateClearBuffer(const Buffer&, uint64_t offset, uint64_t size);
    static void trackEncoder(CommandEncoder&, Vector<uint64_t>&);
    static void trackEncoder(CommandEncoder&, HashSet<uint64_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>&);
    static size_t computeSize(Vector<uint64_t>&, const Device&);
    uint64_t uniqueId() const { return m_uniqueId; }
    NSMutableSet<id<MTLCounterSampleBuffer>> *timestampBuffers() const { return m_retainedTimestampBuffers; };
    void addOnCommitHandler(Function<bool(CommandBuffer&, CommandEncoder&)>&&);
#if ENABLE(WEBGPU_BY_DEFAULT)
    bool useResidencySet(id<MTLResidencySet>);
#endif
    void skippedDrawIndexedValidation(uint64_t bufferIdentifier, DrawIndexCacheContainerIterator);
    void rebindSamplersPreCommit(const BindGroup*);

private:
    CommandEncoder(id<MTLCommandBuffer>, Device&, uint64_t uniqueId);
    CommandEncoder(Device&);

    bool validatePopDebugGroup() const;
#if !ENABLE(WEBGPU_SWIFT)
    NSString* validateFinishError() const;
    NSString* errorValidatingCopyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size);
    NSString* errorValidatingComputePassDescriptor(const WGPUComputePassDescriptor&) const;
    NSString* errorValidatingRenderPassDescriptor(const WGPURenderPassDescriptor&) const;
    NSString* errorValidatingImageCopyBuffer(const WGPUImageCopyBuffer&) const;
    NSString* errorValidatingCopyBufferToTexture(const WGPUImageCopyBuffer&, const WGPUImageCopyTexture&, const WGPUExtent3D&) const;
    NSString* errorValidatingCopyTextureToBuffer(const WGPUImageCopyTexture&, const WGPUImageCopyBuffer&, const WGPUExtent3D&) const;
    NSString* errorValidatingCopyTextureToTexture(const WGPUImageCopyTexture& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize) const;
#endif
private PUBLIC_IN_WEBGPU_SWIFT:
    void discardCommandBuffer();
private:

    RefPtr<CommandBuffer> protectedCachedCommandBuffer() const { return m_cachedCommandBuffer.get(); }
    void retainTimestampsForOneUpdateLoop();

private PUBLIC_IN_WEBGPU_SWIFT:
    id<MTLCommandBuffer> m_commandBuffer { nil };
    id<MTLCommandEncoder> m_existingCommandEncoder { nil };
    id<MTLBlitCommandEncoder> m_blitCommandEncoder { nil };
    NSString* m_lastErrorString { nil };
    uint64_t m_debugGroupStackSize { 0 };
    ThreadSafeWeakPtr<CommandBuffer> m_cachedCommandBuffer;
#if CPU(X86_64) && (PLATFORM(MAC) || PLATFORM(MACCATALYST))
    NSMutableSet<id<MTLTexture>> *m_managedTextures { nil };
    NSMutableSet<id<MTLBuffer>> *m_managedBuffers { nil };
#endif
private:
    id<MTLSharedEvent> m_abortCommandBuffer { nil };


    NSMutableSet<id<MTLIndirectCommandBuffer>> *m_retainedICBs { nil };
    NSMutableSet<id<MTLTexture>> *m_retainedTextures { nil };
    NSMutableSet<id<MTLBuffer>> *m_retainedBuffers { nil };
    HashSet<RefPtr<const Sampler>> m_retainedSamplers;
    NSMutableSet<id<MTLCounterSampleBuffer>> *m_retainedTimestampBuffers { nil };
    Vector<Function<bool(CommandBuffer&, CommandEncoder&)>> m_onCommitHandlers;
    HashMap<uint64_t, Vector<DrawIndexCacheContainerValue>, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_skippedDrawIndexedValidationKeys;
    Vector<RefPtr<const BindGroup>> m_bindGroups;
private PUBLIC_IN_WEBGPU_SWIFT:
    int m_bufferMapCount { 0 };
    bool m_makeSubmitInvalid { false };
    id<MTLSharedEvent> m_sharedEvent { nil };
    uint64_t m_sharedEventSignalValue { 0 };
    const Ref<Device> m_device;
    uint64_t m_uniqueId;
#if ENABLE(WEBGPU_BY_DEFAULT)
    uint32_t m_currentResidencySetCount { 0 };
#endif
private:
// FIXME: remove @safe once rdar://151039766 lands
} __attribute__((swift_attr("@safe"))) SWIFT_SHARED_REFERENCE(refCommandEncoder, derefCommandEncoder);

} // namespace WebGPU

inline void refCommandEncoder(WebGPU::CommandEncoder* obj)
{
    WTF::ref(obj);
}

inline void derefCommandEncoder(WebGPU::CommandEncoder* obj)
{
    WTF::deref(obj);
}
