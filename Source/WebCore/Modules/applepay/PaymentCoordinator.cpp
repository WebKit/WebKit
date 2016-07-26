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
#include "PaymentCoordinator.h"

#if ENABLE(APPLE_PAY)

#include "ApplePaySession.h"
#include "PaymentAuthorizationStatus.h"
#include "PaymentCoordinatorClient.h"
#include "URL.h"

namespace WebCore {

PaymentCoordinator::PaymentCoordinator(PaymentCoordinatorClient& client)
    : m_client(client)
{
}

PaymentCoordinator::~PaymentCoordinator()
{
    m_client.paymentCoordinatorDestroyed();
}

bool PaymentCoordinator::supportsVersion(unsigned version)
{
    return m_client.supportsVersion(version);
}

bool PaymentCoordinator::canMakePayments()
{
    return m_client.canMakePayments();
}

void PaymentCoordinator::canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, std::function<void (bool)> completionHandler)
{
    m_client.canMakePaymentsWithActiveCard(merchantIdentifier, domainName, WTFMove(completionHandler));
}

bool PaymentCoordinator::beginPaymentSession(ApplePaySession& paymentSession, const URL& originatingURL, const Vector<URL>& linkIconURLs, const PaymentRequest& paymentRequest)
{
    ASSERT(!m_activeSession);

    if (!m_client.showPaymentUI(originatingURL, linkIconURLs, paymentRequest))
        return false;

    m_activeSession = &paymentSession;
    return true;
}

void PaymentCoordinator::completeMerchantValidation(const PaymentMerchantSession& paymentMerchantSession)
{
    ASSERT(m_activeSession);

    m_client.completeMerchantValidation(paymentMerchantSession);
}

void PaymentCoordinator::completeShippingMethodSelection(PaymentAuthorizationStatus status, Optional<PaymentRequest::TotalAndLineItems> newTotalAndItems)
{
    ASSERT(m_activeSession);

    m_client.completeShippingMethodSelection(status, WTFMove(newTotalAndItems));
}

void PaymentCoordinator::completeShippingContactSelection(PaymentAuthorizationStatus status, const Vector<PaymentRequest::ShippingMethod>& newShippingMethods, Optional<PaymentRequest::TotalAndLineItems> newTotalAndItems)
{
    ASSERT(m_activeSession);

    m_client.completeShippingContactSelection(status, newShippingMethods, WTFMove(newTotalAndItems));
}

void PaymentCoordinator::completePaymentMethodSelection(Optional<PaymentRequest::TotalAndLineItems> newTotalAndItems)
{
    ASSERT(m_activeSession);

    m_client.completePaymentMethodSelection(WTFMove(newTotalAndItems));
}

void PaymentCoordinator::completePaymentSession(PaymentAuthorizationStatus status)
{
    ASSERT(m_activeSession);

    m_client.completePaymentSession(status);

    if (!isFinalStateStatus(status))
        return;

    m_activeSession = nullptr;
}

void PaymentCoordinator::abortPaymentSession()
{
    ASSERT(m_activeSession);

    m_client.abortPaymentSession();
    m_activeSession = nullptr;
}

void PaymentCoordinator::validateMerchant(const URL& validationURL)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    m_activeSession->validateMerchant(validationURL);
}

void PaymentCoordinator::didAuthorizePayment(const Payment& payment)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    m_activeSession->didAuthorizePayment(payment);
}

void PaymentCoordinator::didSelectPaymentMethod(const PaymentMethod& paymentMethod)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    m_activeSession->didSelectPaymentMethod(paymentMethod);
}

void PaymentCoordinator::didSelectShippingMethod(const PaymentRequest::ShippingMethod& shippingMethod)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    m_activeSession->didSelectShippingMethod(shippingMethod);
}

void PaymentCoordinator::didSelectShippingContact(const PaymentContact& shippingContact)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    m_activeSession->didSelectShippingContact(shippingContact);
}

void PaymentCoordinator::didCancelPayment()
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    m_activeSession->didCancelPayment();
    m_activeSession = nullptr;
}

}

#endif
