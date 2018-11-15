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
#import "GPUQueue.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"
#import "Logging.h"

#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

RefPtr<GPUQueue> GPUQueue::create(const GPUDevice& device)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUQueue::create(): Invalid GPUDevice!");
        return nullptr;
    }

    PlatformQueueSmartPtr queue;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    queue = adoptNS([device.platformDevice() newCommandQueue]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!queue) {
        LOG(WebGPU, "GPUQueue::create(): Unable to create MTLCommandQueue!");
        return nullptr;
    }

    return adoptRef(new GPUQueue(WTFMove(queue)));
}

GPUQueue::GPUQueue(PlatformQueueSmartPtr&& queue)
    : m_platformQueue(WTFMove(queue))
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
