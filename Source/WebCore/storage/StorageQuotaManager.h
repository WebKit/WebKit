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

#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

class StorageQuotaManager : public ThreadSafeRefCounted<StorageQuotaManager>, public CanMakeWeakPtr<StorageQuotaManager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using UsageGetter = Function<uint64_t()>;
    using QuotaIncreaseRequester = Function<void(uint64_t currentQuota, uint64_t currentUsage, uint64_t requestedIncrease, CompletionHandler<void(Optional<uint64_t>)>&&)>;
    WEBCORE_EXPORT static Ref<StorageQuotaManager> create(uint64_t quota, UsageGetter&&, QuotaIncreaseRequester&&);

    static constexpr uint64_t defaultThirdPartyQuotaFromPerOriginQuota(uint64_t quota) { return quota / 10; }
    static constexpr uint64_t defaultQuota() { return 1000 * MB; }
    static constexpr uint64_t defaultThirdPartyQuota() { return defaultThirdPartyQuotaFromPerOriginQuota(defaultQuota()); }

    enum class Decision { Deny, Grant };
    using RequestCallback = CompletionHandler<void(Decision)>;
    WEBCORE_EXPORT void requestSpaceOnMainThread(uint64_t, RequestCallback&&);
    WEBCORE_EXPORT Decision requestSpaceOnBackgroundThread(uint64_t);

    WEBCORE_EXPORT void resetQuotaUpdatedBasedOnUsageForTesting();
    WEBCORE_EXPORT void resetQuotaForTesting();

private:
    StorageQuotaManager(uint64_t quota, UsageGetter&&, QuotaIncreaseRequester&&);
    bool tryGrantRequest(uint64_t);

    void updateQuotaBasedOnUsage();

    Lock m_quotaCountDownLock;
    uint64_t m_quotaCountDown { 0 };
    uint64_t m_quota { 0 };
    uint64_t m_usage { 0 };

    UsageGetter m_usageGetter;
    QuotaIncreaseRequester m_quotaIncreaseRequester;

    Ref<WorkQueue> m_workQueue;

    bool m_quotaUpdatedBasedOnUsage { false };

    // Test only.
    uint64_t m_initialQuota { 0 };
};

} // namespace WebCore
