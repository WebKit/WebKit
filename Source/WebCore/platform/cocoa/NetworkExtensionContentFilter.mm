/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "NetworkExtensionContentFilter.h"

#if HAVE(NE_FILTER_SOURCE)

#import "NEFilterSourceSPI.h"
#import "ResourceResponse.h"
#import "SoftLinking.h"
#import <objc/runtime.h>

SOFT_LINK_FRAMEWORK(NetworkExtension);
SOFT_LINK_CLASS(NetworkExtension, NEFilterSource);

namespace WebCore {

bool NetworkExtensionContentFilter::canHandleResponse(const ResourceResponse& response)
{
    return response.url().protocolIsInHTTPFamily() && [getNEFilterSourceClass() filterRequired];
}

std::unique_ptr<NetworkExtensionContentFilter> NetworkExtensionContentFilter::create(const ResourceResponse& response)
{
    return std::make_unique<NetworkExtensionContentFilter>(response);
}

NetworkExtensionContentFilter::NetworkExtensionContentFilter(const ResourceResponse& response)
    : m_neFilterSourceStatus { NEFilterSourceStatusNeedsMoreData }
    , m_neFilterSource { adoptNS([allocNEFilterSourceInstance() initWithURL:[response.nsURLResponse() URL] direction:NEFilterSourceDirectionInbound socketIdentifier:0]) }
    , m_neFilterSourceQueue { dispatch_queue_create("com.apple.WebCore.NEFilterSourceQueue", DISPATCH_QUEUE_SERIAL) }
{
    ASSERT([getNEFilterSourceClass() filterRequired]);

    long long expectedContentSize = [response.nsURLResponse() expectedContentLength];
    if (expectedContentSize < 0)
        m_originalData = adoptNS([[NSMutableData alloc] init]);
    else
        m_originalData = adoptNS([[NSMutableData alloc] initWithCapacity:(NSUInteger)expectedContentSize]);
}

NetworkExtensionContentFilter::~NetworkExtensionContentFilter()
{
    dispatch_release(m_neFilterSourceQueue);
}

void NetworkExtensionContentFilter::addData(const char* data, int length)
{
    // FIXME: NEFilterSource doesn't buffer data like WebFilterEvaluator does,
    // so we need to do it ourselves so getReplacementData() can return the
    // original bytes back to the loader. We should find a way to remove this
    // additional copy.
    [m_originalData appendBytes:data length:length];

    dispatch_semaphore_t neFilterSourceSemaphore = dispatch_semaphore_create(0);
    [m_neFilterSource addData:[NSData dataWithBytes:(void*)data length:length] withCompletionQueue:m_neFilterSourceQueue completionHandler:^(NEFilterSourceStatus status, NSData *) {
        m_neFilterSourceStatus = status;
        dispatch_semaphore_signal(neFilterSourceSemaphore);
    }];

    // FIXME: We have to block here since DocumentLoader expects to have a
    // blocked/not blocked answer from the filter immediately after calling
    // addData(). We should find a way to make this asynchronous.
    dispatch_semaphore_wait(neFilterSourceSemaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(neFilterSourceSemaphore);
}

void NetworkExtensionContentFilter::finishedAddingData()
{
    dispatch_semaphore_t neFilterSourceSemaphore = dispatch_semaphore_create(0);
    [m_neFilterSource dataCompleteWithCompletionQueue:m_neFilterSourceQueue completionHandler:^(NEFilterSourceStatus status, NSData *) {
        m_neFilterSourceStatus = status;
        dispatch_semaphore_signal(neFilterSourceSemaphore);
    }];

    // FIXME: We have to block here since DocumentLoader expects to have a
    // blocked/not blocked answer from the filter immediately after calling
    // finishedAddingData(). We should find a way to make this asynchronous.
    dispatch_semaphore_wait(neFilterSourceSemaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(neFilterSourceSemaphore);
}

bool NetworkExtensionContentFilter::needsMoreData() const
{
    return m_neFilterSourceStatus == NEFilterSourceStatusNeedsMoreData;
}

bool NetworkExtensionContentFilter::didBlockData() const
{
    return m_neFilterSourceStatus == NEFilterSourceStatusBlock;
}

const char* NetworkExtensionContentFilter::getReplacementData(int& length) const
{
    if (didBlockData()) {
        length = 0;
        return nullptr;
    }

    length = [m_originalData length];
    return static_cast<const char*>([m_originalData bytes]);
}

ContentFilterUnblockHandler NetworkExtensionContentFilter::unblockHandler() const
{
    return { };
}

} // namespace WebCore

#endif // HAVE(NE_FILTER_SOURCE)
