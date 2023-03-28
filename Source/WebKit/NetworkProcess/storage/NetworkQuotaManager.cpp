/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "NetworkQuotaManager.h"

#include "NetworkStorageManager.h"

namespace WebKit {

NetworkQuotaManager::NetworkQuotaManager(uint64_t quota, const Vector<std::pair<WebCore::ClientOrigin, uint64_t>>& usages, Function<bool(const WebCore::SecurityOriginData&)>&& function)
    : m_quota(quota)
    , m_evictDataFunction(WTFMove(function))
{
    for (auto [origin, usage] : usages) {
        m_origins.appendOrMoveToLast(origin.topOrigin);
        m_originUsageMap.ensure(origin.topOrigin, [] {
            return HashMap<WebCore::SecurityOriginData, uint64_t> { };
        }).iterator->value.add(origin.clientOrigin, usage);
    }

    performEvictionIfNeeded();
}

void NetworkQuotaManager::originUsageUpdated(const WebCore::ClientOrigin& origin, uint64_t usage)
{
    originVisited(origin);
    auto& originUsage = m_originUsageMap.ensure(origin.topOrigin, [] {
        return HashMap<WebCore::SecurityOriginData, uint64_t> { };
    }).iterator->value;
    auto iterator = originUsage.ensure(origin.clientOrigin, [] {
        return 0;
    }).iterator;
    auto currentUsage = iterator->value;
    iterator->value = usage;
    if (usage <= currentUsage)
        return;

    performEvictionIfNeeded();
}

void NetworkQuotaManager::performEvictionIfNeeded()
{
    if (!m_evictDataFunction)
        return;

    uint64_t totalUsage = 0;
    for (auto origin : m_origins) {
        auto iterator = m_originUsageMap.find(origin);
        if (iterator == m_originUsageMap.end())
            continue;

        for (auto usage : iterator->value.values())
            totalUsage += usage;
    }

    while (totalUsage > m_quota) {
        ASSERT(!m_origins.isEmpty());

        auto topOrigin = m_origins.first();
        if (!m_evictDataFunction(topOrigin))
            break;

        m_origins.removeFirst();
        auto originUsage = m_originUsageMap.take(topOrigin);
        for (auto& [origin, usage] : originUsage)
            totalUsage -= usage;
    }
}

void NetworkQuotaManager::originVisited(const WebCore::ClientOrigin& origin)
{
    m_origins.appendOrMoveToLast(origin.topOrigin);
}

} // namespace WebKit
