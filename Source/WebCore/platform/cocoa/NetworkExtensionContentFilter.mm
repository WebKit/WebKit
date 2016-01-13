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

#import "ContentFilterUnblockHandler.h"
#import "Logging.h"
#import "NEFilterSourceSPI.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "SharedBuffer.h"
#import "SoftLinking.h"
#import "URL.h"
#import <objc/runtime.h>

SOFT_LINK_FRAMEWORK(NetworkExtension);
SOFT_LINK_CLASS(NetworkExtension, NEFilterSource);

#if HAVE(MODERN_NE_FILTER_SOURCE)
static inline NSData *replacementDataFromDecisionInfo(NSDictionary *decisionInfo)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!decisionInfo || [decisionInfo isKindOfClass:[NSDictionary class]]);
    return decisionInfo[NEFilterSourceOptionsPageData];
}
#endif

namespace WebCore {

bool NetworkExtensionContentFilter::enabled()
{
    bool enabled = [getNEFilterSourceClass() filterRequired];
    LOG(ContentFiltering, "NetworkExtensionContentFilter is %s.\n", enabled ? "enabled" : "not enabled");
    return enabled;
}

std::unique_ptr<NetworkExtensionContentFilter> NetworkExtensionContentFilter::create()
{
    return std::make_unique<NetworkExtensionContentFilter>();
}

NetworkExtensionContentFilter::NetworkExtensionContentFilter()
    : m_status { NEFilterSourceStatusNeedsMoreData }
{
}

void NetworkExtensionContentFilter::initialize(const URL* url)
{
    ASSERT(!m_queue);
    ASSERT(!m_semaphore);
    ASSERT(!m_neFilterSource);
    m_queue = adoptOSObject(dispatch_queue_create("WebKit NetworkExtension Filtering", DISPATCH_QUEUE_SERIAL));
    m_semaphore = adoptOSObject(dispatch_semaphore_create(0));
#if HAVE(MODERN_NE_FILTER_SOURCE)
    ASSERT_UNUSED(url, !url);
    m_neFilterSource = adoptNS([allocNEFilterSourceInstance() initWithDecisionQueue:m_queue.get()]);
#else
    ASSERT_ARG(url, url);
    m_neFilterSource = adoptNS([allocNEFilterSourceInstance() initWithURL:*url direction:NEFilterSourceDirectionInbound socketIdentifier:0]);
#endif
}

void NetworkExtensionContentFilter::willSendRequest(ResourceRequest& request, const ResourceResponse& redirectResponse)
{
#if HAVE(MODERN_NE_FILTER_SOURCE)
    ASSERT(!request.isNull());
    if (!request.url().protocolIsInHTTPFamily() || !enabled()) {
        m_status = NEFilterSourceStatusPass;
        return;
    }

    initialize();

    if (!redirectResponse.isNull()) {
        responseReceived(redirectResponse);
        if (!needsMoreData())
            return;
    }

    RetainPtr<NSString> modifiedRequestURLString;
    [m_neFilterSource willSendRequest:request.nsURLRequest(DoNotUpdateHTTPBody) decisionHandler:[this, &modifiedRequestURLString](NEFilterSourceStatus status, NSDictionary *decisionInfo) {
        modifiedRequestURLString = decisionInfo[NEFilterSourceOptionsRedirectURL];
        ASSERT(!modifiedRequestURLString || [modifiedRequestURLString isKindOfClass:[NSString class]]);
        handleDecision(status, replacementDataFromDecisionInfo(decisionInfo));
    }];

    // FIXME: We have to block here since DocumentLoader expects to have a
    // blocked/not blocked answer from the filter immediately after calling
    // addData(). We should find a way to make this asynchronous.
    dispatch_semaphore_wait(m_semaphore.get(), DISPATCH_TIME_FOREVER);

    if (!modifiedRequestURLString)
        return;

    URL modifiedRequestURL { URL(), modifiedRequestURLString.get() };
    if (!modifiedRequestURL.isValid()) {
        LOG(ContentFiltering, "NetworkExtensionContentFilter failed to convert modified URL string %@ to a WebCore::URL.\n", modifiedRequestURLString.get());
        return;
    }

    request.setURL(modifiedRequestURL);
#else
    UNUSED_PARAM(request);
    UNUSED_PARAM(redirectResponse);
#endif
}

void NetworkExtensionContentFilter::responseReceived(const ResourceResponse& response)
{
    if (!response.url().protocolIsInHTTPFamily()) {
        m_status = NEFilterSourceStatusPass;
        return;
    }

#if !HAVE(MODERN_NE_FILTER_SOURCE)
    if (!enabled()) {
        m_status = NEFilterSourceStatusPass;
        return;
    }

    initialize(&response.url());
#else
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

Ref<SharedBuffer> NetworkExtensionContentFilter::replacementData() const
{
    ASSERT(didBlockData());
    return adoptRef(*SharedBuffer::wrapNSData(m_replacementData.get()).leakRef());
}

ContentFilterUnblockHandler NetworkExtensionContentFilter::unblockHandler() const
{
#if HAVE(MODERN_NE_FILTER_SOURCE)
    using DecisionHandlerFunction = ContentFilterUnblockHandler::DecisionHandlerFunction;

    RetainPtr<NEFilterSource> neFilterSource { m_neFilterSource };
    return ContentFilterUnblockHandler {
        ASCIILiteral("nefilter-unblock"), [neFilterSource](DecisionHandlerFunction decisionHandler) {
            [neFilterSource remediateWithDecisionHandler:[decisionHandler](NEFilterSourceStatus status, NSDictionary *) {
                LOG(ContentFiltering, "NEFilterSource %s the unblock request.\n", status == NEFilterSourceStatusPass ? "allowed" : "did not allow");
                decisionHandler(status == NEFilterSourceStatusPass);
            }];
        }
    };
#else
    return { };
#endif
}

void NetworkExtensionContentFilter::handleDecision(NEFilterSourceStatus status, NSData *replacementData)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!replacementData || [replacementData isKindOfClass:[NSData class]]);
    m_status = status;
    if (status == NEFilterSourceStatusBlock)
        m_replacementData = replacementData;
#if !LOG_DISABLED
    if (!needsMoreData())
        LOG(ContentFiltering, "NetworkExtensionContentFilter stopped buffering with status %zd and replacement data length %zu.\n", status, replacementData.length);
#endif
    dispatch_semaphore_signal(m_semaphore.get());
}

} // namespace WebCore

#endif // HAVE(NETWORK_EXTENSION)
