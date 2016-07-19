/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#include "WebPageProxy.h"
#include "WebPaymentCoordinatorMessages.h"
#include "WebPaymentCoordinatorProxyMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/PaymentAuthorizationStatus.h>

namespace WebKit {

static bool isShowingPaymentUI;

WebPaymentCoordinatorProxy::WebPaymentCoordinatorProxy(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_weakPtrFactory(this)
    , m_state(State::Idle)
    , m_merchantValidationState(MerchantValidationState::Idle)
{
    m_webPageProxy.process().addMessageReceiver(Messages::WebPaymentCoordinatorProxy::messageReceiverName(), m_webPageProxy.pageID(), *this);
}

WebPaymentCoordinatorProxy::~WebPaymentCoordinatorProxy()
{
    if (m_state != State::Idle)
        hidePaymentUI();

    m_webPageProxy.process().removeMessageReceiver(Messages::WebPaymentCoordinatorProxy::messageReceiverName(), m_webPageProxy.pageID());
}

void WebPaymentCoordinatorProxy::canMakePayments(bool& reply)
{
    reply = platformCanMakePayments();
}

void WebPaymentCoordinatorProxy::canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, uint64_t requestID)
{
    auto weakThis = m_weakPtrFactory.createWeakPtr();
    platformCanMakePaymentsWithActiveCard(merchantIdentifier, domainName, [weakThis, requestID](bool canMakePayments) {
        auto paymentCoordinatorProxy = weakThis.get();
        if (!paymentCoordinatorProxy)
            return;

        paymentCoordinatorProxy->m_webPageProxy.send(Messages::WebPaymentCoordinator::CanMakePaymentsWithActiveCardReply(requestID, canMakePayments));
    });
}

void WebPaymentCoordinatorProxy::showPaymentUI(const String& originatingURLString, const Vector<String>& linkIconURLStrings, const WebCore::PaymentRequest& paymentRequest, bool& result)
{
    // FIXME: Make this a message check.
    ASSERT(canBegin());

    if (isShowingPaymentUI) {
        result = false;
        return;
    }
    isShowingPaymentUI = true;

    m_state = State::Activating;

    WebCore::URL originatingURL(WebCore::URL(), originatingURLString);

    Vector<WebCore::URL> linkIconURLs;
    for (const auto& linkIconURLString : linkIconURLStrings)
        linkIconURLs.append(WebCore::URL(WebCore::URL(), linkIconURLString));

    platformShowPaymentUI(originatingURL, linkIconURLs, paymentRequest, [this](bool result) {
        ASSERT(m_state == State::Activating);
        if (!result) {
            didCancelPayment();
            return;
        }

        m_state = State::Active;
    });

    result = true;
}

static bool isValidEnum(WebCore::PaymentAuthorizationStatus status)
{
    switch (status) {
    case WebCore::PaymentAuthorizationStatus::Success:
    case WebCore::PaymentAuthorizationStatus::Failure:
    case WebCore::PaymentAuthorizationStatus::InvalidBillingPostalAddress:
    case WebCore::PaymentAuthorizationStatus::InvalidShippingPostalAddress:
    case WebCore::PaymentAuthorizationStatus::InvalidShippingContact:
    case WebCore::PaymentAuthorizationStatus::PINRequired:
    case WebCore::PaymentAuthorizationStatus::PINIncorrect:
    case WebCore::PaymentAuthorizationStatus::PINLockout:
        return true;
    }

    return false;
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

void WebPaymentCoordinatorProxy::completeShippingMethodSelection(uint32_t opaqueStatus, const Optional<WebCore::PaymentRequest::TotalAndLineItems>& newTotalAndLineItems)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    // FIXME: This should be a MESSAGE_CHECK.
    ASSERT(m_state == State::ShippingMethodSelected);

    auto status = static_cast<WebCore::PaymentAuthorizationStatus>(opaqueStatus);

    // FIXME: Make this a message check.
    RELEASE_ASSERT(isValidEnum(status));

    platformCompleteShippingMethodSelection(status, newTotalAndLineItems);
    m_state = State::Active;
}

void WebPaymentCoordinatorProxy::completeShippingContactSelection(uint32_t opaqueStatus, const Vector<WebCore::PaymentRequest::ShippingMethod>& newShippingMethods, const Optional<WebCore::PaymentRequest::TotalAndLineItems>& newTotalAndLineItems)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    // FIXME: This should be a MESSAGE_CHECK.
    ASSERT(m_state == State::ShippingContactSelected);

