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
#include "WebPaymentCoordinatorProxy.h"

#if ENABLE(APPLE_PAY)

#include "MessageReceiverMap.h"
#include "WebPageProxy.h"
#include "WebPaymentCoordinatorMessages.h"
#include "WebPaymentCoordinatorProxyMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/PaymentAuthorizationStatus.h>

namespace WebKit {

static WeakPtr<WebPaymentCoordinatorProxy>& activePaymentCoordinatorProxy()
{
    static NeverDestroyed<WeakPtr<WebPaymentCoordinatorProxy>> activePaymentCoordinatorProxy;
    return activePaymentCoordinatorProxy.get();
}

WebPaymentCoordinatorProxy::WebPaymentCoordinatorProxy(WebPaymentCoordinatorProxy::Client& client)
    : m_client { client }
{
    m_client.paymentCoordinatorAddMessageReceiver(*this, Messages::WebPaymentCoordinatorProxy::messageReceiverName(), *this);
    finishConstruction(*this);
}

WebPaymentCoordinatorProxy::~WebPaymentCoordinatorProxy()
{
    if (m_state != State::Idle)
        hidePaymentUI();

    m_client.paymentCoordinatorRemoveMessageReceiver(*this, Messages::WebPaymentCoordinatorProxy::messageReceiverName());
}

IPC::Connection* WebPaymentCoordinatorProxy::messageSenderConnection() const
{
    return m_client.paymentCoordinatorConnection(*this);
}

uint64_t WebPaymentCoordinatorProxy::messageSenderDestinationID() const
{
    return *m_destinationID;
}

void WebPaymentCoordinatorProxy::availablePaymentNetworks(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler(platformAvailablePaymentNetworks());
}

void WebPaymentCoordinatorProxy::canMakePayments(CompletionHandler<void(bool)>&& reply)
{
    reply(platformCanMakePayments());
}

void WebPaymentCoordinatorProxy::canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, PAL::SessionID sessionID, CompletionHandler<void(bool)>&& completionHandler)
{
    platformCanMakePaymentsWithActiveCard(merchantIdentifier, domainName, sessionID, WTFMove(completionHandler));
}

void WebPaymentCoordinatorProxy::openPaymentSetup(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&& completionHandler)
{
    platformOpenPaymentSetup(merchantIdentifier, domainName, WTFMove(completionHandler));
}

void WebPaymentCoordinatorProxy::showPaymentUI(uint64_t destinationID, PAL::SessionID sessionID, const String& originatingURLString, const Vector<String>& linkIconURLStrings, const WebCore::ApplePaySessionPaymentRequest& paymentRequest, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto& coordinator = activePaymentCoordinatorProxy())
        coordinator->didCancelPaymentSession();
    activePaymentCoordinatorProxy() = makeWeakPtr(this);

    // FIXME: Make this a message check.
    ASSERT(canBegin());
    ASSERT(!m_destinationID);
    ASSERT(!m_authorizationPresenter);

    m_destinationID = destinationID;
    m_state = State::Activating;

    URL originatingURL(URL(), originatingURLString);

    Vector<URL> linkIconURLs;
    for (const auto& linkIconURLString : linkIconURLStrings)
        linkIconURLs.append(URL(URL(), linkIconURLString));

    platformShowPaymentUI(originatingURL, linkIconURLs, sessionID, paymentRequest, [weakThis = makeWeakPtr(*this)](bool result) {
        if (!weakThis)
            return;

        ASSERT(weakThis->m_state == State::Activating);
        if (!result) {
            weakThis->didCancelPaymentSession();
            return;
        }

        weakThis->m_state = State::Active;
    });

    completionHandler(true);
}

void WebPaymentCoordinatorProxy::completeMerchantValidation(const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    // FIXME: This should be a MESSAGE_CHECK.
    ASSERT(m_merchantValidationState == MerchantValidationState::Validating);

    platformCompleteMerchantValidation(paymentMerchantSession);
    m_merchantValidationState = MerchantValidationState::ValidationComplete;
}

void WebPaymentCoordinatorProxy::completeShippingMethodSelection(const Optional<WebCore::ShippingMethodUpdate>& update)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    // FIXME: This should be a MESSAGE_CHECK.
    ASSERT(m_state == State::ShippingMethodSelected);

    platformCompleteShippingMethodSelection(update);
    m_state = State::Active;
}

void WebPaymentCoordinatorProxy::completeShippingContactSelection(const Optional<WebCore::ShippingContactUpdate>& update)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    // FIXME: This should be a MESSAGE_CHECK.
    ASSERT(m_state == State::ShippingContactSelected);

    platformCompleteShippingContactSelection(update);
    m_state = State::Active;
}

