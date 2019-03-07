/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "GPUProgrammablePassEncoder.h"

#if ENABLE(WEBGPU)

#import "GPUBindGroup.h"
#import "GPUBindGroupLayoutBinding.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

void GPUProgrammablePassEncoder::endPass()
{
    ASSERT(platformPassEncoder());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [platformPassEncoder() endEncoding];
    END_BLOCK_OBJC_EXCEPTIONS;

    m_commandBuffer->setIsEncodingPass(false);
}

void GPUProgrammablePassEncoder::setBindGroup(unsigned index, GPUBindGroup& bindGroup)
{
    const char* const functionName = "GPUProgrammablePassEncoder::setBindGroup()";

    if (!platformPassEncoder()) {
        LOG(WebGPU, "%s: Invalid operation: Encoding is ended!");
        return;
    }

    const auto& vertexArgs = bindGroup.layout().vertexArguments();
    const auto& fragmentArgs = bindGroup.layout().fragmentArguments();
    // FIXME: Finish support for compute.

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (vertexArgs.buffer)
        setVertexBuffer(vertexArgs.buffer.get(), 0, index);
    if (fragmentArgs.buffer)
        setFragmentBuffer(fragmentArgs.buffer.get(), 0, index);

    END_BLOCK_OBJC_EXCEPTIONS;

    // Set each resource on each MTLArgumentEncoder it should be visible on.
    const auto& layoutBindingsMap = bindGroup.layout().bindingsMap();
    for (const auto& resourceBinding : bindGroup.bindings()) {
        auto layoutIterator = layoutBindingsMap.find(resourceBinding.binding);
        if (layoutIterator == layoutBindingsMap.end()) {
            LOG(WebGPU, "%s: GPUBindGroupBinding %lu not found in GPUBindGroupLayout!", functionName, resourceBinding.binding);
            return;
        }
        auto layoutBinding = layoutIterator->value;

        switch (layoutBinding.type) {
        // FIXME: Support more resource types.
        case GPUBindGroupLayoutBinding::BindingType::UniformBuffer:
        case GPUBindGroupLayoutBinding::BindingType::StorageBuffer: {
            if (layoutBinding.visibility & GPUShaderStageBit::Flags::Vertex)
                setResourceAsBufferOnEncoder(vertexArgs.encoder.get(), resourceBinding, functionName);
            if (layoutBinding.visibility & GPUShaderStageBit::Flags::Fragment)
                setResourceAsBufferOnEncoder(fragmentArgs.encoder.get(), resourceBinding, functionName);
            break;
        }
        case GPUBindGroupLayoutBinding::BindingType::Sampler: {
            if (layoutBinding.visibility & GPUShaderStageBit::Flags::Vertex)
                setResourceAsSamplerOnEncoder(vertexArgs.encoder.get(), resourceBinding, functionName);
            if (layoutBinding.visibility & GPUShaderStageBit::Flags::Fragment)
                setResourceAsSamplerOnEncoder(fragmentArgs.encoder.get(), resourceBinding, functionName);
            break;
        }
        case GPUBindGroupLayoutBinding::BindingType::SampledTexture: {
            if (layoutBinding.visibility & GPUShaderStageBit::Flags::Vertex)
                setResourceAsTextureOnEncoder(vertexArgs.encoder.get(), resourceBinding, functionName);
            if (layoutBinding.visibility & GPUShaderStageBit::Flags::Fragment)
                setResourceAsTextureOnEncoder(fragmentArgs.encoder.get(), resourceBinding, functionName);
            break;
        }
        default:
            LOG(WebGPU, "%s: Resource type not yet implemented.", functionName);
            return;
        }
    }
}

void GPUProgrammablePassEncoder::setResourceAsBufferOnEncoder(MTLArgumentEncoder *argumentEncoder, const GPUBindGroupBinding& binding, const char* const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    if (!argumentEncoder) {
        LOG(WebGPU, "%s: No argument encoder for requested stage found!", functionName);
        return;
    }

    if (!WTF::holds_alternative<GPUBufferBinding>(binding.resource)) {
        LOG(WebGPU, "%s: Resource is not a buffer type!", functionName);
        return;
    }

    auto& bufferBinding = WTF::get<GPUBufferBinding>(binding.resource);
    auto& bufferRef = bufferBinding.buffer;
    auto mtlBuffer = bufferRef->platformBuffer();

    if (!mtlBuffer) {
        LOG(WebGPU, "%s: Invalid MTLBuffer in GPUBufferBinding!", functionName);
        return;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    [argumentEncoder setBuffer:mtlBuffer offset:bufferBinding.offset atIndex:binding.binding];
    useResource(mtlBuffer, bufferRef->isReadOnly() ? MTLResourceUsageRead : MTLResourceUsageRead | MTLResourceUsageWrite);

    END_BLOCK_OBJC_EXCEPTIONS;

    m_commandBuffer->useBuffer(bufferRef.copyRef());
}

void GPUProgrammablePassEncoder::setResourceAsSamplerOnEncoder(MTLArgumentEncoder *argumentEncoder, const GPUBindGroupBinding& binding, const char *const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    if (!argumentEncoder) {
        LOG(WebGPU, "%s: No argument encoder for requested stage found!", functionName);
        return;
    }

    if (!WTF::holds_alternative<Ref<GPUSampler>>(binding.resource)) {
        LOG(WebGPU, "%s: Resource is not a GPUSampler!", functionName);
        return;
    }

    auto& samplerRef = WTF::get<Ref<GPUSampler>>(binding.resource);
    auto mtlSampler = samplerRef->platformSampler();

    if (!mtlSampler) {
        LOG(WebGPU, "%s: Invalid MTLSamplerState in GPUSampler binding!", functionName);
        return;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [argumentEncoder setSamplerState:mtlSampler atIndex:binding.binding];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void GPUProgrammablePassEncoder::setResourceAsTextureOnEncoder(MTLArgumentEncoder *argumentEncoder, const GPUBindGroupBinding& binding, const char* const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    if (!argumentEncoder) {
        LOG(WebGPU, "%s: No argument encoder for requested stage found!", functionName);
        return;
    }

    if (!WTF::holds_alternative<Ref<GPUTexture>>(binding.resource)) {
        LOG(WebGPU, "%s: Resource is not a GPUTextureView!", functionName);
        return;
    }

    auto& textureRef = WTF::get<Ref<GPUTexture>>(binding.resource);
    auto mtlTexture = textureRef->platformTexture();

    if (!mtlTexture) {
        LOG(WebGPU, "%s: Invalid MTLTexture in GPUTextureView binding!", functionName);
        return;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    [argumentEncoder setTexture:mtlTexture atIndex:binding.binding];
    useResource(mtlTexture, textureRef->isReadOnly() ? MTLResourceUsageRead : MTLResourceUsageRead | MTLResourceUsageWrite);

    END_BLOCK_OBJC_EXCEPTIONS;

    m_commandBuffer->useTexture(textureRef.copyRef());
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
