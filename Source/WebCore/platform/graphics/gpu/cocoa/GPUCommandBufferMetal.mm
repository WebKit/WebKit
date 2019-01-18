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
#import "GPUCommandBuffer.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"
#import "GPUQueue.h"
#import "Logging.h"

#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

RefPtr<GPUCommandBuffer> GPUCommandBuffer::create(GPUDevice& device)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUCommandBuffer::create(): Invalid GPUDevice!");
        return nullptr;
    }

    auto gpuCommandQueue = device.getQueue();
    if (!gpuCommandQueue)
        return nullptr;

    auto mtlQueue = gpuCommandQueue->platformQueue();

    PlatformCommandBufferSmartPtr buffer;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    buffer = retainPtr([mtlQueue commandBuffer]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!buffer) {
        LOG(WebGPU, "GPUCommandBuffer::create(): Unable to create MTLCommandBuffer!");
        return nullptr;
    }

    return adoptRef(new GPUCommandBuffer(WTFMove(buffer)));
}

GPUCommandBuffer::GPUCommandBuffer(PlatformCommandBufferSmartPtr&& buffer)
    : m_platformCommandBuffer(WTFMove(buffer))
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
