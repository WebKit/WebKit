/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEBDRIVER_BIDI)

#include "APIHTTPCookieStore.h"
#include "WebDriverBidiBackendDispatchers.h"

namespace WebKit {

class WebAutomationSession;

class BidiStorageAgent final : public Inspector::BidiStorageBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(BidiStorageAgent);
public:
    BidiStorageAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiStorageAgent() override;

    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::BidiStorage::PartitionKey>> makePartitionKey(RefPtr<JSON::Object> partitionDescriptor);
    Inspector::Protocol::ErrorStringOr<Ref<API::HTTPCookieStore>> cookieStoreForPartition(RefPtr<JSON::Object> partitionDescriptor);
    void getCookies(RefPtr<JSON::Object>&& optionalFilter, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallbackOf<Ref<JSON::ArrayOf<Inspector::Protocol::BidiStorage::PartialCookie>>, Ref<Inspector::Protocol::BidiStorage::PartitionKey>>&&) override;
    void setCookie(Ref<JSON::Object>&& cookie, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallback<Ref<Inspector::Protocol::BidiStorage::PartitionKey>>&&) override;
    void deleteCookies(RefPtr<JSON::Object>&& optionalFilter, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallback<Ref<Inspector::Protocol::BidiStorage::PartitionKey>>&&) override;

private:
    WeakPtr<WebAutomationSession> m_session;
    Ref<Inspector::BidiStorageBackendDispatcher> m_storageDomainDispatcher;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
