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

#if HAVE(NETWORK_EXTENSION)

#import "NEFilterSourceSPI.h"
#import "ResourceResponse.h"
#import "SoftLinking.h"
#import <objc/runtime.h>

SOFT_LINK_FRAMEWORK(NetworkExtension);
SOFT_LINK_CLASS(NetworkExtension, NEFilterSource);

#if HAVE(MODERN_NE_FILTER_SOURCE)
// FIXME: <rdar://problem/20165664> Expose decisionHandler dictionary keys as NSString constants in NEFilterSource.h
static NSString * const optionsPageData = @"PageData";

static inline NSData *replacementDataFromDecisionInfo(NSDictionary *decisionInfo)
{
    id replacementData = decisionInfo[optionsPageData];
    ASSERT(!replacementData || [replacementData isKindOfClass:[NSData class]]);
    return replacementData;
}
#endif

namespace WebCore {

bool NetworkExtensionContentFilter::canHandleResponse(const ResourceResponse& response)
{
    return response.url().protocolIsInHTTPFamily() && [getNEFilterSourceClass() filterRequired];
}

std::unique_ptr<NetworkExtensionContentFilter> NetworkExtensionContentFilter::create(const ResourceResponse& response)
{
    return std::make_unique<NetworkExtensionContentFilter>(response);
}

static inline RetainPtr<NEFilterSource> createNEFilterSource(const URL& url, dispatch_queue_t decisionQueue)
{
#if HAVE(MODERN_NE_FILTER_SOURCE)
    UNUSED_PARAM(url);
    return adoptNS([allocNEFilterSourceInstance() initWithDecisionQueue:decisionQueue]);
#else
    UNUSED_PARAM(decisionQueue);
    return adoptNS([allocNEFilterSourceInstance() initWithURL:url direction:NEFilterSourceDirectionInbound socketIdentifier:0]);
#endif
}

NetworkExtensionContentFilter::NetworkExtensionContentFilter(const ResourceResponse& response)
    : m_status { NEFilterSourceStatusNeedsMoreData }
    , m_queue { adoptOSObject(dispatch_queue_create("com.apple.WebCore.NEFilterSourceQueue", DISPATCH_QUEUE_SERIAL)) }
    , m_semaphore { adoptOSObject(dispatch_semaphore_create(0)) }
    , m_originalData { *SharedBuffer::create() }
    , m_neFilterSource { createNEFilterSource(response.url(), m_queue.get()) }
{
    ASSERT([getNEFilterSourceClass() filterRequired]);

#if HAVE(MODERN_NE_FILTER_SOURCE)
    [m_neFilterSource receivedResponse:response.nsURLResponse() decisionHandler:[this](NEFilterSourceStatus status, NSDictionary *decisionInfo) {
        handleDecision(status, replacementDataFromDecisionInfo(decisionInfo));
    }];

    // FIXME: We have to block here since DocumentLoader expects to have a
    // blocked/not blocked answer from the filter immediately after calling
    // addData(). We should find a way to make this asynchronous.
    dispatch_semaphore_wait(m_semaphore.get(), DISPATCH_TIME_FOREVER);
#endif
}

void NetworkExtensionContentFilter::addData(const char* data, int length)
{
    RetainPtr<NSData> copiedData { [NSData dataWithBytes:(void*)data length:length] };

    // FIXME: NEFilterSource doesn't buffer data like WebFilterEvaluator does,
    // so we need to do it ourselves so getReplacementData() can return the
    // original bytes back to the loader. We should find a way to remove this
    // additional copy.
    m_originalData->append((CFDataRef)copiedData.get());

#if HAVE(MODERN_NE_FILTER_SOURCE)
    [m_neFilterSource receivedData:copiedData.get() decisionHandler:[this](NEFilterSourceStatus status, NSDictionary *decisionInfo) {
        handleDecision(status, replacementDataFromDecisionInfo(decisionInfo));
    }];
#else
    [m_neFilterSource addData:copiedData.get() withCompletionQueue:m_queue.get() completionHandler:[this](NEFilterSourceStatus status, NSData *replacementData) {
        ASSERT(!replacementData);
        handleDecision(status, replacementData);
    }];
#endif

    // FIXME: We have to block here since DocumentLoader expects to have a
    // blocked/not blocked answer from the filter immediately after calling
    // addData(). We should find a way to make this asynchronous.
    dispatch_semaphore_wait(m_semaphore.get(), DISPATCH_TIME_FOREVER);
}

void NetworkExtensionContentFilter::finishedAddingData()
{
#if HAVE(MODERN_NE_FILTER_SOURCE)
    [m_neFilterSource finishedLoadingWithDecisionHandler:[this](NEFilterSourceStatus status, NSDictionary *decisionInfo) {
        handleDecision(status, replacementDataFromDecisionInfo(decisionInfo));
    }];
#else
    [m_neFilterSource dataCompleteWithCompletionQueue:m_queue.get() completionHandler:[this](NEFilterSourceStatus status, NSData *replacementData) {
        ASSERT(!replacementData);
        handleDecision(status, replacementData);
    }];
#endif

    // FIXME: We have to block here since DocumentLoader expects to have a
    // blocked/not blocked answer from the filter immediately after calling
    // finishedAddingData(). We should find a way to make this asynchronous.
    dispatch_semaphore_wait(m_semaphore.get(), DISPATCH_TIME_FOREVER);
}

bool NetworkExtensionContentFilter::needsMoreData() const
{
    return m_status == NEFilterSourceStatusNeedsMoreData;
}

bool NetworkExtensionContentFilter::didBlockData() const
{
    return m_status == NEFilterSourceStatusBlock;
}

const char* NetworkExtensionContentFilter::getReplacementData(int& length) const
{
    if (didBlockData()) {
        length = [m_replacementData length];
        return static_cast<const char*>([m_replacementData bytes]);
    }

    length = m_originalData->size();
    return m_originalData->data();
}

ContentFilterUnblockHandler NetworkExtensionContentFilter::unblockHandler() const
{
    return { };
}

void NetworkExtensionContentFilter::handleDecision(NEFilterSourceStatus status, NSData *replacementData)
{
    m_status = status;
    if (status == NEFilterSourceStatusBlock)
        m_replacementData = replacementData;
    dispatch_semaphore_signal(m_semaphore.get());
}

} // namespace WebCore

#endif // HAVE(NETWORK_EXTENSION)
