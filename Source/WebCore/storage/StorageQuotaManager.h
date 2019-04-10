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

#pragma once

#include "ClientOrigin.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class StorageQuotaUser;

class StorageQuotaManager : public CanMakeWeakPtr<StorageQuotaManager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using SpaceIncreaseRequester = WTF::Function<void(uint64_t quota, uint64_t currentSpace, uint64_t spaceIncrease, CompletionHandler<void(Optional<uint64_t>)>&&)>;
    StorageQuotaManager(uint64_t quota, SpaceIncreaseRequester&& spaceIncreaseRequester)
        : m_quota(quota)
        , m_spaceIncreaseRequester(WTFMove(spaceIncreaseRequester))
    {
    }
    WEBCORE_EXPORT ~StorageQuotaManager();

    static constexpr uint64_t defaultThirdPartyQuotaFromPerOriginQuota(uint64_t quota) { return quota / 10; }

    static constexpr uint64_t defaultQuota() { return 1000 * MB; }
    static constexpr uint64_t defaultThirdPartyQuota() { return defaultThirdPartyQuotaFromPerOriginQuota(defaultQuota()); }

    WEBCORE_EXPORT void addUser(StorageQuotaUser&);
    WEBCORE_EXPORT void removeUser(StorageQuotaUser&);

    enum class Decision { Deny, Grant };
    using RequestCallback = CompletionHandler<void(Decision)>;
    WEBCORE_EXPORT void requestSpace(uint64_t, RequestCallback&&);
    void resetQuota(uint64_t newQuota) { m_quota = newQuota; }

    WEBCORE_EXPORT void updateQuotaBasedOnSpaceUsage();

private:
    uint64_t spaceUsage() const;
    bool shouldAskForMoreSpace(uint64_t spaceIncrease) const;
    void askForMoreSpace(uint64_t spaceIncrease);

    void initializeUsersIfNeeded();
    void askUserToInitialize(StorageQuotaUser&);

    enum class ShouldDequeueFirstPendingRequest { No, Yes };
    void processPendingRequests(Optional<uint64_t>, ShouldDequeueFirstPendingRequest);

    uint64_t m_quota { 0 };

    bool m_isWaitingForSpaceIncreaseResponse { false };
    SpaceIncreaseRequester m_spaceIncreaseRequester;

    enum class WhenInitializedCalled { No, Yes };
    HashMap<StorageQuotaUser*, WhenInitializedCalled> m_pendingInitializationUsers;
    HashSet<const StorageQuotaUser*> m_users;

    struct PendingRequest {
        uint64_t spaceIncrease;
        RequestCallback callback;
    };
    Deque<PendingRequest> m_pendingRequests;
};

} // namespace WebCore
