/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "WebPaymentCoordinator.h"

#if ENABLE(APPLE_PAY)

#include "DataReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPaymentCoordinatorMessages.h"
#include "WebPaymentCoordinatorProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>
#include <WebCore/PaymentCoordinator.h>
#include <wtf/URL.h>

namespace WebKit {

WebPaymentCoordinator::WebPaymentCoordinator(WebPage& webPage)
    : m_webPage(webPage)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebPaymentCoordinator::messageReceiverName(), m_webPage.pageID(), *this);
}

WebPaymentCoordinator::~WebPaymentCoordinator()
{
    WebProcess::singleton().removeMessageReceiver(*this);
}

void WebPaymentCoordinator::networkProcessConnectionClosed()
{
#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (remoteUIEnabled())
        didCancelPaymentSession();
#endif
}

const WebPaymentCoordinator::AvailablePaymentNetworksSet& WebPaymentCoordinator::availablePaymentNetworks()
{
    if (m_availablePaymentNetworks)
        return *m_availablePaymentNetworks;

    m_availablePaymentNetworks = WebPaymentCoordinator::AvailablePaymentNetworksSet();

    Vector<String> availablePaymentNetworks;
    using AvailablePaymentNetworksMessage = Messages::WebPaymentCoordinatorProxy::AvailablePaymentNetworks;
    if (sendSync(AvailablePaymentNetworksMessage(), AvailablePaymentNetworksMessage::Reply(availablePaymentNetworks))) {
        for (auto& network : availablePaymentNetworks)
            m_availablePaymentNetworks->add(network);
    }

    return *m_availablePaymentNetworks;
}

Optional<String> WebPaymentCoordinator::validatedPaymentNetwork(const String& paymentNetwork)
{
    auto& paymentNetworks = availablePaymentNetworks();
    auto result = paymentNetworks.find(paymentNetwork);
    if (result == paymentNetworks.end())
        return WTF::nullopt;
    return *result;
}

bool WebPaymentCoordinator::canMakePayments()
{
    bool canMakePayments;
    if (!sendSync(Messages::WebPaymentCoordinatorProxy::CanMakePayments(), Messages::WebPaymentCoordinatorProxy::CanMakePayments::Reply(canMakePayments)))
        return false;

    return canMakePayments;
}

void WebPaymentCoordinator::canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPaymentCoordinatorProxy::CanMakePaymentsWithActiveCard(merchantIdentifier, domainName, m_webPage.sessionID()), WTFMove(completionHandler));
}

void WebPaymentCoordinator::openPaymentSetup(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPaymentCoordinatorProxy::OpenPaymentSetup(merchantIdentifier, domainName), WTFMove(completionHandler));
}

bool WebPaymentCoordinator::showPaymentUI(const URL& originatingURL, const Vector<URL>& linkIconURLs, const WebCore::ApplePaySessionPaymentRequest& paymentRequest)
{
    Vector<String> linkIconURLStrings;
    for (const auto& linkIconURL : linkIconURLs)
        linkIconURLStrings.append(linkIconURL.string());

    bool result;
    if (!sendSync(Messages::WebPaymentCoordinatorProxy::ShowPaymentUI(m_webPage.pageID(), m_webPage.sessionID(), originatingURL.string(), linkIconURLStrings, paymentRequest), Messages::WebPaymentCoordinatorProxy::ShowPaymentUI::Reply(result)))
        return false;

    return result;
}

void WebPaymentCoordinator::completeMerchantValidation(const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    send(Messages::WebPaymentCoordinatorProxy::CompleteMerchantValidation(paymentMerchantSession));
}

void WebPaymentCoordinator::completeShippingMethodSelection(Optional<WebCore::ShippingMethodUpdate>&& update)
{
    send(Messages::WebPaymentCoordinatorProxy::CompleteShippingMethodSelection(update));
}

void WebPaymentCoordinator::completeShippingContactSelection(Optional<WebCore::ShippingContactUpdate>&& update)
{
    send(Messages::WebPaymentCoordinatorProxy::CompleteShippingContactSelection(update));
}

void WebPaymentCoordinator::completePaymentMethodSelection(Optional<WebCore::PaymentMethodUpdate>&& update)
{
    send(Messages::WebPaymentCoordinatorProxy::CompletePaymentMethodSelection(update));
}

void WebPaymentCoordinator::completePaymentSession(Optional<WebCore::PaymentAuthorizationResult>&& result)
{
    send(Messages::WebPaymentCoordinatorProxy::CompletePaymentSession(result));
}

void WebPaymentCoordinator::abortPaymentSession()
{
    send(Messages::WebPaymentCoordinatorProxy::AbortPaymentSession());
}

void WebPaymentCoordinator::cancelPaymentSession()
{
    send(Messages::WebPaymentCoordinatorProxy::CancelPaymentSession());
}

void WebPaymentCoordinator::paymentCoordinatorDestroyed()
{
    delete this;
}

IPC::Connection* WebPaymentCoordinator::messageSenderConnection() const
{
#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (remoteUIEnabled())
        return &WebProcess::singleton().ensureNetworkProcessConnection().connection();
#endif
    return WebProcess::singleton().parentProcessConnection();
}

uint64_t WebPaymentCoordinator::messageSenderDestinationID() const
{
    return m_webPage.pageID();
}

void WebPaymentCoordinator::validateMerchant(const String& validationURLString)
{
    paymentCoordinator().validateMerchant(URL(URL(), validationURLString));
}

void WebPaymentCoordinator::didAuthorizePayment(const WebCore::Payment& payment)
{
    paymentCoordinator().didAuthorizePayment(payment);
}

void WebPaymentCoordinator::didSelectShippingMethod(const WebCore::ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    paymentCoordinator().didSelectShippingMethod(shippingMethod);
}

void WebPaymentCoordinator::didSelectShippingContact(const WebCore::PaymentContact& shippingContact)
{
    paymentCoordinator().didSelectShippingContact(shippingContact);
}

void WebPaymentCoordinator::didSelectPaymentMethod(const WebCore::PaymentMethod& paymentMethod)
{
    paymentCoordinator().didSelectPaymentMethod(paymentMethod);
}

void WebPaymentCoordinator::didCancelPaymentSession()
{
    paymentCoordinator().didCancelPaymentSession();
}

WebCore::PaymentCoordinator& WebPaymentCoordinator::paymentCoordinator()
{
    return m_webPage.corePage()->paymentCoordinator();
}

#if ENABLE(APPLE_PAY_REMOTE_UI)
bool WebPaymentCoordinator::remoteUIEnabled() const
{
    if (auto page = m_webPage.corePage())
        return page->settings().applePayRemoteUIEnabled();
    return false;
}
#endif

}

#endif
