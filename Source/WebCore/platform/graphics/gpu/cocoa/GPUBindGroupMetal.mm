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

#import "GPUBindGroupBinding.h"
#import "GPUBindGroupDescriptor.h"
#import "GPUBindGroupLayout.h"
#import "GPUSampler.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Optional.h>

namespace WebCore {

static RetainPtr<MTLBuffer> tryCreateArgumentBuffer(MTLArgumentEncoder *encoder)
{
    RetainPtr<MTLBuffer> buffer;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    buffer = adoptNS([encoder.device newBufferWithLength:encoder.encodedLength options:0]);
    [encoder setArgumentBuffer:buffer.get() offset:0];
    END_BLOCK_OBJC_EXCEPTIONS;
    return buffer;
}
    
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
    return GPUBufferBinding { bufferBinding.buffer.copyRef(), bufferBinding.offset, bufferBinding.size };
}

static void setBufferOnEncoder(MTLArgumentEncoder *argumentEncoder, const GPUBufferBinding& bufferBinding, unsigned index)
{
    ASSERT(argumentEncoder && bufferBinding.buffer->platformBuffer());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [argumentEncoder setBuffer:bufferBinding.buffer->platformBuffer() offset:bufferBinding.offset atIndex:index];
    END_BLOCK_OBJC_EXCEPTIONS;
}
    
