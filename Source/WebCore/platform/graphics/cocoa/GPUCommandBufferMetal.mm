/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "GPUCommandBuffer.h"

#if ENABLE(WEBGPU)

#import "GPUCommandQueue.h"
#import "GPUDevice.h"
#import "GPUDrawable.h"
#import "Logging.h"

#import <Metal/Metal.h>
#import <wtf/BlockPtr.h>

namespace WebCore {

GPUCommandBuffer::GPUCommandBuffer(GPUCommandQueue* queue)
{
    LOG(WebGPU, "GPUCommandBuffer::GPUCommandBuffer()");

    if (!queue || !queue->platformCommandQueue())
        return;

    m_commandBuffer = (MTLCommandBuffer *)[queue->platformCommandQueue() commandBuffer];
}

MTLCommandBuffer *GPUCommandBuffer::platformCommandBuffer()
{
    return m_commandBuffer.get();
}

void GPUCommandBuffer::presentDrawable(GPUDrawable* drawable)
{
    if (!m_commandBuffer || !drawable->platformDrawable())
        return;

    [m_commandBuffer presentDrawable:static_cast<id<MTLDrawable>>(drawable->platformDrawable())];
    drawable->release();
}

void GPUCommandBuffer::commit()
{
    if (!m_commandBuffer)
        return;

    [m_commandBuffer commit];
}

DOMPromiseProxy<IDLVoid>& GPUCommandBuffer::completed()
{
    if (!m_commandBuffer)
        return m_completedPromise;

    [m_commandBuffer addCompletedHandler:BlockPtr<void (id<MTLCommandBuffer>)>::fromCallable([this, protectedThis = makeRef(*this)] (id<MTLCommandBuffer>) {
        callOnMainThread([this, protectedThis = makeRef(*this)] {
            this->m_completedPromise.resolve();
        });
    }).get()];
    
    return m_completedPromise;
}

} // namespace WebCore

#endif
