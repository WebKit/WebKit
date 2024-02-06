/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include <WebCore/CredentialRequestOptions.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/MediationRequirement.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

#if HAVE(WEB_AUTHN_AS_MODERN)
OBJC_CLASS _WKASDelegate;
OBJC_CLASS NSArray;
OBJC_CLASS NSString;
OBJC_CLASS ASAuthorization;
OBJC_CLASS ASAuthorizationController;
typedef NSString *ASAuthorizationPublicKeyCredentialUserVerificationPreference;
typedef NSString *ASAuthorizationPublicKeyCredentialAttestationKind;
#endif

namespace WebCore {
enum class AuthenticatorAttachment : uint8_t;
struct ExceptionData;
struct PublicKeyCredentialCreationOptions;
struct AuthenticatorResponseData;
struct PublicKeyCredentialRequestOptions;
class SecurityOriginData;
}

#if HAVE(UNIFIED_ASC_AUTH_UI)
OBJC_CLASS ASCAuthorizationRemotePresenter;
OBJC_CLASS ASCCredentialRequestContext;
OBJC_CLASS ASCAgentProxy;
#endif

namespace WebKit {

class WebPageProxy;

struct FrameInfoData;
struct WebAuthenticationRequestData;

using CapabilitiesCompletionHandler = CompletionHandler<void(Vector<KeyValuePair<String, bool>>&&)>;
using RequestCompletionHandler = CompletionHandler<void(const WebCore::AuthenticatorResponseData&, WebCore::AuthenticatorAttachment, const WebCore::ExceptionData&)>;

class WebAuthenticatorCoordinatorProxy : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebAuthenticatorCoordinatorProxy);
public:
    explicit WebAuthenticatorCoordinatorProxy(WebPageProxy&);
    ~WebAuthenticatorCoordinatorProxy();

#if HAVE(WEB_AUTHN_AS_MODERN)
    void pauseConditionalAssertion();
    void unpauseConditionalAssertion();
#endif

private:
    using QueryCompletionHandler = CompletionHandler<void(bool)>;

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Receivers.
    void makeCredential(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::PublicKeyCredentialCreationOptions&&, RequestCompletionHandler&&);
    void getAssertion(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::PublicKeyCredentialRequestOptions&&, WebCore::MediationRequirement, std::optional<WebCore::SecurityOriginData>, RequestCompletionHandler&&);
    void isUserVerifyingPlatformAuthenticatorAvailable(const WebCore::SecurityOriginData&, QueryCompletionHandler&&);
    void isConditionalMediationAvailable(const WebCore::SecurityOriginData&, QueryCompletionHandler&&);
    void getClientCapabilities(const WebCore::SecurityOriginData&, CapabilitiesCompletionHandler&&);
    void cancel(CompletionHandler<void()>&&);

    void handleRequest(WebAuthenticationRequestData&&, RequestCompletionHandler&&);

    WebPageProxy& m_webPageProxy;

#if HAVE(UNIFIED_ASC_AUTH_UI)
    bool isASCAvailable();

#if HAVE(WEB_AUTHN_AS_MODERN)
    RetainPtr<ASAuthorizationController> constructASController(const WebAuthenticationRequestData&);
    RetainPtr<NSArray> requestsForRegisteration(const WebCore::PublicKeyCredentialCreationOptions&, const WebCore::SecurityOriginData& callerOrigin);
    RetainPtr<NSArray> requestsForAssertion(const WebCore::PublicKeyCredentialRequestOptions&, const WebCore::SecurityOriginData& callerOrigin, const std::optional<WebCore::SecurityOriginData>& parentOrigin);
#endif

    void performRequest(WebAuthenticationRequestData&&, RequestCompletionHandler&&);

    RetainPtr<ASCCredentialRequestContext> contextForRequest(WebAuthenticationRequestData&&);
    void performRequestLegacy(RetainPtr<ASCCredentialRequestContext>, RequestCompletionHandler&&);

#if HAVE(WEB_AUTHN_AS_MODERN)
    RequestCompletionHandler m_completionHandler;
    RetainPtr<_WKASDelegate> m_delegate;
    RetainPtr<ASAuthorizationController> m_controller;
    bool m_paused { false };
    bool m_isConditionalAssertion { false };
#endif

    RetainPtr<ASCAuthorizationRemotePresenter> m_presenter;
    RetainPtr<ASCAgentProxy> m_proxy;
    CompletionHandler<void()> m_cancelHandler;
#endif // HAVE(UNIFIED_ASC_AUTH_UI)
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
