/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#import "ParentalControlsContentFilter.h"

#if HAVE(PARENTAL_CONTROLS)

#import "ContentFilterUnblockHandler.h"
#import "Logging.h"
#import "ResourceResponse.h"
#import "SharedBuffer.h"
#import <objc/runtime.h>
#import <pal/spi/cocoa/WebFilterEvaluatorSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(WebContentAnalysis);
SOFT_LINK_CLASS(WebContentAnalysis, WebFilterEvaluator);

namespace WebCore {

#if PLATFORM(IOS)
ParentalControlsContentFilter::SandboxExtensionState ParentalControlsContentFilter::m_sandboxExtensionState = SandboxExtensionState::NotSet;
#endif

bool ParentalControlsContentFilter::enabled()
{
#if PLATFORM(IOS)
    bool enabled = false;
    switch (m_sandboxExtensionState) {
    case SandboxExtensionState::Consumed:
        enabled = true;
        break;
    case SandboxExtensionState::NotConsumed:
        enabled = false;
        break;
    case SandboxExtensionState::NotSet:
        enabled = [getWebFilterEvaluatorClass() isManagedSession];
        break;
    }
#else
    bool enabled = [getWebFilterEvaluatorClass() isManagedSession];
#endif
    LOG(ContentFiltering, "ParentalControlsContentFilter is %s.\n", enabled ? "enabled" : "not enabled");
    return enabled;
}

UniqueRef<ParentalControlsContentFilter> ParentalControlsContentFilter::create()
{
    return makeUniqueRef<ParentalControlsContentFilter>();
}

static inline bool canHandleResponse(const ResourceResponse& response)
{
#if HAVE(SYSTEM_HTTP_CONTENT_FILTERING)
    return response.url().protocolIs("https"_s);
#else
    return response.url().protocolIsInHTTPFamily();
#endif
}

void ParentalControlsContentFilter::responseReceived(const ResourceResponse& response)
{
    ASSERT(!m_webFilterEvaluator);

    if (!canHandleResponse(response) || !enabled()) {
        m_state = State::Allowed;
        return;
    }

    m_webFilterEvaluator = adoptNS([allocWebFilterEvaluatorInstance() initWithResponse:response.nsURLResponse()]);
    updateFilterState();
}

void ParentalControlsContentFilter::addData(const SharedBuffer& data)
{
    ASSERT(![m_replacementData length]);
    m_replacementData = [m_webFilterEvaluator addData:data.createNSData().get()];
    updateFilterState();
    ASSERT(needsMoreData() || [m_replacementData length]);
}

void ParentalControlsContentFilter::finishedAddingData()
{
    ASSERT(![m_replacementData length]);
    m_replacementData = [m_webFilterEvaluator dataComplete];
    updateFilterState();
}

Ref<FragmentedSharedBuffer> ParentalControlsContentFilter::replacementData() const
{
    ASSERT(didBlockData());
    return SharedBuffer::create(m_replacementData.get());
}

#if ENABLE(CONTENT_FILTERING)
ContentFilterUnblockHandler ParentalControlsContentFilter::unblockHandler() const
{
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    return ContentFilterUnblockHandler { "unblock"_s, m_webFilterEvaluator };
#else
    return { };
#endif
}
#endif

void ParentalControlsContentFilter::updateFilterState()
{
    switch ([m_webFilterEvaluator filterState]) {
    case kWFEStateAllowed:
    case kWFEStateEvaluating:
        m_state = State::Allowed;
        break;
    case kWFEStateBlocked:
        m_state = State::Blocked;
        break;
    case kWFEStateBuffering:
        m_state = State::Filtering;
        break;
    }

#if !LOG_DISABLED
    if (!needsMoreData())
        LOG(ContentFiltering, "ParentalControlsContentFilter stopped buffering with state %d and replacement data length %zu.\n", m_state, [m_replacementData length]);
#endif
}

#if PLATFORM(IOS)
void ParentalControlsContentFilter::setHasConsumedSandboxExtension(bool hasConsumedSandboxExtension)
{
    if (m_sandboxExtensionState == SandboxExtensionState::Consumed)
        return;

    m_sandboxExtensionState = (hasConsumedSandboxExtension ? SandboxExtensionState::Consumed : SandboxExtensionState::NotConsumed);
}
#endif

} // namespace WebCore

#endif // HAVE(PARENTAL_CONTROLS)
