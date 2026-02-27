/*
 * Copyright (C) 2026 Shopify Inc. All rights reserved.
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
#include "FedCMProxy.h"

#if ENABLE(FEDCM)

#include "ExceptionData.h"
#include "FedCMProxyClient.h"
#include "IdentityCredential.h"
#include "IdentityCredentialCreateOptions.h"
#include "IdentityCredentialRequestOptions.h"
#include "MockFedCMConfiguration.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FedCMProxy);

Ref<FedCMProxy> FedCMProxy::create(Ref<FedCMProxyClient>&& client, Page& page)
{
    return adoptRef(*new FedCMProxy(WTF::move(client), page));
}

FedCMProxy::FedCMProxy(Ref<FedCMProxyClient>&& client, Page& page)
    : m_client(WTF::move(client))
    , m_page(page)
{
}

void FedCMProxy::requestCredential(const Document&, CredentialPromise&& promise, IdentityCredentialRequestOptions&& options)
{
    if (m_mockConfig) {
        if (!m_mockConfig->error.isEmpty()) {
            if (m_mockConfig->error == "NetworkError"_s)
                return promise.reject(Exception { ExceptionCode::NetworkError, "Mock network error."_s });
            return promise.reject(Exception { ExceptionCode::NotAllowedError, m_mockConfig->error });
        }

        if (!m_mockConfig->token.isEmpty()) {
            auto credential = IdentityCredential::create(m_mockConfig->token, m_mockConfig->isAutoSelected);
            promise.resolve(credential.ptr());
            return;
        }

        promise.reject(Exception { ExceptionCode::NetworkError, "No token configured in mock."_s });
        return;
    }

    m_client->requestCredential(WTF::move(options), [promise = WTF::move(promise)](Expected<FedCMResponse, ExceptionData>&& result) mutable {
        if (!result) {
            promise.reject(result.error().toException());
            return;
        }
        auto credential = IdentityCredential::create(result->token, result->isAutoSelected);
        promise.resolve(credential.ptr());
    });
}

void FedCMProxy::createCredential(const Document&, CredentialPromise&& promise, IdentityCredentialCreateOptions&& options)
{
    if (m_mockConfig) {
        if (!m_mockConfig->error.isEmpty()) {
            if (m_mockConfig->error == "NetworkError"_s)
                return promise.reject(Exception { ExceptionCode::NetworkError, "Mock network error."_s });
            return promise.reject(Exception { ExceptionCode::NotAllowedError, m_mockConfig->error });
        }

        if (!m_mockConfig->createToken.isEmpty()) {
            auto credential = IdentityCredential::create(m_mockConfig->createToken, false);
            promise.resolve(credential.ptr());
            return;
        }

        promise.reject(Exception { ExceptionCode::NetworkError, "No create token configured in mock."_s });
        return;
    }

    m_client->createCredential(WTF::move(options), [promise = WTF::move(promise)](Expected<FedCMResponse, ExceptionData>&& result) mutable {
        if (!result) {
            promise.reject(result.error().toException());
            return;
        }
        auto credential = IdentityCredential::create(result->token, result->isAutoSelected);
        promise.resolve(credential.ptr());
    });
}

void FedCMProxy::setMockConfiguration(MockFedCMConfiguration&& config)
{
    m_mockConfig = WTF::move(config);
}

void FedCMProxy::clearMockConfiguration()
{
    m_mockConfig = std::nullopt;
}

} // namespace WebCore

#endif // ENABLE(FEDCM)
