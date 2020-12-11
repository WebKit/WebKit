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

#include "config.h"
#include "AuthenticatorPresenterCoordinator.h"

#if ENABLE(WEB_AUTHN)

#import "AuthenticatorManager.h"
#import "WKASCAuthorizationPresenterDelegate.h"
#import <WebCore/NotImplemented.h>
#import <wtf/BlockPtr.h>

#import "AuthenticationServicesCoreSoftLink.h"

namespace WebKit {
using namespace WebCore;

AuthenticatorPresenterCoordinator::AuthenticatorPresenterCoordinator(const AuthenticatorManager& manager, const String& rpId, const TransportSet& transports, ClientDataType type)
    : m_manager(makeWeakPtr(manager))
{
#if HAVE(ASC_AUTH_UI)
    auto presentationContext = adoptNS([allocASCAuthorizationPresentationContextInstance() initWithRequestContext:nullptr appIdentifier:nullptr]);
    [presentationContext setRelyingPartyIdentifier: rpId];

    switch (type) {
    case ClientDataType::Create:
        if (transports.contains(AuthenticatorTransport::Internal))
            [presentationContext addLoginChoice:adoptNS([allocASCPlatformPublicKeyCredentialLoginChoiceInstance() initRegistrationChoice]).get()];
        if (transports.contains(AuthenticatorTransport::Usb) || transports.contains(AuthenticatorTransport::Nfc))
            [presentationContext addLoginChoice:adoptNS([allocASCSecurityKeyPublicKeyCredentialLoginChoiceInstance() initRegistrationChoice]).get()];
        break;
    case ClientDataType::Get:
        // FIXME(219710): Adopt new UI for the Platform Authenticator getAssertion flow.
        // FIXME(219711): Adopt new UI for the Security Key getAssertion flow.
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_presenterDelegate = [[WKASCAuthorizationPresenterDelegate alloc] initWithCoordinator:*this];
    m_presenter = [allocASCAuthorizationPresenterInstance() init];
    [m_presenter setDelegate:m_presenterDelegate.get()];

    auto completionHandler = makeBlockPtr([manager = m_manager] (id<ASCCredentialProtocol> credential, NSError *error) mutable {
        ASSERT(!RunLoop::isMain());
        if (credential || !error)
            return;

        LOG_ERROR("Couldn't complete the authenticator presentation context: %@", error);
        // This block can be executed in another thread.
        RunLoop::main().dispatch([manager] () mutable {
            if (manager)
                manager->cancel();
        });
    });
    [m_presenter presentAuthorizationWithContext:presentationContext.get() completionHandler:completionHandler.get()];
#endif // HAVE(ASC_AUTH_UI)
}

void AuthenticatorPresenterCoordinator::updatePresenter(WebAuthenticationStatus)
{
    // FIXME(219713): Adopt new UI for the update flow.
}

void AuthenticatorPresenterCoordinator::requestPin(uint64_t, CompletionHandler<void(const String&)>&&)
{
    // FIXME(219712): Adopt new UI for the Client PIN flow.
}

void AuthenticatorPresenterCoordinator::selectAssertionResponse(Vector<Ref<AuthenticatorAssertionResponse>>&&, WebAuthenticationSource, CompletionHandler<void(AuthenticatorAssertionResponse*)>&&)
{
    // FIXME(219710): Adopt new UI for the Platform Authenticator getAssertion flow.
    // FIXME(219711): Adopt new UI for the Security Key getAssertion flow.
}

void AuthenticatorPresenterCoordinator::requestLAContextForUserVerification(CompletionHandler<void(LAContext *)>&& completionHandler)
{
    if (m_laContext) {
        completionHandler(m_laContext.get());
        return;
    }

    m_laContextHandler = WTFMove(completionHandler);
}

void AuthenticatorPresenterCoordinator::dimissPresenter(WebAuthenticationResult result)
{
    if (result == WebAuthenticationResult::Succeeded && m_credentialRequestHandler) {
        m_credentialRequestHandler();
        return;
    }
    // FIXME(219716): Adopt new UI for the dismiss flow.
}

void AuthenticatorPresenterCoordinator::setLAContext(LAContext *context)
{
    if (m_laContextHandler) {
        m_laContextHandler(context);
        return;
    }

    m_laContext = context;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
