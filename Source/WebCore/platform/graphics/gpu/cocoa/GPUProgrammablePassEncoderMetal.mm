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
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

void GPUProgrammablePassEncoder::endPass()
{
    if (!platformPassEncoder())
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [platformPassEncoder() endEncoding];
    END_BLOCK_OBJC_EXCEPTIONS

    invalidateEncoder();
    m_commandBuffer->setIsEncodingPass(false);
}

void GPUProgrammablePassEncoder::setBindGroup(unsigned index, GPUBindGroup& bindGroup)
{
    if (!platformPassEncoder()) {
        LOG(WebGPU, "GPUProgrammablePassEncoder::setBindGroup(): Invalid operation: Encoding is ended!");
        return;
    }

    auto argumentBuffer = bindGroup.argumentBuffer();
    if (!argumentBuffer.first)
        return;

    if (argumentBuffer.second.vertex)
        setVertexBuffer(argumentBuffer.first, *argumentBuffer.second.vertex, index);
    if (argumentBuffer.second.fragment)
        setFragmentBuffer(argumentBuffer.first, *argumentBuffer.second.fragment, index);
    if (argumentBuffer.second.compute)
        setComputeBuffer(argumentBuffer.first, *argumentBuffer.second.compute, index);

    for (auto& buffer : bindGroup.boundBuffers()) {
        useResource(buffer->platformBuffer(), static_cast<MTLResourceUsage>(buffer->platformUsage()));
        m_commandBuffer->useBuffer(buffer.copyRef());
    }
    for (auto& texture : bindGroup.boundTextures()) {
        useResource(texture->platformTexture(), static_cast<MTLResourceUsage>(texture->platformUsage()));
        m_commandBuffer->useTexture(texture.copyRef());
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
