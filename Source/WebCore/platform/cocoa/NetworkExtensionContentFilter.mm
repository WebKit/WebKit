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

#if ENABLE(CONTENT_FILTERING)

#import "ContentFilterUnblockHandler.h"
#import "Logging.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "RuntimeApplicationChecks.h"
#import "SharedBuffer.h"
#import <objc/runtime.h>
#import <pal/spi/cocoa/NEFilterSourceSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/URL.h>
#import <wtf/threads/BinarySemaphore.h>

static inline NSData *replacementDataFromDecisionInfo(NSDictionary *decisionInfo)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!decisionInfo || [decisionInfo isKindOfClass:[NSDictionary class]]);
    return decisionInfo[NEFilterSourceOptionsPageData];
}

namespace WebCore {

#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
NetworkExtensionContentFilter::SandboxExtensionsState NetworkExtensionContentFilter::m_sandboxExtensionsState = SandboxExtensionsState::NotSet;
#endif

bool NetworkExtensionContentFilter::enabled()
{
#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    return isRequired();
#else
    bool enabled = false;
    switch (m_sandboxExtensionsState) {
    case SandboxExtensionsState::Consumed:
        enabled = true;
        break;
    case SandboxExtensionsState::NotConsumed:
        enabled = false;
        break;
    case SandboxExtensionsState::NotSet:
        enabled = isRequired();
        break;
    }
    LOG(ContentFiltering, "NetworkExtensionContentFilter is %s.\n", enabled ? "enabled" : "not enabled");
    return enabled;
#endif
}

UniqueRef<NetworkExtensionContentFilter> NetworkExtensionContentFilter::create()
{
    return makeUniqueRef<NetworkExtensionContentFilter>();
}

void NetworkExtensionContentFilter::initialize(const URL* url)
{
    ASSERT(!m_queue);
    ASSERT(!m_neFilterSource);
    m_queue = adoptOSObject(dispatch_queue_create("WebKit NetworkExtension Filtering", DISPATCH_QUEUE_SERIAL));
    ASSERT_UNUSED(url, !url);
    m_neFilterSource = adoptNS([[NEFilterSource alloc] initWithDecisionQueue:m_queue.get()]);
    [m_neFilterSource setSourceAppIdentifier:applicationBundleIdentifier()];
    [m_neFilterSource setSourceAppPid:presentingApplicationPID()];
}

void NetworkExtensionContentFilter::willSendRequest(ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT(!request.isNull());
    if (!request.url().protocolIsInHTTPFamily() || !enabled()) {
        m_state = State::Allowed;
        return;
    }

    initialize();

    if (!redirectResponse.isNull()) {
        responseReceived(redirectResponse);
        if (!needsMoreData())
            return;
    }

    BinarySemaphore semaphore;
    RetainPtr<NSString> modifiedRequestURLString;
    [m_neFilterSource willSendRequest:request.nsURLRequest(DoNotUpdateHTTPBody) decisionHandler:[this, &modifiedRequestURLString, &semaphore](NEFilterSourceStatus status, NSDictionary *decisionInfo) {
        modifiedRequestURLString = decisionInfo[NEFilterSourceOptionsRedirectURL];
        ASSERT(!modifiedRequestURLString || [modifiedRequestURLString isKindOfClass:[NSString class]]);
        handleDecision(status, replacementDataFromDecisionInfo(decisionInfo));
        semaphore.signal();
    }];

    // FIXME: We have to block here since DocumentLoader expects to have a
    // blocked/not blocked answer from the filter immediately after calling
    // addData(). We should find a way to make this asynchronous.
    semaphore.wait();

    if (!modifiedRequestURLString)
        return;

    URL modifiedRequestURL { modifiedRequestURLString.get() };
    if (!modifiedRequestURL.isValid()) {
        LOG(ContentFiltering, "NetworkExtensionContentFilter failed to convert modified URL string %@ to a  URL.\n", modifiedRequestURLString.get());
        return;
    }

    request.setURL(modifiedRequestURL);
}

void NetworkExtensionContentFilter::responseReceived(const ResourceResponse& response)
{
    if (!response.url().protocolIsInHTTPFamily()) {
        m_state = State::Allowed;
        return;
    }

    BinarySemaphore semaphore;
    [m_neFilterSource receivedResponse:response.nsURLResponse() decisionHandler:[this, &semaphore](NEFilterSourceStatus status, NSDictionary *decisionInfo) {
        handleDecision(status, replacementDataFromDecisionInfo(decisionInfo));
        semaphore.signal();
    }];

    // FIXME: We have to block here since DocumentLoader expects to have a
    // blocked/not blocked answer from the filter immediately after calling
    // addData(). We should find a way to make this asynchronous.
    semaphore.wait();
}

void NetworkExtensionContentFilter::addData(const SharedBuffer& data)
{
    auto nsData = data.createNSData();

    BinarySemaphore semaphore;
    [m_neFilterSource receivedData:nsData.get() decisionHandler:[this, &semaphore](NEFilterSourceStatus status, NSDictionary *decisionInfo) {
        handleDecision(status, replacementDataFromDecisionInfo(decisionInfo));
        semaphore.signal();
    }];

    // FIXME: We have to block here since DocumentLoader expects to have a
    // blocked/not blocked answer from the filter immediately after calling
    // addData(). We should find a way to make this asynchronous.
    semaphore.wait();
}

void NetworkExtensionContentFilter::finishedAddingData()
{
    BinarySemaphore semaphore;
    [m_neFilterSource finishedLoadingWithDecisionHandler:[this, &semaphore](NEFilterSourceStatus status, NSDictionary *decisionInfo) {
        handleDecision(status, replacementDataFromDecisionInfo(decisionInfo));
        semaphore.signal();
    }];

    // FIXME: We have to block here since DocumentLoader expects to have a
    // blocked/not blocked answer from the filter immediately after calling
    // finishedAddingData(). We should find a way to make this asynchronous.
    semaphore.wait();
}

Ref<FragmentedSharedBuffer> NetworkExtensionContentFilter::replacementData() const
{
    ASSERT(didBlockData());
    return SharedBuffer::create(m_replacementData.get());
}

ContentFilterUnblockHandler NetworkExtensionContentFilter::unblockHandler() const
{
    using DecisionHandlerFunction = ContentFilterUnblockHandler::DecisionHandlerFunction;

    RetainPtr<NEFilterSource> neFilterSource { m_neFilterSource };
    return ContentFilterUnblockHandler {
        "nefilter-unblock"_s, [neFilterSource](DecisionHandlerFunction decisionHandler) {
            [neFilterSource remediateWithDecisionHandler:[decisionHandler](NEFilterSourceStatus status, NSDictionary *) {
                LOG(ContentFiltering, "NEFilterSource %s the unblock request.\n", status == NEFilterSourceStatusPass ? "allowed" : "did not allow");
                decisionHandler(status == NEFilterSourceStatusPass);
            }];
        }
    };
}

void NetworkExtensionContentFilter::handleDecision(NEFilterSourceStatus status, NSData *replacementData)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!replacementData || [replacementData isKindOfClass:[NSData class]]);

    switch (status) {
    case NEFilterSourceStatusPass:
    case NEFilterSourceStatusError:
    case NEFilterSourceStatusWhitelisted:
    case NEFilterSourceStatusBlacklisted:
        m_state = State::Allowed;
        break;
    case NEFilterSourceStatusBlock:
        m_state = State::Blocked;
        break;
    case NEFilterSourceStatusNeedsMoreData:
        m_state = State::Filtering;
        break;
    }

    if (didBlockData())
        m_replacementData = replacementData;
#if !LOG_DISABLED
    if (!needsMoreData())
        LOG(ContentFiltering, "NetworkExtensionContentFilter stopped buffering with status %zd and replacement data length %zu.\n", status, replacementData.length);
#endif
}

bool NetworkExtensionContentFilter::isRequired()
{
    return [NEFilterSource filterRequired];
}

#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
void NetworkExtensionContentFilter::setHasConsumedSandboxExtensions(bool hasConsumedSandboxExtensions)
{
    if (m_sandboxExtensionsState == SandboxExtensionsState::Consumed)
        return;

    m_sandboxExtensionsState = (hasConsumedSandboxExtensions ? SandboxExtensionsState::Consumed : SandboxExtensionsState::NotConsumed);
}
#endif

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
