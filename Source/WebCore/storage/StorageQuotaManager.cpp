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

#include "StorageQuotaUser.h"

namespace WebCore {

StorageQuotaManager::~StorageQuotaManager()
{
    while (!m_pendingRequests.isEmpty())
        m_pendingRequests.takeFirst().callback(Decision::Deny);
}

uint64_t StorageQuotaManager::spaceUsage() const
{
    uint64_t usage = 0;
    for (auto& user : m_users)
        usage += user->spaceUsed();
    return usage;
}

void StorageQuotaManager::updateQuotaBasedOnSpaceUsage()
{
    if (!m_quota)
        return;

    auto defaultQuotaStep = m_quota / 10;
    m_quota = std::max(m_quota, defaultQuotaStep * ((spaceUsage() / defaultQuotaStep) + 1));
}

void StorageQuotaManager::addUser(StorageQuotaUser& user)
{
    ASSERT(!m_pendingInitializationUsers.contains(&user));
    ASSERT(!m_users.contains(&user));
    m_pendingInitializationUsers.add(&user);
    user.whenInitialized([this, &user, weakThis = makeWeakPtr(this)]() {
        if (!weakThis)
            return;

        if (m_pendingInitializationUsers.remove(&user))
            m_users.add(&user);

        if (!m_pendingInitializationUsers.isEmpty())
            return;

        updateQuotaBasedOnSpaceUsage();
        processPendingRequests({ }, ShouldDequeueFirstPendingRequest::No);
    });
}

bool StorageQuotaManager::shouldAskForMoreSpace(uint64_t spaceIncrease) const
{
    if (!spaceIncrease)
        return false;

    return spaceUsage() + spaceIncrease > m_quota;
}

void StorageQuotaManager::removeUser(StorageQuotaUser& user)
{
    ASSERT(m_users.contains(&user) || m_pendingInitializationUsers.contains(&user));
    m_users.remove(&user);
    if (m_pendingInitializationUsers.remove(&user) && m_pendingInitializationUsers.isEmpty())
        processPendingRequests({ }, ShouldDequeueFirstPendingRequest::No);
}

void StorageQuotaManager::requestSpace(uint64_t spaceIncrease, RequestCallback&& callback)
{
    if (!m_pendingRequests.isEmpty() || !m_pendingInitializationUsers.isEmpty()) {
        m_pendingRequests.append({ spaceIncrease, WTFMove(callback) });
        return;
    }

    if (shouldAskForMoreSpace(spaceIncrease)) {
        m_pendingRequests.append({ spaceIncrease, WTFMove(callback) });
        askForMoreSpace(spaceIncrease);
        return;
    }
    callback(Decision::Grant);
}

void StorageQuotaManager::askForMoreSpace(uint64_t spaceIncrease)
{
    ASSERT(shouldAskForMoreSpace(spaceIncrease));
    ASSERT(!m_isWaitingForSpaceIncreaseResponse);
    m_isWaitingForSpaceIncreaseResponse = true;
    m_spaceIncreaseRequester(m_quota, spaceUsage(), spaceIncrease, [this, weakThis = makeWeakPtr(*this)](Optional<uint64_t> newQuota) {
        if (!weakThis)
            return;
        m_isWaitingForSpaceIncreaseResponse = false;
        processPendingRequests(newQuota, ShouldDequeueFirstPendingRequest::Yes);
    });
}

void StorageQuotaManager::processPendingRequests(Optional<uint64_t> newQuota, ShouldDequeueFirstPendingRequest shouldDequeueFirstPendingRequest)
{
    if (m_pendingRequests.isEmpty())
        return;

    if (newQuota)
        m_quota = *newQuota;

    if (m_isWaitingForSpaceIncreaseResponse)
        return;

    if (!m_pendingInitializationUsers.isEmpty())
        return;

    if (shouldDequeueFirstPendingRequest == ShouldDequeueFirstPendingRequest::Yes) {
        auto request = m_pendingRequests.takeFirst();
        auto decision = shouldAskForMoreSpace(request.spaceIncrease) ? Decision::Deny : Decision::Grant;
        request.callback(decision);
    }

    while (!m_pendingRequests.isEmpty()) {
        auto& request = m_pendingRequests.first();

        if (shouldAskForMoreSpace(request.spaceIncrease)) {
            uint64_t spaceIncrease = 0;
            for (auto& request : m_pendingRequests)
                spaceIncrease += request.spaceIncrease;
            askForMoreSpace(spaceIncrease);
            return;
        }

        m_pendingRequests.takeFirst().callback(Decision::Grant);
    }
}

} // namespace WebCore
