/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "GPUBindGroup.h"

#if ENABLE(WEBGPU)

#import "GPUBindGroupAllocator.h"
#import "GPUBindGroupBinding.h"
#import "GPUBindGroupDescriptor.h"
#import "GPUBindGroupLayout.h"
#import "GPUSampler.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/Optional.h>

namespace WebCore {
    
static Optional<GPUBufferBinding> tryGetResourceAsBufferBinding(const GPUBindingResource& resource, const char* const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    if (!WTF::holds_alternative<GPUBufferBinding>(resource)) {
        LOG(WebGPU, "%s: Resource is not a buffer type!", functionName);
        return WTF::nullopt;
    }
    auto& bufferBinding = WTF::get<GPUBufferBinding>(resource);
    if (!bufferBinding.buffer->platformBuffer()) {
        LOG(WebGPU, "%s: Invalid MTLBuffer in GPUBufferBinding!", functionName);
        return WTF::nullopt;
    }
    if (!WTF::isInBounds<NSUInteger>(bufferBinding.size) || bufferBinding.size > bufferBinding.buffer->byteLength()) {
        LOG(WebGPU, "%s: GPUBufferBinding size is too large!", functionName);
        return WTF::nullopt;
    }
    // MTLBuffer size (NSUInteger) is 32 bits on some platforms.
    if (!WTF::isInBounds<NSUInteger>(bufferBinding.offset)) {
        LOG(WebGPU, "%s: Buffer offset is too large!", functionName);
        return WTF::nullopt;
    }
    return GPUBufferBinding { bufferBinding.buffer.copyRef(), bufferBinding.offset, bufferBinding.size };
}

static void setBufferOnEncoder(MTLArgumentEncoder *argumentEncoder, const GPUBufferBinding& bufferBinding, unsigned name, unsigned lengthName)
{
    ASSERT(argumentEncoder && bufferBinding.buffer->platformBuffer());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    // Bounds check when converting GPUBufferBinding ensures that NSUInteger cast of uint64_t offset is safe.
    [argumentEncoder setBuffer:bufferBinding.buffer->platformBuffer() offset:static_cast<NSUInteger>(bufferBinding.offset) atIndex:name];
    void* lengthPointer = [argumentEncoder constantDataAtIndex:lengthName];
    memcpy(lengthPointer, &bufferBinding.size, sizeof(uint64_t));
    END_BLOCK_OBJC_EXCEPTIONS;
}
    
static MTLSamplerState *tryGetResourceAsMtlSampler(const GPUBindingResource& resource, const char* const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    if (!WTF::holds_alternative<Ref<const GPUSampler>>(resource)) {
        LOG(WebGPU, "%s: Resource is not a GPUSampler!", functionName);
        return nullptr;
    }
    auto samplerState = WTF::get<Ref<const GPUSampler>>(resource)->platformSampler();
    if (!samplerState) {
        LOG(WebGPU, "%s: Invalid MTLSamplerState in GPUSampler binding!", functionName);
        return nullptr;
    }
    return samplerState;
}

static void setSamplerOnEncoder(MTLArgumentEncoder *argumentEncoder, MTLSamplerState *sampler, unsigned index)
{
    ASSERT(argumentEncoder && sampler);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [argumentEncoder setSamplerState:sampler atIndex:index];
    END_BLOCK_OBJC_EXCEPTIONS;
}

static RefPtr<GPUTexture> tryGetResourceAsTexture(const GPUBindingResource& resource, const char* const functionName)
    {
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    if (!WTF::holds_alternative<Ref<GPUTexture>>(resource)) {
        LOG(WebGPU, "%s: Resource is not a GPUTextureView!", functionName);
        return nullptr;
    }
    auto& textureRef = WTF::get<Ref<GPUTexture>>(resource);
    if (!textureRef->platformTexture()) {
        LOG(WebGPU, "%s: Invalid MTLTexture in GPUTextureView binding!", functionName);
        return nullptr;
    }
    return textureRef.copyRef();
}

static void setTextureOnEncoder(MTLArgumentEncoder *argumentEncoder, MTLTexture *texture, unsigned index)
{
    ASSERT(argumentEncoder && texture);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [argumentEncoder setTexture:texture atIndex:index];
    END_BLOCK_OBJC_EXCEPTIONS;
}

RefPtr<GPUBindGroup> GPUBindGroup::tryCreate(const GPUBindGroupDescriptor& descriptor, GPUBindGroupAllocator& allocator)
{
    const char* const functionName = "GPUBindGroup::tryCreate()";
    
    MTLArgumentEncoder *vertexEncoder = descriptor.layout->vertexEncoder();
    MTLArgumentEncoder *fragmentEncoder = descriptor.layout->fragmentEncoder();
    MTLArgumentEncoder *computeEncoder = descriptor.layout->computeEncoder();

    auto offsets = allocator.allocateAndSetEncoders(vertexEncoder, fragmentEncoder, computeEncoder);
    if (!offsets)
        return nullptr;
    
    HashSet<Ref<GPUBuffer>> boundBuffers;
    HashSet<Ref<GPUTexture>> boundTextures;

    // Set each resource on each MTLArgumentEncoder it should be visible on.
    const auto& layoutBindingsMap = descriptor.layout->bindingsMap();
    for (const auto& resourceBinding : descriptor.bindings) {
        auto index = resourceBinding.binding;
        auto layoutIterator = layoutBindingsMap.find(index);
        if (layoutIterator == layoutBindingsMap.end()) {
            LOG(WebGPU, "%s: GPUBindGroupBinding %u not found in GPUBindGroupLayout!", functionName, index);
            return nullptr;
        }
        auto layoutBinding = layoutIterator->value;
        if (layoutBinding.externalBinding.visibility == GPUShaderStageBit::Flags::None)
            continue;

        bool isForVertex = layoutBinding.externalBinding.visibility & GPUShaderStageBit::Flags::Vertex;
        bool isForFragment = layoutBinding.externalBinding.visibility & GPUShaderStageBit::Flags::Fragment;
        bool isForCompute = layoutBinding.externalBinding.visibility & GPUShaderStageBit::Flags::Compute;

        if (isForVertex && !vertexEncoder) {
            LOG(WebGPU, "%s: No vertex argument encoder found for binding %u!", functionName, index);
            return nullptr;
        }
        if (isForFragment && !fragmentEncoder) {
            LOG(WebGPU, "%s: No fragment argument encoder found for binding %u!", functionName, index);
            return nullptr;
        }
        if (isForCompute && !computeEncoder) {
            LOG(WebGPU, "%s: No compute argument encoder found for binding %u!", functionName, index);
            return nullptr;
        }

        auto handleBuffer = [&](unsigned internalLengthName) -> bool {
            auto bufferResource = tryGetResourceAsBufferBinding(resourceBinding.resource, functionName);
            if (!bufferResource)
                return false;
            if (isForVertex)
                setBufferOnEncoder(vertexEncoder, *bufferResource, layoutBinding.internalName, internalLengthName);
            if (isForFragment)
                setBufferOnEncoder(fragmentEncoder, *bufferResource, layoutBinding.internalName, internalLengthName);
            if (isForCompute)
                setBufferOnEncoder(computeEncoder, *bufferResource, layoutBinding.internalName, internalLengthName);
            boundBuffers.addVoid(WTFMove(bufferResource->buffer));
            return true;
        };

        auto success = WTF::visit(WTF::makeVisitor([&](GPUBindGroupLayout::UniformBuffer& uniformBuffer) -> bool {
            return handleBuffer(uniformBuffer.internalLengthName);
        }, [&](GPUBindGroupLayout::DynamicUniformBuffer& dynamicUniformBuffer) -> bool {
            return handleBuffer(dynamicUniformBuffer.internalLengthName);
        }, [&](GPUBindGroupLayout::Sampler&) -> bool {
            auto samplerState = tryGetResourceAsMtlSampler(resourceBinding.resource, functionName);
            if (!samplerState)
                return false;
            if (isForVertex)
                setSamplerOnEncoder(vertexEncoder, samplerState, layoutBinding.internalName);
            if (isForFragment)
                setSamplerOnEncoder(fragmentEncoder, samplerState, layoutBinding.internalName);
            if (isForCompute)
                setSamplerOnEncoder(computeEncoder, samplerState, layoutBinding.internalName);
            return true;
        }, [&](GPUBindGroupLayout::SampledTexture&) -> bool {
            auto textureResource = tryGetResourceAsTexture(resourceBinding.resource, functionName);
            if (!textureResource)
                return false;
            if (isForVertex)
                setTextureOnEncoder(vertexEncoder, textureResource->platformTexture(), layoutBinding.internalName);
            if (isForFragment)
                setTextureOnEncoder(fragmentEncoder, textureResource->platformTexture(), layoutBinding.internalName);
            if (isForCompute)
                setTextureOnEncoder(computeEncoder, textureResource->platformTexture(), layoutBinding.internalName);
            boundTextures.addVoid(textureResource.releaseNonNull());
            return true;
        }, [&](GPUBindGroupLayout::StorageBuffer& storageBuffer) -> bool {
            return handleBuffer(storageBuffer.internalLengthName);
        }, [&](GPUBindGroupLayout::DynamicStorageBuffer& dynamicStorageBuffer) -> bool {
            return handleBuffer(dynamicStorageBuffer.internalLengthName);
        }), layoutBinding.internalBindingDetails);
        if (!success)
            return nullptr;
    }
    
    return adoptRef(new GPUBindGroup(WTFMove(*offsets), allocator, WTFMove(boundBuffers), WTFMove(boundTextures)));
}
    
GPUBindGroup::GPUBindGroup(GPUBindGroupAllocator::ArgumentBufferOffsets&& offsets, GPUBindGroupAllocator& allocator, HashSet<Ref<GPUBuffer>>&& buffers, HashSet<Ref<GPUTexture>>&& textures)
    : m_argumentBufferOffsets(WTFMove(offsets))
    , m_allocator(makeRef(allocator))
    , m_boundBuffers(WTFMove(buffers))
    , m_boundTextures(WTFMove(textures))
{
}

GPUBindGroup::~GPUBindGroup()
{
    GPUBindGroupAllocator& rawAllocator = m_allocator.leakRef();
    rawAllocator.deref();
    rawAllocator.tryReset();
}
    
} // namespace WebCore

#endif // ENABLE(WEBPGU)
