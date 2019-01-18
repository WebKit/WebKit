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
    if (!m_isEncoding)
        return;

    [platformPassEncoder() endEncoding];
    m_isEncoding = false;
}

void GPUProgrammablePassEncoder::setResourceAsBufferOnEncoder(MTLArgumentEncoder *argumentEncoder, const GPUBindingResource& resource, unsigned long index, const char* const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    if (!argumentEncoder) {
        LOG(WebGPU, "%s: No argument encoder for requested stage found!", functionName);
        return;
    }

    if (!WTF::holds_alternative<GPUBufferBinding>(resource)) {
        LOG(WebGPU, "%s: Resource is not a buffer type!", functionName);
        return;
    }

    auto& bufferBinding = WTF::get<GPUBufferBinding>(resource);
    auto buffer = bufferBinding.buffer->platformBuffer();

    if (!buffer) {
        LOG(WebGPU, "%s: Invalid MTLBuffer in GPUBufferBinding!", functionName);
        return;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    [argumentEncoder setBuffer:buffer offset:bufferBinding.offset atIndex:index];
    useResource(buffer, MTLResourceUsageRead);

    END_BLOCK_OBJC_EXCEPTIONS;
}

void GPUProgrammablePassEncoder::setBindGroup(unsigned long index, const GPUBindGroup& bindGroup)
{
    const char* const functionName = "GPUProgrammablePassEncoder::setBindGroup()";
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif

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
    const auto& bindingsMap = bindGroup.layout().bindingsMap();
    for (const auto& binding : bindGroup.bindings()) {
        auto iterator = bindingsMap.find(binding.binding);
        if (iterator == bindingsMap.end()) {
            LOG(WebGPU, "%s: GPUBindGroupBinding %lu not found in GPUBindGroupLayout!", functionName, binding.binding);
            return;
        }
        auto bindGroupLayoutBinding = iterator->value;

        switch (bindGroupLayoutBinding.type) {
        // FIXME: Support more resource types.
        case GPUBindGroupLayoutBinding::BindingType::UniformBuffer:
        case GPUBindGroupLayoutBinding::BindingType::StorageBuffer: {
            if (bindGroupLayoutBinding.visibility & GPUShaderStageBit::VERTEX)
                setResourceAsBufferOnEncoder(vertexArgs.encoder.get(), binding.resource, binding.binding, functionName);
            if (bindGroupLayoutBinding.visibility & GPUShaderStageBit::FRAGMENT)
                setResourceAsBufferOnEncoder(fragmentArgs.encoder.get(), binding.resource, binding.binding, functionName);
            break;
        }
        default:
            LOG(WebGPU, "%s: Resource type not yet implemented.", functionName);
            return;
        }
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
