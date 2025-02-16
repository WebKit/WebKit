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
#include "ResourceMonitorChecker.h"

#include "Logging.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(CONTENT_EXTENSIONS)

#define RESOURCEMONITOR_RELEASE_LOG(fmt, ...) RELEASE_LOG(ResourceLoading, "%p - ResourceMonitorChecker::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

static size_t networkUsageThresholdWithRandomNoise(size_t threshold, double randomness)
{
    return static_cast<size_t>(threshold * (1 + randomness * cryptographicallyRandomUnitInterval()));
}

ResourceMonitorChecker& ResourceMonitorChecker::singleton()
{
    static MainThreadNeverDestroyed<ResourceMonitorChecker> globalChecker;
    return globalChecker;
}

ResourceMonitorChecker::ResourceMonitorChecker()
    : m_workQueue { WorkQueue::create("ResourceMonitorChecker Work Queue"_s) }
    , m_networkUsageThreshold { networkUsageThresholdWithRandomNoise(networkUsageThreshold, networkUsageThresholdRandomness) }
{
    protectedWorkQueue()->dispatchAfter(ruleListPreparationTimeout, [this] mutable {
        if (m_ruleList)
            return;

        RESOURCEMONITOR_RELEASE_LOG("did not receive rule list in time, using default eligibility");

        m_ruleListIsPreparing = false;
        finishPendingQueries([](const auto&) {
            return defaultEligibility;
        });
    });
}

ResourceMonitorChecker::~ResourceMonitorChecker()
{
    finishPendingQueries([](const auto&) {
        return defaultEligibility;
    });
}

void ResourceMonitorChecker::checkEligibility(ContentExtensions::ResourceLoadInfo&& info, CompletionHandler<void(Eligibility)>&& completionHandler)
{
    ASSERT(isMainThread());

    protectedWorkQueue()->dispatch([this, info = crossThreadCopy(WTFMove(info)), completionHandler = WTFMove(completionHandler)] mutable {
        if (!m_ruleList && m_ruleListIsPreparing) {
            m_pendingQueries.append(std::make_pair(WTFMove(info), WTFMove(completionHandler)));
            return;
        }

        Eligibility eligibility = m_ruleList ? checkEligibility(info) : defaultEligibility;

        callOnMainRunLoop([eligibility, completionHandler = WTFMove(completionHandler)] mutable {
            completionHandler(eligibility);
        });
    });
}

ResourceMonitorChecker::Eligibility ResourceMonitorChecker::checkEligibility(const ContentExtensions::ResourceLoadInfo& info)
{
    ASSERT(m_ruleList);

    auto matched = m_ruleList->processContentRuleListsForResourceMonitoring(info.resourceURL, info.mainDocumentURL, info.frameURL, info.type);
    RESOURCEMONITOR_RELEASE_LOG("The url is %" PUBLIC_LOG_STRING ": %" SENSITIVE_LOG_STRING, (matched ? "eligible" : "not eligible"), info.resourceURL.string().utf8().data());

    return matched ? Eligibility::Eligible : Eligibility::NotEligible;
}

void ResourceMonitorChecker::setContentRuleList(ContentExtensions::ContentExtensionsBackend&& backend)
{
    ASSERT(isMainThread());

    protectedWorkQueue()->dispatch([this, backend = crossThreadCopy(WTFMove(backend))] mutable {
        m_ruleList = makeUnique<ContentExtensions::ContentExtensionsBackend>(WTFMove(backend));
        m_ruleListIsPreparing = false;

        RESOURCEMONITOR_RELEASE_LOG("content rule list is set");

        if (!m_pendingQueries.isEmpty()) {
            finishPendingQueries([this](const auto& info) {
                return checkEligibility(info); }
            );
        }
    });
}

void ResourceMonitorChecker::finishPendingQueries(Function<Eligibility(const ContentExtensions::ResourceLoadInfo&)> checker)
{
    RESOURCEMONITOR_RELEASE_LOG("finish pending queries: count %lu", m_pendingQueries.size());

    for (auto& pair : m_pendingQueries) {
        auto& [info, completionHandler] = pair;

        Eligibility eligibility = checker(info);

        callOnMainRunLoop([eligibility, completionHandler = WTFMove(completionHandler)] mutable {
            completionHandler(eligibility);
        });
    }
    m_pendingQueries.clear();
}

void ResourceMonitorChecker::setNetworkUsageThreshold(size_t threshold, double randomness)
{
    m_networkUsageThreshold = networkUsageThresholdWithRandomNoise(threshold, randomness);
}

} // namespace WebCore

#undef RESOURCEMONITOR_RELEASE_LOG

#endif