    auto status = static_cast<WebCore::PaymentAuthorizationStatus>(opaqueStatus);

    // FIXME: Make this a message check.
    RELEASE_ASSERT(isValidEnum(status));

    platformCompleteShippingContactSelection(status, newShippingMethods, newTotalAndLineItems);
    m_state = State::Active;
}

void WebPaymentCoordinatorProxy::completePaymentMethodSelection(const Optional<WebCore::PaymentRequest::TotalAndLineItems>& newTotalAndLineItems)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    // FIXME: This should be a MESSAGE_CHECK.
    ASSERT(m_state == State::PaymentMethodSelected);

    platformCompletePaymentMethodSelection(newTotalAndLineItems);
    m_state = State::Active;
}

void WebPaymentCoordinatorProxy::completePaymentSession(uint32_t opaqueStatus)
{
    // It's possible that the payment has been canceled already.
    if (!canCompletePayment())
        return;

    auto status = static_cast<WebCore::PaymentAuthorizationStatus>(opaqueStatus);

    // FIXME: Make this a message check.
    RELEASE_ASSERT(isValidEnum(status));

    platformCompletePaymentSession(status);

    if (!WebCore::isFinalStateStatus(status)) {
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

void WebPaymentCoordinatorProxy::didCancelPayment()
{
    ASSERT(canCancel());

    m_webPageProxy.send(Messages::WebPaymentCoordinator::DidCancelPayment());

    didReachFinalState();
}

void WebPaymentCoordinatorProxy::validateMerchant(const WebCore::URL& url)
{
    ASSERT(m_merchantValidationState == MerchantValidationState::Idle);

    m_merchantValidationState = MerchantValidationState::Validating;
    m_webPageProxy.send(Messages::WebPaymentCoordinator::ValidateMerchant(url.string()));
}

void WebPaymentCoordinatorProxy::didAuthorizePayment(const WebCore::Payment& payment)
{
    m_state = State::Authorized;
    m_webPageProxy.send(Messages::WebPaymentCoordinator::DidAuthorizePayment(payment));
}

void WebPaymentCoordinatorProxy::didSelectShippingMethod(const WebCore::PaymentRequest::ShippingMethod& shippingMethod)
{
    ASSERT(m_state == State::Active);

    m_state = State::ShippingMethodSelected;
    m_webPageProxy.send(Messages::WebPaymentCoordinator::DidSelectShippingMethod(shippingMethod));
}

void WebPaymentCoordinatorProxy::didSelectShippingContact(const WebCore::PaymentContact& shippingContact)
{
    ASSERT(m_state == State::Active);

    m_state = State::ShippingContactSelected;
    m_webPageProxy.send(Messages::WebPaymentCoordinator::DidSelectShippingContact(shippingContact));
}

void WebPaymentCoordinatorProxy::didSelectPaymentMethod(const WebCore::PaymentMethod& paymentMethod)
{
    ASSERT(m_state == State::Active);

    m_state = State::PaymentMethodSelected;
    m_webPageProxy.send(Messages::WebPaymentCoordinator::DidSelectPaymentMethod(paymentMethod));
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
    m_state = State::Idle;
    m_merchantValidationState = MerchantValidationState::Idle;

    ASSERT(isShowingPaymentUI);
    isShowingPaymentUI = false;
}

}

#endif
