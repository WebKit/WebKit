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

#pragma once

#include "QuotaIncreaseRequestIdentifier.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebKit {

class QuotaManager : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<QuotaManager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using GetUsageFunction = Function<uint64_t()>;
    using IncreaseQuotaFunction = Function<void(QuotaIncreaseRequestIdentifier, uint64_t currentQuota, uint64_t currentUsage, uint64_t requestedIncrease)>;
    static Ref<QuotaManager> create(uint64_t quota, GetUsageFunction&&, IncreaseQuotaFunction&&);

    enum class Decision : bool { Deny, Grant };
    using RequestCallback = CompletionHandler<void(Decision)>;
    void requestSpace(uint64_t spaceRequested, RequestCallback&&);
    void didIncreaseQuota(QuotaIncreaseRequestIdentifier, std::optional<uint64_t> newQuota);

    void resetQuotaUpdatedBasedOnUsageForTesting();
    void resetQuotaForTesting();

private:
    QuotaManager(uint64_t quota, GetUsageFunction&&, IncreaseQuotaFunction&&);
    void handleRequests();
    bool grantWithCurrentQuota(uint64_t spaceRequested);
    bool grantFastPath(uint64_t spaceRequested);

    struct Request {
        uint64_t spaceRequested;
        RequestCallback callback;
        QuotaIncreaseRequestIdentifier identifier;
    };
    Deque<Request> m_requests;
    std::optional<Request> m_currentRequest;
    bool m_isHandlingRequests { false };
    uint64_t m_quotaCountdown { 0 };
    uint64_t m_quota;
    uint64_t m_initialQuota; // Test only.
    std::optional<uint64_t> m_usage;
    GetUsageFunction m_getUsageFunction;
    IncreaseQuotaFunction m_increaseQuotaFunction;
};

} // namespace WebKit
