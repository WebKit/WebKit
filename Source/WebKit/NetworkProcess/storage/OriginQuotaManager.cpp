/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "OriginQuotaManager.h"

#include <wtf/SetForScope.h>

namespace WebKit {

static constexpr double defaultReportedQuotaIncreaseFactor = 2.0;

Ref<OriginQuotaManager> OriginQuotaManager::create(uint64_t quota, uint64_t standardReportedQuota, GetUsageFunction&& getUsageFunction, IncreaseQuotaFunction&& increaseQuotaFunction, NotifySpaceGrantedFunction&& notifySpaceGrantedFunction)
{
    return adoptRef(*new OriginQuotaManager(quota, standardReportedQuota, WTFMove(getUsageFunction), WTFMove(increaseQuotaFunction), WTFMove(notifySpaceGrantedFunction)));
}

OriginQuotaManager::OriginQuotaManager(uint64_t quota, uint64_t standardReportedQuota, GetUsageFunction&& getUsageFunction, IncreaseQuotaFunction&& increaseQuotaFunction, NotifySpaceGrantedFunction&& notifySpaceGrantedFunction)
    : m_quota(quota)
    , m_standardReportedQuota(standardReportedQuota)
    , m_initialQuota(quota)
    , m_getUsageFunction(WTFMove(getUsageFunction))
    , m_increaseQuotaFunction(WTFMove(increaseQuotaFunction))
    , m_notifySpaceGrantedFunction(WTFMove(notifySpaceGrantedFunction))
{
    ASSERT(m_quota);
}

uint64_t OriginQuotaManager::usage()
{
    // Estimated usage that includes granted space.
    if (m_quotaCountdown)
        return m_quota - m_quotaCountdown;

    if (!m_usage)
        m_usage = m_getUsageFunction();

    return std::min(*m_usage, m_quota);
}

void OriginQuotaManager::requestSpace(uint64_t spaceRequested, RequestCallback&& callback)
{
    m_requests.append(OriginQuotaManager::Request { spaceRequested, WTFMove(callback), QuotaIncreaseRequestIdentifier { } });
    handleRequests();
}

void OriginQuotaManager::handleRequests()
{
    if (m_currentRequest)
        return;

    SetForScope isHandlingRequests(m_isHandlingRequests, true);

    while (!m_currentRequest && !m_requests.isEmpty()) {
        m_currentRequest = m_requests.takeFirst();
        if (grantWithCurrentQuota(m_currentRequest->spaceRequested)) {
            m_currentRequest->callback(Decision::Grant);
            m_currentRequest = std::nullopt;
            continue;
        }

        if (!m_increaseQuotaFunction) {
            m_currentRequest->callback(Decision::Deny);
            m_currentRequest = std::nullopt;
            continue;
        }
    
        m_currentRequest->identifier = QuotaIncreaseRequestIdentifier::generate();
        m_increaseQuotaFunction(m_currentRequest->identifier, m_quota, *m_usage, m_currentRequest->spaceRequested);
    }
}

bool OriginQuotaManager::grantWithCurrentQuota(uint64_t spaceRequested)
{
    if (grantFastPath(spaceRequested))
        return true;

    // When OriginQuotaManager is used for the first time, we want to make sure its initial quota is bigger than existing disk usage,
    // based on the assumption that the quota was increased to at least the disk usage under user's permission before.
    bool shouldUpdateQuotaBasedOnUsage = !m_usage;
    m_usage = m_getUsageFunction();
    if (shouldUpdateQuotaBasedOnUsage) {
        auto defaultQuotaStep = m_quota / 10;
        m_quota = std::max(m_quota, defaultQuotaStep * ((*m_usage / defaultQuotaStep) + 1));
    }
    m_quotaCountdown = *m_usage < m_quota ? m_quota - *m_usage : 0;

    return grantFastPath(spaceRequested);
}

void OriginQuotaManager::spaceGranted(uint64_t amount)
{
    if (m_notifySpaceGrantedFunction)
        m_notifySpaceGrantedFunction(amount);
}

bool OriginQuotaManager::grantFastPath(uint64_t spaceRequested)
{
    if (spaceRequested <= m_quotaCountdown) {
        m_quotaCountdown -= spaceRequested;
        spaceGranted(spaceRequested);
        return true;
    }

    return false;
}

void OriginQuotaManager::didIncreaseQuota(QuotaIncreaseRequestIdentifier identifier, std::optional<uint64_t> newQuota)
{
    if (!m_currentRequest || m_currentRequest->identifier != identifier)
        return;

    if (newQuota) {
        m_quota = *newQuota;
        // Recalculate m_quotaCountdown based on usage.
        m_quotaCountdown = 0;
    }

    auto decision = grantWithCurrentQuota(m_currentRequest->spaceRequested) ? Decision::Grant : Decision::Deny;
    m_currentRequest->callback(decision);
    m_currentRequest = std::nullopt;

    if (!m_isHandlingRequests)
        handleRequests();
}

void OriginQuotaManager::resetQuotaUpdatedBasedOnUsageForTesting()
{
    resetQuotaForTesting();
    m_usage = std::nullopt;
}

void OriginQuotaManager::resetQuotaForTesting()
{
    m_quota = m_initialQuota;
    m_quotaCountdown = 0;
}

uint64_t OriginQuotaManager::reportedQuota()
{
    if (!m_standardReportedQuota)
        return m_quota;

    // Standard reported quota is at least double existing usage.
    auto expectedUsage = usage() * defaultReportedQuotaIncreaseFactor;
    while (expectedUsage > m_standardReportedQuota && m_standardReportedQuota < m_quota)
        m_standardReportedQuota *= defaultReportedQuotaIncreaseFactor;

    return std::min(m_quota, m_standardReportedQuota);
}

} // namespace WebKit
