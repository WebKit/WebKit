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
#include "ApplePayShippingContactEditingMode.h"
#include "ApplePayShippingMethod.h"
#include "MockPaymentAddress.h"
#include "MockPaymentContactFields.h"
#include "MockPaymentError.h"
#include "PaymentCoordinatorClient.h"
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/MockPaymentCoordinatorAdditions.h>
#endif

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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    void changeCouponCode(String&& couponCode);
#endif
    void acceptPayment();
    void cancelPayment();

    void addSetupFeature(ApplePaySetupFeatureState, ApplePaySetupFeatureType, bool supportsInstallments);
    const ApplePaySetupConfiguration& setupConfiguration() const { return m_setupConfiguration; }

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

#if ENABLE(APPLE_PAY_COUPON_CODE)
    std::optional<bool> supportsCouponCode() const { return m_supportsCouponCode; }
    const String& couponCode() const { return m_couponCode; }
#endif

#if ENABLE(APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE)
    std::optional<ApplePayShippingContactEditingMode> shippingContactEditingMode() const { return m_shippingContactEditingMode; }
#endif

    void ref() const { }
    void deref() const { }

private:
    std::optional<String> validatedPaymentNetwork(const String&) final;
    bool canMakePayments() final;
    void canMakePaymentsWithActiveCard(const String&, const String&, CompletionHandler<void(bool)>&&) final;
    void openPaymentSetup(const String&, const String&, CompletionHandler<void(bool)>&&) final;
    bool showPaymentUI(const URL&, const Vector<URL>&, const ApplePaySessionPaymentRequest&) final;
    void completeMerchantValidation(const PaymentMerchantSession&) final;
    void completeShippingMethodSelection(std::optional<ApplePayShippingMethodUpdate>&&) final;
    void completeShippingContactSelection(std::optional<ApplePayShippingContactUpdate>&&) final;
    void completePaymentMethodSelection(std::optional<ApplePayPaymentMethodUpdate>&&) final;
#if ENABLE(APPLE_PAY_COUPON_CODE)
    void completeCouponCodeChange(std::optional<ApplePayCouponCodeUpdate>&&) final;
#endif
    void completePaymentSession(std::optional<PaymentAuthorizationResult>&&) final;
    void abortPaymentSession() final;
    void cancelPaymentSession() final;
    void paymentCoordinatorDestroyed() final;

    bool isMockPaymentCoordinator() const final { return true; }

    void getSetupFeatures(const ApplePaySetupConfiguration&, const URL&, CompletionHandler<void(Vector<Ref<ApplePaySetupFeature>>&&)>&&) final;
    void beginApplePaySetup(const ApplePaySetupConfiguration&, const URL&, Vector<RefPtr<ApplePaySetupFeature>>&&, CompletionHandler<void(bool)>&&) final;

    void dispatchIfShowing(Function<void()>&&);

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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    std::optional<bool> m_supportsCouponCode;
    String m_couponCode;
#endif
#if ENABLE(APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE)
    std::optional<ApplePayShippingContactEditingMode> m_shippingContactEditingMode;
#endif
    ApplePaySetupConfiguration m_setupConfiguration;
    Vector<Ref<ApplePaySetupFeature>> m_setupFeatures;

#if defined(MockPaymentCoordinatorAdditions_members)
    MockPaymentCoordinatorAdditions_members
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MockPaymentCoordinator)
    static bool isType(const WebCore::PaymentCoordinatorClient& paymentCoordinatorClient) { return paymentCoordinatorClient.isMockPaymentCoordinator(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(APPLE_PAY)
