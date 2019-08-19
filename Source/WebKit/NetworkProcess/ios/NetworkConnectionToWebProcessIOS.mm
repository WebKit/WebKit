/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkConnectionToWebProcess.h"

#if PLATFORM(IOS_FAMILY)

#import "NetworkSessionCocoa.h"
#import "PaymentAuthorizationController.h"

namespace WebKit {
    
#if ENABLE(APPLE_PAY_REMOTE_UI)

WebPaymentCoordinatorProxy& NetworkConnectionToWebProcess::paymentCoordinator()
{
    if (!m_paymentCoordinator)
        m_paymentCoordinator = makeUnique<WebPaymentCoordinatorProxy>(*this);
    return *m_paymentCoordinator;
}

IPC::Connection* NetworkConnectionToWebProcess::paymentCoordinatorConnection(const WebPaymentCoordinatorProxy&)
{
    return &connection();
}

UIViewController *NetworkConnectionToWebProcess::paymentCoordinatorPresentingViewController(const WebPaymentCoordinatorProxy&)
{
    return nil;
}

const String& NetworkConnectionToWebProcess::paymentCoordinatorBoundInterfaceIdentifier(const WebPaymentCoordinatorProxy&, PAL::SessionID sessionID)
{
    if (auto* session = static_cast<NetworkSessionCocoa*>(m_networkProcess->networkSession(sessionID)))
        return session->boundInterfaceIdentifier();
    return emptyString();
}

const String& NetworkConnectionToWebProcess::paymentCoordinatorCTDataConnectionServiceType(const WebPaymentCoordinatorProxy&, PAL::SessionID sessionID)
{
    if (auto* session = static_cast<NetworkSessionCocoa*>(m_networkProcess->networkSession(sessionID)))
        return session->ctDataConnectionServiceType();
    return emptyString();
}

const String& NetworkConnectionToWebProcess::paymentCoordinatorSourceApplicationBundleIdentifier(const WebPaymentCoordinatorProxy&, PAL::SessionID sessionID)
{
    if (auto* session = static_cast<NetworkSessionCocoa*>(m_networkProcess->networkSession(sessionID)))
        return session->sourceApplicationBundleIdentifier();
    return emptyString();
}

const String& NetworkConnectionToWebProcess::paymentCoordinatorSourceApplicationSecondaryIdentifier(const WebPaymentCoordinatorProxy&, PAL::SessionID sessionID)
{
    if (auto* session = static_cast<NetworkSessionCocoa*>(m_networkProcess->networkSession(sessionID)))
        return session->sourceApplicationSecondaryIdentifier();
    return emptyString();
}

std::unique_ptr<PaymentAuthorizationPresenter> NetworkConnectionToWebProcess::paymentCoordinatorAuthorizationPresenter(WebPaymentCoordinatorProxy& coordinator, PKPaymentRequest *request)
{
    return makeUnique<PaymentAuthorizationController>(coordinator, request);
}

void NetworkConnectionToWebProcess::paymentCoordinatorAddMessageReceiver(WebPaymentCoordinatorProxy&, const IPC::StringReference&, IPC::MessageReceiver&)
{
}

void NetworkConnectionToWebProcess::paymentCoordinatorRemoveMessageReceiver(WebPaymentCoordinatorProxy&, const IPC::StringReference&)
{
}

#endif // ENABLE(APPLE_PAY_REMOTE_UI)

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
