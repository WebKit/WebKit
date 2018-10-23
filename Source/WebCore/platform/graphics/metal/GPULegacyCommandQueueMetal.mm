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
#import "GPULegacyCommandQueue.h"

#if ENABLE(WEBMETAL)

#import "GPULegacyDevice.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

static NSString * const commandQueueDefaultLabel = @"com.apple.WebKit";
static NSString * const commandQueueLabelPrefix = @"com.apple.WebKit.";

GPULegacyCommandQueue::GPULegacyCommandQueue(const GPULegacyDevice& device)
    : m_metal { adoptNS([device.metal() newCommandQueue]) }
{
    LOG(WebMetal, "GPULegacyCommandQueue::GPULegacyCommandQueue()");

    [m_metal setLabel:commandQueueDefaultLabel];
}

String GPULegacyCommandQueue::label() const
{
    if (!m_metal)
        return emptyString();

    NSString *prefixedLabel = [m_metal label];

    if ([prefixedLabel isEqualToString:commandQueueDefaultLabel])
        return emptyString();

    ASSERT(prefixedLabel.length > commandQueueLabelPrefix.length);
    return [prefixedLabel substringFromIndex:commandQueueLabelPrefix.length];
}

void GPULegacyCommandQueue::setLabel(const String& label) const
{
    if (label.isEmpty())
        [m_metal setLabel:commandQueueDefaultLabel];
    else
        [m_metal setLabel:[commandQueueLabelPrefix stringByAppendingString:label]];
}

} // namespace WebCore

#endif
