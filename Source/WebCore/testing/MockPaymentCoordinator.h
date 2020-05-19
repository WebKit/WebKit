/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "ApplePayInstallmentConfigurationWebCore.h"
#include "ApplePayLineItem.h"
#include "ApplePaySetupConfiguration.h"
#include "ApplePayShippingMethod.h"
#include "MockPaymentAddress.h"
#include "MockPaymentContactFields.h"
#include "MockPaymentError.h"
#include "PaymentCoordinatorClient.h"
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class Page;
struct ApplePayPaymentMethod;

class MockPaymentCoordinator final : public PaymentCoordinatorClient {
public:
    explicit MockPaymentCoordinator(Page&);

    void setCanMakePayments(bool canMakePayments) { m_canMakePayments = canMakePayments; }
    void setCanMakePaymentsWithActiveCard(bool canMakePaymentsWithActiveCard) { m_canMakePaymentsWithActiveCard = canMakePaymentsWithActiveCard; }
    void setShippingAddress(MockPaymentAddress&& shippingAddress) { m_shippingAddress = WTFMove(shippingAddress); }
    void changeShippingOption(String&& shippingOption);
    void changePaymentMethod(ApplePayPaymentMethod&&);
    void acceptPayment();
    void cancelPayment();
    
#if ENABLE(APPLE_PAY_SETUP)
    void addSetupFeature(ApplePaySetupFeatureState, ApplePaySetupFeatureType, bool supportsInstallments);
    const ApplePaySetupConfiguration& setupConfiguration() const { return m_setupConfiguration; }
#endif

    const ApplePayLineItem& total() const { return m_total; }
    const Vector<ApplePayLineItem>& lineItems() const { return m_lineItems; }
    const Vector<MockPaymentError>& errors() const { return m_errors; }
    const Vector<ApplePayShippingMethod>& shippingMethods() const { return m_shippingMethods; }
    const MockPaymentContactFields& requiredBillingContactFields() const { return m_requiredBillingContactFields; }
    const MockPaymentContactFields& requiredShippingContactFields() const { return m_requiredShippingContactFields; }

    bool supportsUnrestrictedApplePay() const final { return m_supportsUnrestrictedApplePay; }
    void setSupportsUnrestrictedApplePay(bool supports) { m_supportsUnrestrictedApplePay = supports; }
    
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    ApplePayInstallmentConfiguration installmentConfiguration() const { return m_installmentConfiguration; }
#endif

    void ref() const { }
    void deref() const { }

private:
    Optional<String> validatedPaymentNetwork(const String&) final;
    bool canMakePayments() final;
    void canMakePaymentsWithActiveCard(const String&, const String&, CompletionHandler<void(bool)>&&) final;
    void openPaymentSetup(const String&, const String&, CompletionHandler<void(bool)>&&) final;
    bool showPaymentUI(const URL&, const Vector<URL>&, const ApplePaySessionPaymentRequest&) final;
    void completeMerchantValidation(const PaymentMerchantSession&) final;
    void completeShippingMethodSelection(Optional<ShippingMethodUpdate>&&) final;
    void completeShippingContactSelection(Optional<ShippingContactUpdate>&&) final;
    void completePaymentMethodSelection(Optional<PaymentMethodUpdate>&&) final;
    void completePaymentSession(Optional<PaymentAuthorizationResult>&&) final;
    void abortPaymentSession() final;
    void cancelPaymentSession() final;
    void paymentCoordinatorDestroyed() final;

    bool isMockPaymentCoordinator() const final { return true; }

    bool isAlwaysOnLoggingAllowed() const final { return true; }

#if ENABLE(APPLE_PAY_SETUP)
    void getSetupFeatures(const ApplePaySetupConfiguration&, const URL&, CompletionHandler<void(Vector<Ref<ApplePaySetupFeature>>&&)>&&) final;
    void beginApplePaySetup(const ApplePaySetupConfiguration&, const URL&, Vector<RefPtr<ApplePaySetupFeature>>&&, CompletionHandler<void(bool)>&&) final;
#endif

    void updateTotalAndLineItems(const ApplePaySessionPaymentRequest::TotalAndLineItems&);

    Page& m_page;
    bool m_canMakePayments { true };
    bool m_canMakePaymentsWithActiveCard { true };
    ApplePayPaymentContact m_shippingAddress;
    ApplePayLineItem m_total;
    Vector<ApplePayLineItem> m_lineItems;
    Vector<MockPaymentError> m_errors;
    Vector<ApplePayShippingMethod> m_shippingMethods;
    HashSet<String, ASCIICaseInsensitiveHash> m_availablePaymentNetworks;
    MockPaymentContactFields m_requiredBillingContactFields;
    MockPaymentContactFields m_requiredShippingContactFields;
    bool m_supportsUnrestrictedApplePay { true };
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    ApplePayInstallmentConfiguration m_installmentConfiguration;
#endif
#if ENABLE(APPLE_PAY_SETUP)
    ApplePaySetupConfiguration m_setupConfiguration;
    Vector<Ref<ApplePaySetupFeature>> m_setupFeatures;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MockPaymentCoordinator)
    static bool isType(const WebCore::PaymentCoordinatorClient& paymentCoordinatorClient) { return paymentCoordinatorClient.isMockPaymentCoordinator(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(APPLE_PAY)
