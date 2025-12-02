/*
 * Copyright (C) 2015-2024 Apple Inc. All rights reserved.
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
#import <wtf/SoftLinking.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/SpanCocoa.h>

#if HAVE(WEBCONTENTRESTRICTIONS)
#import <WebCore/ParentalControlsURLFilter.h>
#import <wtf/CompletionHandler.h>
#import <pal/cocoa/WebContentRestrictionsSoftLink.h>
#elif HAVE(WEBCONTENTANALYSIS_FRAMEWORK)
#import <pal/spi/cocoa/WebFilterEvaluatorSPI.h>
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(WebContentAnalysis);
SOFT_LINK_CLASS_OPTIONAL(WebContentAnalysis, WebFilterEvaluator);
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ParentalControlsContentFilter);

#if HAVE(WEBCONTENTRESTRICTIONS)

Ref<ParentalControlsURLFilter> ParentalControlsContentFilter::protectedImpl() const
{
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    return ParentalControlsURLFilter::filterWithConfigurationPath(m_webContentRestrictionsConfigurationPath);
#else
    return ParentalControlsURLFilter::singleton();
#endif
}

#endif

bool ParentalControlsContentFilter::enabled() const
{
#if HAVE(WEBCONTENTRESTRICTIONS)
    return protectedImpl()->isEnabled();
#elif HAVE(WEBCONTENTANALYSIS_FRAMEWORK)
    return [getWebFilterEvaluatorClassSingleton() isManagedSession];
#else
    return false;
#endif
}

Ref<ParentalControlsContentFilter> ParentalControlsContentFilter::create(const PlatformContentFilter::FilterParameters& params)
{
    return adoptRef(*new ParentalControlsContentFilter(params));
}

ParentalControlsContentFilter::ParentalControlsContentFilter(const PlatformContentFilter::FilterParameters& params)
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    : m_webContentRestrictionsConfigurationPath(params.webContentRestrictionsConfigurationPath)
#endif
{
    UNUSED_PARAM(params);
}

void ParentalControlsContentFilter::willSendRequest(ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(String&&)>&& completionHandler)
{
    completionHandler({ });
}

static inline bool canHandleResponse(const ResourceResponse& response)
{
    return response.url().protocolIsInHTTPFamily();
}

void ParentalControlsContentFilter::responseReceived(const ResourceResponse& response)
{
    if (!canHandleResponse(response) || !enabled()) {
        m_state = State::Allowed;
        return;
    }

#if HAVE(WEBCONTENTRESTRICTIONS)
    ASSERT(!m_evaluatedURL);
    m_evaluatedURL = response.url();
    m_state = State::Filtering;
    protectedImpl()->isURLAllowed(*m_evaluatedURL, *this);
#elif HAVE(WEBCONTENTANALYSIS_FRAMEWORK)
    ASSERT(!m_webFilterEvaluator);
    m_webFilterEvaluator = adoptNS([allocWebFilterEvaluatorInstance() initWithResponse:response.protectedNSURLResponse().get()]);
#if HAVE(WEBFILTEREVALUATOR_AUDIT_TOKEN)
    if (m_hostProcessAuditToken)
        m_webFilterEvaluator.get().browserAuditToken = *m_hostProcessAuditToken;
#endif
    updateFilterState();
#endif
}

void ParentalControlsContentFilter::addData(const SharedBuffer& data)
{
#if HAVE(WEBCONTENTRESTRICTIONS)
    UNUSED_PARAM(data);
#elif HAVE(WEBCONTENTANALYSIS_FRAMEWORK)
    ASSERT(![m_replacementData length]);
    m_replacementData = [m_webFilterEvaluator addData:data.createNSData().get()];
    updateFilterState();
    ASSERT(needsMoreData() || [m_replacementData length]);
#else
    UNUSED_PARAM(data);
#endif
}

void ParentalControlsContentFilter::finishedAddingData()
{
#if HAVE(WEBCONTENTRESTRICTIONS)
    if (m_state != State::Filtering)
        return;

    // Callers expect state is ready after finishing adding data.
    Locker resultLocker { m_resultLock };
    while (!m_isAllowdByWebContentRestrictions)
        m_resultCondition.wait(m_resultLock);

    m_state = *m_isAllowdByWebContentRestrictions ? State::Allowed : State::Blocked;
    m_replacementData = std::exchange(m_webContentRestrictionsReplacementData, nullptr);

#elif HAVE(WEBCONTENTANALYSIS_FRAMEWORK)
    ASSERT(![m_replacementData length]);
    m_replacementData = [m_webFilterEvaluator dataComplete];
    updateFilterState();
#endif
}

Ref<FragmentedSharedBuffer> ParentalControlsContentFilter::replacementData() const
{
    ASSERT(didBlockData());
    return SharedBuffer::create(m_replacementData.get());
}

#if ENABLE(CONTENT_FILTERING)
ContentFilterUnblockHandler ParentalControlsContentFilter::unblockHandler() const
{
#if HAVE(WEBCONTENTRESTRICTIONS)
    return ContentFilterUnblockHandler { *m_evaluatedURL };
#elif HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    return ContentFilterUnblockHandler { "unblock"_s, m_webFilterEvaluator };
#else
    return { };
#endif
}
#endif

#if HAVE(WEBCONTENTRESTRICTIONS)

void ParentalControlsContentFilter::didReceiveAllowDecisionOnQueue(bool isAllowed, NSData *replacementData)
{
    RELEASE_ASSERT(!isMainThread());

    Locker resultLocker { m_resultLock };
    ASSERT(!m_isAllowdByWebContentRestrictions);
    m_isAllowdByWebContentRestrictions = isAllowed;
    m_webContentRestrictionsReplacementData = replacementData;
    m_resultCondition.notifyOne();
    callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }]() {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->updateFilterStateOnMain();
    });
}

void ParentalControlsContentFilter::updateFilterStateOnMain()
{
    ASSERT(isMainThread());

    if (m_state != State::Filtering)
        return;

    Locker resultLocker { m_resultLock };
    ASSERT(m_isAllowdByWebContentRestrictions);
    m_state = *m_isAllowdByWebContentRestrictions ? State::Allowed : State::Blocked;
    m_replacementData = std::exchange(m_webContentRestrictionsReplacementData, nullptr);
}

#elif HAVE(WEBCONTENTANALYSIS_FRAMEWORK)

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
}

#endif

} // namespace WebCore

#endif // HAVE(PARENTAL_CONTROLS)