void WebPaymentCoordinatorProxy::completePaymentMethodSelection(const Optional<WebCore::PaymentMethodUpdate>& update)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    // FIXME: This should be a MESSAGE_CHECK.
    ASSERT(m_state == State::PaymentMethodSelected);

    platformCompletePaymentMethodSelection(update);
    m_state = State::Active;
}

void WebPaymentCoordinatorProxy::completePaymentSession(const Optional<WebCore::PaymentAuthorizationResult>& result)
{
    // It's possible that the payment has been canceled already.
    if (!canCompletePayment())
        return;

    bool isFinalStateResult = WebCore::isFinalStateResult(result);

    platformCompletePaymentSession(result);

    if (!isFinalStateResult) {
        m_state = State::Active;
        return;
    }

    didReachFinalState();
}

void WebPaymentCoordinatorProxy::abortPaymentSession()
{
    // It's possible that the payment has been canceled already.
    if (!canAbort())
        return;

    hidePaymentUI();

    didReachFinalState();
}

void WebPaymentCoordinatorProxy::cancelPaymentSession()
{
    if (!canCancel())
        return;

    didCancelPaymentSession();
}

void WebPaymentCoordinatorProxy::didCancelPaymentSession()
{
    ASSERT(canCancel());
    send(Messages::WebPaymentCoordinator::DidCancelPaymentSession());
    hidePaymentUI();
    didReachFinalState();
}

void WebPaymentCoordinatorProxy::presenterWillValidateMerchant(PaymentAuthorizationPresenter&, const URL& url)
{
    ASSERT(m_merchantValidationState == MerchantValidationState::Idle);

    m_merchantValidationState = MerchantValidationState::Validating;
    send(Messages::WebPaymentCoordinator::ValidateMerchant(url.string()));
}

void WebPaymentCoordinatorProxy::presenterDidAuthorizePayment(PaymentAuthorizationPresenter&, const WebCore::Payment& payment)
{
    m_state = State::Authorized;
    send(Messages::WebPaymentCoordinator::DidAuthorizePayment(payment));
}

void WebPaymentCoordinatorProxy::presenterDidFinish(PaymentAuthorizationPresenter&, bool didReachFinalState)
{
    if (!didReachFinalState)
        didCancelPaymentSession();
    else
        hidePaymentUI();
}

void WebPaymentCoordinatorProxy::presenterDidSelectShippingMethod(PaymentAuthorizationPresenter&, const WebCore::ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    ASSERT(m_state == State::Active);

    m_state = State::ShippingMethodSelected;
    send(Messages::WebPaymentCoordinator::DidSelectShippingMethod(shippingMethod));
}

void WebPaymentCoordinatorProxy::presenterDidSelectShippingContact(PaymentAuthorizationPresenter&, const WebCore::PaymentContact& shippingContact)
{
    ASSERT(m_state == State::Active);

    m_state = State::ShippingContactSelected;
    send(Messages::WebPaymentCoordinator::DidSelectShippingContact(shippingContact));
}

void WebPaymentCoordinatorProxy::presenterDidSelectPaymentMethod(PaymentAuthorizationPresenter&, const WebCore::PaymentMethod& paymentMethod)
{
    ASSERT(m_state == State::Active);

    m_state = State::PaymentMethodSelected;
    send(Messages::WebPaymentCoordinator::DidSelectPaymentMethod(paymentMethod));
}

bool WebPaymentCoordinatorProxy::canBegin() const
{
    switch (m_state) {
    case State::Idle:
        return true;

    case State::Activating:
    case State::Active:
    case State::Authorized:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
        return false;
    }
}

bool WebPaymentCoordinatorProxy::canCancel() const
{
    switch (m_state) {
    case State::Activating:
    case State::Active:
    case State::Authorized:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
        return true;

    case State::Idle:
        return false;
    }
}

bool WebPaymentCoordinatorProxy::canCompletePayment() const
{
    switch (m_state) {
    case State::Authorized:
        return true;

    case State::Idle:
    case State::Activating:
    case State::Active:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
        return false;
    }
}

bool WebPaymentCoordinatorProxy::canAbort() const
{
    switch (m_state) {
    case State::Activating:
    case State::Active:
    case State::Authorized:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
        return true;

    case State::Idle:
        return false;
    }
}

void WebPaymentCoordinatorProxy::didReachFinalState()
{
    m_destinationID = WTF::nullopt;
    m_state = State::Idle;
    m_merchantValidationState = MerchantValidationState::Idle;

    ASSERT(activePaymentCoordinatorProxy() == this);
    activePaymentCoordinatorProxy() = nullptr;
}

}

#endif
