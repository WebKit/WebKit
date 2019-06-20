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

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [platformPassEncoder() endEncoding];
    END_BLOCK_OBJC_EXCEPTIONS;

    invalidateEncoder();
    m_commandBuffer->setIsEncodingPass(false);
}

void GPUProgrammablePassEncoder::setBindGroup(unsigned index, GPUBindGroup& bindGroup)
{
    if (!platformPassEncoder()) {
        LOG(WebGPU, "GPUProgrammablePassEncoder::setBindGroup(): Invalid operation: Encoding is ended!");
        return;
    }
    
    if (bindGroup.vertexArgsBuffer())
        setVertexBuffer(bindGroup.vertexArgsBuffer(), 0, index);
    if (bindGroup.fragmentArgsBuffer())
        setFragmentBuffer(bindGroup.fragmentArgsBuffer(), 0, index);
    if (bindGroup.computeArgsBuffer())
        setComputeBuffer(bindGroup.computeArgsBuffer(), 0, index);

    for (auto& bufferRef : bindGroup.boundBuffers()) {
        MTLResourceUsage usage = 0;
        if (bufferRef->isUniform()) {
            ASSERT(!bufferRef->isStorage());
            usage = MTLResourceUsageRead;
        } else if (bufferRef->isStorage()) {
            ASSERT(!bufferRef->isUniform());
            usage = MTLResourceUsageRead | MTLResourceUsageWrite;
        }
        useResource(bufferRef->platformBuffer(), usage);
        m_commandBuffer->useBuffer(bufferRef.copyRef());
    }
    for (auto& textureRef : bindGroup.boundTextures()) {
        MTLResourceUsage usage = 0;
        if (textureRef->isSampled()) {
            ASSERT(!textureRef->isStorage());
            usage = MTLResourceUsageRead | MTLResourceUsageSample;
        } else if (textureRef->isStorage()) {
            ASSERT(!textureRef->isSampled());
            usage = MTLResourceUsageRead | MTLResourceUsageWrite;
        }
        useResource(textureRef->platformTexture(), usage);
        m_commandBuffer->useTexture(textureRef.copyRef());
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
