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
#include "QuotaManager.h"

#include <wtf/SetForScope.h>

namespace WebKit {

Ref<QuotaManager> QuotaManager::create(uint64_t quota, GetUsageFunction&& getUsageFunction, IncreaseQuotaFunction&& increaseQuotaFunction)
{
    return adoptRef(*new QuotaManager(quota, WTFMove(getUsageFunction), WTFMove(increaseQuotaFunction)));
}

QuotaManager::QuotaManager(uint64_t quota, GetUsageFunction&& getUsageFunction, IncreaseQuotaFunction&& increaseQuotaFunction)
    : m_quota(quota)
    , m_initialQuota(quota)
    , m_getUsageFunction(WTFMove(getUsageFunction))
    , m_increaseQuotaFunction(WTFMove(increaseQuotaFunction))
{
    ASSERT(m_quota);
}

uint64_t QuotaManager::usage()
{
    return m_getUsageFunction();
}

void QuotaManager::requestSpace(uint64_t spaceRequested, RequestCallback&& callback)
{
    m_requests.append(QuotaManager::Request { spaceRequested, WTFMove(callback), QuotaIncreaseRequestIdentifier { } });
    handleRequests();
}

void QuotaManager::handleRequests()
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

        m_currentRequest->identifier = QuotaIncreaseRequestIdentifier::generateThreadSafe();
        m_increaseQuotaFunction(m_currentRequest->identifier, m_quota, *m_usage, m_currentRequest->spaceRequested);
    }
}

bool QuotaManager::grantWithCurrentQuota(uint64_t spaceRequested)
{
    if (grantFastPath(spaceRequested))
        return true;

    // When QuotaManager is used for the first time, we want to make sure its initial quota is bigger than existing disk usage,
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

bool QuotaManager::grantFastPath(uint64_t spaceRequested)
{
    if (spaceRequested <= m_quotaCountdown) {
        m_quotaCountdown -= spaceRequested;
        return true;
    }

    return false;
}

void QuotaManager::didIncreaseQuota(QuotaIncreaseRequestIdentifier identifier, std::optional<uint64_t> newQuota)
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

void QuotaManager::resetQuotaUpdatedBasedOnUsageForTesting()
{
    resetQuotaForTesting();
    m_usage = std::nullopt;
}

void QuotaManager::resetQuotaForTesting()
{
    m_quota = m_initialQuota;
    m_quotaCountdown = 0;
}

} // namespace WebKit
