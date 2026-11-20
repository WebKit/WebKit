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

namespace WebCore {
struct Cookie;
}

namespace WebKit {

class WebAutomationSession;
class WebsiteDataStore;

// The storage partition that a storage.PartitionDescriptor resolves to.
// https://w3c.github.io/webdriver-bidi/#expand-a-storage-partition-spec
struct BidiStoragePartition {
    // The user context's data store. Its cookie store lives in the network process, so cookies
    // are read and written there for every frame, whichever web process the frame is in.
    Ref<WebsiteDataStore> dataStore;
    Ref<Inspector::Protocol::BidiStorage::PartitionKey> partitionKey;
    // The host of a storageKey descriptor's sourceOrigin, or null. WebKit keeps one cookie store
    // per user context, so a sourceOrigin limits the partition to the cookies that domain-match it.
    String sourceOriginHost;

    Ref<API::HTTPCookieStore> cookieStore() const;
    bool containsCookie(const WebCore::Cookie&) const;
};

// A deserialized storage.CookieFilter. An empty filter matches every cookie.
// https://w3c.github.io/webdriver-bidi/#deserialize-filter
struct BidiCookieFilter {
    static Inspector::Protocol::ErrorStringOr<BidiCookieFilter> fromProtocol(const RefPtr<JSON::Object>&);

    // https://w3c.github.io/webdriver-bidi/#match-cookie
    bool matches(const WebCore::Cookie&) const;

    std::optional<String> name;
    std::optional<Vector<uint8_t>> value;
    std::optional<String> domain;
    std::optional<String> path;
    std::optional<uint64_t> size;
    std::optional<bool> httpOnly;
    std::optional<bool> secure;
    std::optional<Inspector::Protocol::BidiStorage::CookieSameSitePolicy> sameSite;
    std::optional<uint64_t> expiry;
};

class BidiStorageAgent final : public Inspector::BidiStorageBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(BidiStorageAgent);
public:
    BidiStorageAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiStorageAgent() override;

    // https://w3c.github.io/webdriver-bidi/#expand-a-storage-partition-spec and #get-the-cookie-store.
    Inspector::Protocol::ErrorStringOr<BidiStoragePartition> resolveStoragePartition(const RefPtr<JSON::Object>& partitionSpec);

    // https://w3c.github.io/webdriver-bidi/#serialize-cookie
    static Ref<Inspector::Protocol::BidiStorage::Cookie> serializeCookie(const WebCore::Cookie&);

    void getCookies(RefPtr<JSON::Object>&& optionalFilter, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallbackOf<Ref<JSON::ArrayOf<Inspector::Protocol::BidiStorage::Cookie>>, Ref<Inspector::Protocol::BidiStorage::PartitionKey>>&&) override;
    void setCookie(Ref<JSON::Object>&& cookie, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallback<Ref<Inspector::Protocol::BidiStorage::PartitionKey>>&&) override;
    void deleteCookies(RefPtr<JSON::Object>&& optionalFilter, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallback<Ref<Inspector::Protocol::BidiStorage::PartitionKey>>&&) override;

private:
    WeakPtr<WebAutomationSession> m_session;
    const Ref<Inspector::BidiStorageBackendDispatcher> m_storageDomainDispatcher;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
