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

#pragma once

#if ENABLE(APPLE_PAY)

#include "ApplePaySessionPaymentRequest.h"
#include <wtf/Function.h>

namespace WebCore {

class Payment;
class PaymentCoordinatorClient;
class PaymentContact;
class PaymentMerchantSession;
class PaymentMethod;
class PaymentSession;
class URL;
enum class PaymentAuthorizationStatus;
struct PaymentAuthorizationResult;
struct PaymentMethodUpdate;
struct ShippingContactUpdate;
struct ShippingMethodUpdate;

class PaymentCoordinator {
public:
    WEBCORE_EXPORT explicit PaymentCoordinator(PaymentCoordinatorClient&);
    WEBCORE_EXPORT ~PaymentCoordinator();

    PaymentCoordinatorClient& client() { return m_client; }

    bool supportsVersion(unsigned version) const;
    bool canMakePayments();
    void canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, WTF::Function<void (bool)>&& completionHandler);
    void openPaymentSetup(const String& merchantIdentifier, const String& domainName, WTF::Function<void (bool)>&& completionHandler);

    bool hasActiveSession() const { return m_activeSession; }

    bool beginPaymentSession(PaymentSession&, const URL& originatingURL, const Vector<URL>& linkIconURLs, const ApplePaySessionPaymentRequest&);
    void completeMerchantValidation(const PaymentMerchantSession&);
    void completeShippingMethodSelection(std::optional<ShippingMethodUpdate>&&);
    void completeShippingContactSelection(std::optional<ShippingContactUpdate>&&);
    void completePaymentMethodSelection(std::optional<PaymentMethodUpdate>&&);
    void completePaymentSession(std::optional<PaymentAuthorizationResult>&&);
    void abortPaymentSession();
    void cancelPaymentSession();

    WEBCORE_EXPORT void validateMerchant(URL&& validationURL);
    WEBCORE_EXPORT void didAuthorizePayment(const Payment&);
    WEBCORE_EXPORT void didSelectPaymentMethod(const PaymentMethod&);
    WEBCORE_EXPORT void didSelectShippingMethod(const ApplePaySessionPaymentRequest::ShippingMethod&);
    WEBCORE_EXPORT void didSelectShippingContact(const PaymentContact&);
    WEBCORE_EXPORT void didCancelPaymentSession();

    std::optional<String> validatedPaymentNetwork(unsigned version, const String&) const;

private:
    PaymentCoordinatorClient& m_client;

    RefPtr<PaymentSession> m_activeSession;
};

}

#endif
