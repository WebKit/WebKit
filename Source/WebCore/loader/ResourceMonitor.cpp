/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ResourceMonitor.h"

#include "Document.h"
#include "HTMLIFrameElement.h"
#include "LocalFrame.h"
#include "ResourceMonitorChecker.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

#if ENABLE(CONTENT_EXTENSIONS)

static constexpr size_t networkUsageThreshold = 4 * MB;
static constexpr size_t networkUsageThresholdRandomness = 1303 * KB;

static size_t networkUsageThresholdWithRandomNoise()
{
    return networkUsageThreshold + static_cast<size_t>(cryptographicallyRandomUnitInterval() * networkUsageThresholdRandomness);
}

Ref<ResourceMonitor> ResourceMonitor::create(LocalFrame& frame)
{
    return adoptRef(*new ResourceMonitor(frame));
}

ResourceMonitor::ResourceMonitor(LocalFrame& frame)
    : m_frame(frame)
    , m_networkUsageThreshold { networkUsageThresholdWithRandomNoise() }
{
    if (RefPtr parentMonitor = parentResourceMonitorIfExists())
        m_eligibility = parentMonitor->eligibility();
}

void ResourceMonitor::setEligibility(Eligibility eligibility)
{
    if (m_eligibility == eligibility || m_eligibility == Eligibility::Eligible)
        return;

    m_eligibility = eligibility;

    if (RefPtr parentMonitor = parentResourceMonitorIfExists())
        parentMonitor->setEligibility(eligibility);
    else
        checkNetworkUsageExcessIfNecessary();
}

void ResourceMonitor::setDocumentURL(URL&& url)
{
    RefPtr frame = m_frame.get();
    if (!frame)
        return;

    m_frameURL = WTFMove(url);

    didReceiveResponse(m_frameURL, ContentExtensions::ResourceType::Document);

    if (RefPtr iframe = dynamicDowncast<HTMLIFrameElement>(frame->ownerElement())) {
        if (auto& url = iframe->initiatorSourceURL(); !url.isEmpty())
            didReceiveResponse(url, ContentExtensions::ResourceType::Script);
    }
}

void ResourceMonitor::didReceiveResponse(const URL& url, OptionSet<ContentExtensions::ResourceType> resourceType)
{
    ASSERT(isMainThread());

    if (m_eligibility == Eligibility::Eligible)
        return;

    RefPtr frame = m_frame.get();
    RefPtr page = frame ? frame->mainFrame().page() : nullptr;
    if (!page)
        return;

    ContentExtensions::ResourceLoadInfo info = {
        .resourceURL = url,
        .mainDocumentURL = page->mainFrameURL(),
        .frameURL = m_frameURL,
        .type = resourceType
    };

    ResourceMonitorChecker::singleton().checkEligibility(WTFMove(info), [weakThis = WeakPtr { *this }](Eligibility eligibility) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setEligibility(eligibility);
    });
}

void ResourceMonitor::addNetworkUsage(size_t bytes)
{
    if (m_networkUsageExceed)
        return;

    m_networkUsage += bytes;

    if (RefPtr parentMonitor = parentResourceMonitorIfExists())
        parentMonitor->addNetworkUsage(bytes);
    else
        checkNetworkUsageExcessIfNecessary();
}

void ResourceMonitor::checkNetworkUsageExcessIfNecessary()
{
    ASSERT(!parentResourceMonitorIfExists());
    if (m_eligibility != Eligibility::Eligible || m_networkUsageExceed)
        return;

    if (m_networkUsage.hasOverflowed() || m_networkUsage >= m_networkUsageThreshold) {
        m_networkUsageExceed = true;

        RefPtr frame = m_frame.get();
        if (!frame)
            return;

        // If the frame has sticky user activation, don't do offloading.
        if (RefPtr protectedWindow = frame->window(); protectedWindow && protectedWindow->hasStickyActivation())
            return;

        frame->loader().client().didExceedNetworkUsageThreshold();
    }
}

ResourceMonitor* ResourceMonitor::parentResourceMonitorIfExists() const
{
    RefPtr frame = m_frame.get();
    RefPtr document = frame ? frame->document() : nullptr;
    return document ? document->parentResourceMonitorIfExists() : nullptr;
}

#endif

} // namespace WebCore