static MTLSamplerState *tryGetResourceAsMTLSamplerState(const GPUBindingResource& resource, const char* const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    if (!WTF::holds_alternative<Ref<GPUSampler>>(resource)) {
        LOG(WebGPU, "%s: Resource is not a GPUSampler!", functionName);
        return nullptr;
    }
    auto samplerState = WTF::get<Ref<GPUSampler>>(resource)->platformSampler();
    if (!samplerState)
        LOG(WebGPU, "%s: Invalid MTLSamplerState in GPUSampler binding!", functionName);

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
    
RefPtr<GPUBindGroup> GPUBindGroup::tryCreate(const GPUBindGroupDescriptor& descriptor)
{
    const char* const functionName = "GPUBindGroup::tryCreate()";
    
    MTLArgumentEncoder *vertexEncoder = descriptor.layout->vertexEncoder();
    MTLArgumentEncoder *fragmentEncoder = descriptor.layout->fragmentEncoder();
    MTLArgumentEncoder *computeEncoder = descriptor.layout->computeEncoder();
    
    RetainPtr<MTLBuffer> vertexArgsBuffer;
    if (vertexEncoder && !(vertexArgsBuffer = tryCreateArgumentBuffer(vertexEncoder))) {
        LOG(WebGPU, "%s: Unable to create MTLBuffer for vertex argument buffer!", functionName);
        return nullptr;
    }
    RetainPtr<MTLBuffer> fragmentArgsBuffer;
    if (fragmentEncoder && !(fragmentArgsBuffer = tryCreateArgumentBuffer(fragmentEncoder))) {
        LOG(WebGPU, "%s: Unable to create MTLBuffer for fragment argument buffer!", functionName);
        return nullptr;
    }
    RetainPtr<MTLBuffer> computeArgsBuffer;
    if (computeEncoder && !(computeArgsBuffer = tryCreateArgumentBuffer(computeEncoder))) {
        LOG(WebGPU, "%s: Unable to create MTLBuffer for compute argument buffer!", functionName);
        return nullptr;
    }
    
    Vector<Ref<GPUBuffer>> boundBuffers;
    Vector<Ref<GPUTexture>> boundTextures;

    // Set each resource on each MTLArgumentEncoder it should be visible on.
    const auto& layoutBindingsMap = descriptor.layout->bindingsMap();
    for (const auto& resourceBinding : descriptor.bindings) {
        auto index = resourceBinding.binding;
        auto layoutIterator = layoutBindingsMap.find(index);
        if (layoutIterator == layoutBindingsMap.end()) {
            LOG(WebGPU, "%s: GPUBindGroupBinding %lu not found in GPUBindGroupLayout!", functionName, index);
            return nullptr;
        }
        auto layoutBinding = layoutIterator->value;
        if (layoutBinding.visibility == GPUShaderStageBit::Flags::None)
            continue;

        bool isForVertex = layoutBinding.visibility & GPUShaderStageBit::Flags::Vertex;
        bool isForFragment = layoutBinding.visibility & GPUShaderStageBit::Flags::Fragment;
        bool isForCompute = layoutBinding.visibility & GPUShaderStageBit::Flags::Compute;

        if (isForVertex && !vertexEncoder) {
            LOG(WebGPU, "%s: No vertex argument encoder found for binding %lu!", functionName, index);
            return nullptr;
        }
        if (isForFragment && !fragmentEncoder) {
            LOG(WebGPU, "%s: No fragment argument encoder found for binding %lu!", functionName, index);
            return nullptr;
        }
        if (isForCompute && !computeEncoder) {
            LOG(WebGPU, "%s: No compute argument encoder found for binding %lu!", functionName, index);
            return nullptr;
        }

        switch (layoutBinding.type) {
        // FIXME: Support more resource types.
        // FIXME: We could avoid this ugly switch-on-type using virtual functions if GPUBindingResource is refactored as a base class rather than a Variant.
        case GPUBindingType::UniformBuffer:
        case GPUBindingType::StorageBuffer: {
            auto bufferResource = tryGetResourceAsBufferBinding(resourceBinding.resource, functionName);
            if (!bufferResource)
                return nullptr;
            if (isForVertex)
                setBufferOnEncoder(vertexEncoder, *bufferResource, index);
            if (isForFragment)
                setBufferOnEncoder(fragmentEncoder, *bufferResource, index);
            if (isForCompute)
                setBufferOnEncoder(computeEncoder, *bufferResource, index);
            boundBuffers.append(bufferResource->buffer.copyRef());
            break;
        }
        case GPUBindingType::Sampler: {
            auto samplerState = tryGetResourceAsMTLSamplerState(resourceBinding.resource, functionName);
            if (!samplerState)
                return nullptr;
            if (isForVertex)
                setSamplerOnEncoder(vertexEncoder, samplerState, index);
            if (isForFragment)
                setSamplerOnEncoder(fragmentEncoder, samplerState, index);
            if (isForCompute)
                setSamplerOnEncoder(computeEncoder, samplerState, index);
            break;
        }
        case GPUBindingType::SampledTexture: {
            auto textureResource = tryGetResourceAsTexture(resourceBinding.resource, functionName);
            if (!textureResource)
                return nullptr;
            if (isForVertex)
                setTextureOnEncoder(vertexEncoder, textureResource->platformTexture(), index);
            if (isForFragment)
                setTextureOnEncoder(fragmentEncoder, textureResource->platformTexture(), index);
            if (isForCompute)
                setTextureOnEncoder(computeEncoder, textureResource->platformTexture(), index);
            boundTextures.append(textureResource.releaseNonNull());
            break;
        }
        default:
            LOG(WebGPU, "%s: Resource type not yet implemented.", functionName);
            return nullptr;
        }
    }
    
    return adoptRef(new GPUBindGroup(WTFMove(vertexArgsBuffer), WTFMove(fragmentArgsBuffer), WTFMove(computeArgsBuffer), WTFMove(boundBuffers), WTFMove(boundTextures)));
}
    
GPUBindGroup::GPUBindGroup(RetainPtr<MTLBuffer>&& vertexBuffer, RetainPtr<MTLBuffer>&& fragmentBuffer, RetainPtr<MTLBuffer>&& computeBuffer, Vector<Ref<GPUBuffer>>&& buffers, Vector<Ref<GPUTexture>>&& textures)
    : m_vertexArgsBuffer(WTFMove(vertexBuffer))
    , m_fragmentArgsBuffer(WTFMove(fragmentBuffer))
    , m_computeArgsBuffer(WTFMove(computeBuffer))
    , m_boundBuffers(WTFMove(buffers))
    , m_boundTextures(WTFMove(textures))
{
}
    
} // namespace WebCore

#endif // ENABLE(WEBPGU)
