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
#import "GPUCommandQueue.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"
#import "Logging.h"

#import <Metal/Metal.h>

static NSString *webkitPrefix = @"com.apple.WebKit";

namespace WebCore {

GPUCommandQueue::GPUCommandQueue(GPUDevice* device)
{
    LOG(WebGPU, "GPUCommandQueue::GPUCommandQueue()");

    if (!device || !device->platformDevice())
        return;

    m_commandQueue = adoptNS((MTLCommandQueue *)[device->platformDevice() newCommandQueue]);
    setLabel(emptyString());
}

String GPUCommandQueue::label() const
{
    if (!m_commandQueue)
        return emptyString();

    NSString *prefixedLabel = [m_commandQueue label];

    if ([prefixedLabel isEqualToString:webkitPrefix])
        return emptyString();

    ASSERT(prefixedLabel.length >= (webkitPrefix.length + 1));
    return [prefixedLabel substringFromIndex:(webkitPrefix.length + 1)];
}

void GPUCommandQueue::setLabel(const String& label)
{
    ASSERT(m_commandQueue);
    if (label.isEmpty())
        [m_commandQueue setLabel:webkitPrefix];
    else
        [m_commandQueue setLabel:[NSString stringWithFormat:@"%@.%@", webkitPrefix, static_cast<NSString *>(label)]];
}
    
MTLCommandQueue *GPUCommandQueue::platformCommandQueue()
{
    return m_commandQueue.get();
}

} // namespace WebCore

#endif
