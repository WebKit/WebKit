/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include <WebCore/AuthenticatorCoordinatorClient.h>
#include <WebCore/FrameIdentifier.h>

namespace WebKit {

class WebPage;

class WebAuthenticatorCoordinator final : public WebCore::AuthenticatorCoordinatorClient {
public:
    explicit WebAuthenticatorCoordinator(WebPage&);

private:
    // WebCore::AuthenticatorCoordinatorClient
    void makeCredential(const WebCore::Frame&, const WebCore::SecurityOrigin&, const Vector<uint8_t>&, const WebCore::PublicKeyCredentialCreationOptions&, WebCore::RequestCompletionHandler&&) final;
    void getAssertion(const WebCore::Frame&, const WebCore::SecurityOrigin&, const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialRequestOptions&, WebCore::MediationRequirement, const std::pair<WebAuthn::Scope, std::optional<WebCore::SecurityOriginData>>&, WebCore::RequestCompletionHandler&&) final;
    void isConditionalMediationAvailable(const WebCore::SecurityOrigin&, WebCore::QueryCompletionHandler&&) final;
    void isUserVerifyingPlatformAuthenticatorAvailable(const WebCore::SecurityOrigin&, WebCore::QueryCompletionHandler&&) final;
    void resetUserGestureRequirement() final { m_requireUserGesture = false; }
    void cancel() final;

    bool processingUserGesture(const WebCore::Frame&, const WebCore::FrameIdentifier&);

    WebPage& m_webPage;
    bool m_requireUserGesture { false };
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
