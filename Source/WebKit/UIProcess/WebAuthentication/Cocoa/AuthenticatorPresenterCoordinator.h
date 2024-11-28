/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "WebAuthenticationFlags.h"
#include <WebCore/AuthenticatorAssertionResponse.h>
#include <WebCore/AuthenticatorTransport.h>
#include <WebCore/WebAuthenticationConstants.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS ASCAppleIDCredential;
OBJC_CLASS ASCAuthorizationPresentationContext;
OBJC_CLASS ASCAuthorizationPresenter;
OBJC_CLASS ASCLoginChoiceProtocol;
OBJC_CLASS LAContext;
OBJC_CLASS NSError;
OBJC_CLASS WKASCAuthorizationPresenterDelegate;

namespace WebKit {

class AuthenticatorManager;

class AuthenticatorPresenterCoordinator : public RefCountedAndCanMakeWeakPtr<AuthenticatorPresenterCoordinator> {
    WTF_MAKE_TZONE_ALLOCATED(AuthenticatorPresenterCoordinator);
    WTF_MAKE_NONCOPYABLE(AuthenticatorPresenterCoordinator);
public:
    using TransportSet = HashSet<WebCore::AuthenticatorTransport, WTF::IntHash<WebCore::AuthenticatorTransport>, WTF::StrongEnumHashTraits<WebCore::AuthenticatorTransport>>;
    using CredentialRequestHandler = Function<void(ASCAppleIDCredential *, NSError *)>;

    static Ref<AuthenticatorPresenterCoordinator> create(const AuthenticatorManager&, const String& rpId, const TransportSet&, WebCore::ClientDataType, const String& username);
    ~AuthenticatorPresenterCoordinator();

    void updatePresenter(WebAuthenticationStatus);
    void requestPin(uint64_t retries, CompletionHandler<void(const String&)>&&);
    void selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&&, WebAuthenticationSource, CompletionHandler<void(WebCore::AuthenticatorAssertionResponse*)>&&);
    void requestLAContextForUserVerification(CompletionHandler<void(LAContext *)>&&);
    void dimissPresenter(WebAuthenticationResult);

    void setCredentialRequestHandler(CredentialRequestHandler&& handler) { m_credentialRequestHandler = WTFMove(handler); }
    void setLAContext(LAContext *);
    void didSelectAssertionResponse(const String& credentialName, LAContext *);
    void setPin(const String&);

private:
    AuthenticatorPresenterCoordinator(const AuthenticatorManager&, const String& rpId, const TransportSet&, WebCore::ClientDataType, const String& username);

    WeakPtr<AuthenticatorManager> m_manager;
    RetainPtr<ASCAuthorizationPresentationContext> m_context;
    RetainPtr<ASCAuthorizationPresenter> m_presenter;
    RetainPtr<WKASCAuthorizationPresenterDelegate> m_presenterDelegate;
    Function<void()> m_delayedPresentation;
#if HAVE(ASC_AUTH_UI)
    bool m_delayedPresentationNeedsSecurityKey { false };
#endif

    CredentialRequestHandler m_credentialRequestHandler;

    CompletionHandler<void(LAContext *)> m_laContextHandler;
    RetainPtr<LAContext> m_laContext;

    CompletionHandler<void(WebCore::AuthenticatorAssertionResponse*)> m_responseHandler;
    HashMap<String, RefPtr<WebCore::AuthenticatorAssertionResponse>> m_credentials;

    CompletionHandler<void(const String&)> m_pinHandler;
#if HAVE(ASC_AUTH_UI)
    bool m_presentedPIN { false };
#endif
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
