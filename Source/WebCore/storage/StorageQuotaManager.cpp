/*
 * Copyright (C) 2019 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StorageQuotaManager.h"

#include "Logging.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

Ref<StorageQuotaManager> StorageQuotaManager::create(uint64_t quota, UsageGetter&& usageGetter, QuotaIncreaseRequester&& quotaIncreaseRequester)
{
    return adoptRef(*new StorageQuotaManager(quota, WTFMove(usageGetter), WTFMove(quotaIncreaseRequester)));
}

StorageQuotaManager::StorageQuotaManager(uint64_t quota, UsageGetter&& usageGetter, QuotaIncreaseRequester&& quotaIncreaseRequester)
    : m_quota(quota)
    , m_usageGetter(WTFMove(usageGetter))
    , m_quotaIncreaseRequester(WTFMove(quotaIncreaseRequester))
    , m_workQueue(WorkQueue::create("StorageQuotaManager Background Queue", WorkQueue::Type::Serial))
    , m_initialQuota(quota)
{
}

void StorageQuotaManager::requestSpaceOnMainThread(uint64_t spaceRequested, RequestCallback&& callback)
{
    ASSERT(isMainThread());

    // Fast path.
    if (m_quotaCountDownLock.tryLock()) {
        if (tryGrantRequest(spaceRequested)) {
            m_quotaCountDownLock.unlock();
            callback(Decision::Grant);
            return;
        }
        m_quotaCountDownLock.unlock();
    }

    m_workQueue->dispatch([this, protectedThis = makeRef(*this), spaceRequested, callback = WTFMove(callback)]() mutable {
        auto decision = requestSpaceOnBackgroundThread(spaceRequested);
        callOnMainThread([callback = WTFMove(callback), decision]() mutable {
            callback(decision);
        });
    });
}

StorageQuotaManager::Decision StorageQuotaManager::requestSpaceOnBackgroundThread(uint64_t spaceRequested)
{
    ASSERT(!isMainThread());

    LockHolder locker(m_quotaCountDownLock);

    if (tryGrantRequest(spaceRequested))
        return Decision::Grant;

    m_usage = m_usageGetter();
    updateQuotaBasedOnUsage();
    m_quotaCountDown = m_usage < m_quota ? m_quota - m_usage : 0;
    if (tryGrantRequest(spaceRequested))
        return Decision::Grant;

    // Block this thread until getting decsion for quota increase.
    BinarySemaphore semaphore;
    callOnMainThread([this, protectedThis = makeRef(*this), spaceRequested, &semaphore]() mutable {
        RELEASE_LOG(Storage, "%p - StorageQuotaManager asks for quota increase %" PRIu64, this, spaceRequested);
        m_quotaIncreaseRequester(m_quota, m_usage, spaceRequested, [this, protectedThis = WTFMove(protectedThis), &semaphore](Optional<uint64_t> newQuota) mutable {
            RELEASE_LOG(Storage, "%p - StorageQuotaManager receives quota increase response %" PRIu64, this, newQuota ? *newQuota : 0);
            ASSERT(isMainThread());

            if (newQuota)
                m_quota = *newQuota;

            semaphore.signal();
        });
    });

    semaphore.wait();

    m_usage = m_usageGetter();
    m_quotaCountDown = m_usage < m_quota ? m_quota - m_usage : 0;
    return tryGrantRequest(spaceRequested) ? Decision::Grant : Decision::Deny;
}

bool StorageQuotaManager::tryGrantRequest(uint64_t spaceRequested)
{
    ASSERT(m_quotaCountDownLock.isLocked());
    if (spaceRequested <= m_quotaCountDown) {
        m_quotaCountDown -= spaceRequested;
        return true;
    }
    return false;
}
    
void StorageQuotaManager::updateQuotaBasedOnUsage()
{
    // When StorageQuotaManager is used for the first time, we want to make sure its initial quota is bigger than current disk usage,
    // based on the assumption that the quota was increased to at least the disk usage under user's permission before.
    ASSERT(m_quotaCountDownLock.isLocked());
    if (!m_quotaUpdatedBasedOnUsage) {
        m_quotaUpdatedBasedOnUsage = true;
        auto defaultQuotaStep = m_quota / 10;
        m_quota = std::max(m_quota, defaultQuotaStep * ((m_usage / defaultQuotaStep) + 1));
    }
}

void StorageQuotaManager::resetQuotaUpdatedBasedOnUsageForTesting()
{
    LockHolder locker(m_quotaCountDownLock);
    m_quota = m_initialQuota;
    m_quotaCountDown = 0;
    m_quotaUpdatedBasedOnUsage = false;
}

void StorageQuotaManager::resetQuotaForTesting()
{
    LockHolder locker(m_quotaCountDownLock);
    m_quota = m_initialQuota;
    m_quotaCountDown = 0;
}

} // namespace WebCore
