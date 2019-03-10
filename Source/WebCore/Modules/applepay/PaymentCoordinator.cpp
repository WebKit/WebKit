/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#include "PaymentAuthorizationStatus.h"
#include "PaymentCoordinatorClient.h"
#include "PaymentSession.h"
#include <wtf/CompletionHandler.h>
#include <wtf/URL.h>

namespace WebCore {

PaymentCoordinator::PaymentCoordinator(PaymentCoordinatorClient& client)
    : m_client { client }
{
}

PaymentCoordinator::~PaymentCoordinator()
{
    m_client.paymentCoordinatorDestroyed();
}

bool PaymentCoordinator::supportsVersion(unsigned version) const
{
    return m_client.supportsVersion(version);
}

bool PaymentCoordinator::canMakePayments()
{
    return m_client.canMakePayments();
}

void PaymentCoordinator::canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, WTF::Function<void (bool)>&& completionHandler)
{
    m_client.canMakePaymentsWithActiveCard(merchantIdentifier, domainName, WTFMove(completionHandler));
}

void PaymentCoordinator::openPaymentSetup(const String& merchantIdentifier, const String& domainName, WTF::Function<void (bool)>&& completionHandler)
{
    m_client.openPaymentSetup(merchantIdentifier, domainName, WTFMove(completionHandler));
}

bool PaymentCoordinator::beginPaymentSession(PaymentSession& paymentSession, const URL& originatingURL, const Vector<URL>& linkIconURLs, const ApplePaySessionPaymentRequest& paymentRequest)
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

void PaymentCoordinator::completeShippingMethodSelection(Optional<ShippingMethodUpdate>&& update)
{
    ASSERT(m_activeSession);

    m_client.completeShippingMethodSelection(WTFMove(update));
}

void PaymentCoordinator::completeShippingContactSelection(Optional<ShippingContactUpdate>&& update)
{
    ASSERT(m_activeSession);

    m_client.completeShippingContactSelection(WTFMove(update));
}

void PaymentCoordinator::completePaymentMethodSelection(Optional<PaymentMethodUpdate>&& update)
{
    ASSERT(m_activeSession);

    m_client.completePaymentMethodSelection(WTFMove(update));
}

void PaymentCoordinator::completePaymentSession(Optional<PaymentAuthorizationResult>&& result)
{
    ASSERT(m_activeSession);

    bool isFinalState = isFinalStateResult(result);

    m_client.completePaymentSession(WTFMove(result));

    if (!isFinalState)
        return;

    m_activeSession = nullptr;
}

void PaymentCoordinator::abortPaymentSession()
{
    ASSERT(m_activeSession);

    m_client.abortPaymentSession();
    m_activeSession = nullptr;
}

void PaymentCoordinator::cancelPaymentSession()
{
    ASSERT(m_activeSession);

    m_client.cancelPaymentSession();
}

void PaymentCoordinator::validateMerchant(URL&& validationURL)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    m_activeSession->validateMerchant(WTFMove(validationURL));
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

void PaymentCoordinator::didSelectShippingMethod(const ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
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

void PaymentCoordinator::didCancelPaymentSession()
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    m_activeSession->didCancelPaymentSession();
    m_activeSession = nullptr;
}

Optional<String> PaymentCoordinator::validatedPaymentNetwork(unsigned version, const String& paymentNetwork) const
{
    if (version < 2 && equalIgnoringASCIICase(paymentNetwork, "jcb"))
        return WTF::nullopt;

    if (version < 3 && equalIgnoringASCIICase(paymentNetwork, "carteBancaire"))
        return WTF::nullopt;

    return m_client.validatedPaymentNetwork(paymentNetwork);
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
