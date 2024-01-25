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

#include "config.h"
#include "MockPaymentCoordinator.h"

#if ENABLE(APPLE_PAY)

#include "ApplePayCouponCodeUpdate.h"
#include "ApplePayLaterAvailability.h"
#include "ApplePayPaymentAuthorizationResult.h"
#include "ApplePayPaymentMethodUpdate.h"
#include "ApplePaySessionPaymentRequest.h"
#include "ApplePayShippingContactEditingMode.h"
#include "ApplePayShippingContactUpdate.h"
#include "ApplePayShippingMethodUpdate.h"
#include "MockApplePaySetupFeature.h"
#include "MockPayment.h"
#include "MockPaymentContact.h"
#include "MockPaymentMethod.h"
#include "Page.h"
#include "PaymentCoordinator.h"
#include "PaymentInstallmentConfigurationWebCore.h"
#include "PaymentSessionError.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>

namespace WebCore {

MockPaymentCoordinator::MockPaymentCoordinator(Page& page)
    : m_page { page }
{
    m_availablePaymentNetworks.add("amex"_s);
    m_availablePaymentNetworks.add("carteBancaire"_s);
    m_availablePaymentNetworks.add("chinaUnionPay"_s);
    m_availablePaymentNetworks.add("discover"_s);
    m_availablePaymentNetworks.add("interac"_s);
    m_availablePaymentNetworks.add("jcb"_s);
    m_availablePaymentNetworks.add("masterCard"_s);
    m_availablePaymentNetworks.add("privateLabel"_s);
    m_availablePaymentNetworks.add("visa"_s);
}

std::optional<String> MockPaymentCoordinator::validatedPaymentNetwork(const String& paymentNetwork) const
{
    auto result = m_availablePaymentNetworks.find(paymentNetwork);
    if (result == m_availablePaymentNetworks.end())
        return std::nullopt;
    return *result;
}

bool MockPaymentCoordinator::canMakePayments()
{
    return m_canMakePayments;
}

void MockPaymentCoordinator::canMakePaymentsWithActiveCard(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler)
{
    RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), canMakePaymentsWithActiveCard = m_canMakePaymentsWithActiveCard]() mutable {
        completionHandler(canMakePaymentsWithActiveCard);
    });
}

void MockPaymentCoordinator::openPaymentSetup(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler)
{
    RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(true);
    });
}

static uint64_t showCount;
static uint64_t hideCount;

MockPaymentCoordinator::~MockPaymentCoordinator()
{
    ASSERT(showCount == hideCount);
}

void MockPaymentCoordinator::dispatchIfShowing(Function<void()>&& function)
{
    if (showCount <= hideCount)
        return;

    RunLoop::main().dispatch([currentShowCount = showCount, function = WTFMove(function)]() {
        if (showCount > hideCount && showCount == currentShowCount)
            function();
    });
}

bool MockPaymentCoordinator::showPaymentUI(const URL&, const Vector<URL>&, const ApplePaySessionPaymentRequest& request)
{
    if (request.shippingContact().pkContact().get())
        m_shippingAddress = request.shippingContact().toApplePayPaymentContact(request.version());
    m_supportedCountries = request.supportedCountries();
    m_shippingMethods = request.shippingMethods();
    m_requiredBillingContactFields = request.requiredBillingContactFields();
    m_requiredShippingContactFields = request.requiredShippingContactFields();
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    if (auto& configuration = request.installmentConfiguration().applePayInstallmentConfiguration())
        m_installmentConfiguration = *configuration;
#endif
#if ENABLE(APPLE_PAY_COUPON_CODE)
    m_supportsCouponCode = request.supportsCouponCode();
    m_couponCode = request.couponCode();
#endif
#if ENABLE(APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE)
    m_shippingContactEditingMode = request.shippingContactEditingMode();
#endif
#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    m_recurringPaymentRequest = request.recurringPaymentRequest();
#endif
#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    m_automaticReloadPaymentRequest = request.automaticReloadPaymentRequest();
#endif
#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    m_multiTokenContexts = request.multiTokenContexts();
#endif
#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
    m_deferredPaymentRequest = request.deferredPaymentRequest();
#endif
#if ENABLE(APPLE_PAY_LATER_AVAILABILITY)
    m_applePayLaterAvailability = request.applePayLaterAvailability();
#endif

    ASSERT(showCount == hideCount);
    ++showCount;
    dispatchIfShowing([page = &m_page]() {
        page->paymentCoordinator().validateMerchant(URL { "https://webkit.org/"_str });
    });
    return true;
}

void MockPaymentCoordinator::completeMerchantValidation(const PaymentMerchantSession&)
{
    dispatchIfShowing([page = &m_page, shippingAddress = m_shippingAddress]() mutable {
        page->paymentCoordinator().didSelectShippingContact(MockPaymentContact { WTFMove(shippingAddress) });
    });
}

void MockPaymentCoordinator::completeShippingMethodSelection(std::optional<ApplePayShippingMethodUpdate>&& shippingMethodUpdate)
{
    if (!shippingMethodUpdate)
        return;

    m_total = WTFMove(shippingMethodUpdate->newTotal);
    m_lineItems = WTFMove(shippingMethodUpdate->newLineItems);
#if ENABLE(APPLE_PAY_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_LINE_ITEMS)
    m_shippingMethods = WTFMove(shippingMethodUpdate->newShippingMethods);
#endif
#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    m_recurringPaymentRequest = WTFMove(shippingMethodUpdate->newRecurringPaymentRequest);
#endif
#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    m_automaticReloadPaymentRequest = WTFMove(shippingMethodUpdate->newAutomaticReloadPaymentRequest);
#endif
#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    m_multiTokenContexts = WTFMove(shippingMethodUpdate->newMultiTokenContexts);
#endif
#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
    m_deferredPaymentRequest = WTFMove(shippingMethodUpdate->newDeferredPaymentRequest);
#endif
}

static Vector<MockPaymentError> convert(Vector<RefPtr<ApplePayError>>&& errors)
{
    Vector<MockPaymentError> result;
    for (auto& error : errors) {
        if (error)
            result.append({ error->code(), error->message(), error->contactField() });
    }
    return result;
}

void MockPaymentCoordinator::completeShippingContactSelection(std::optional<ApplePayShippingContactUpdate>&& shippingContactUpdate)
{
    if (!shippingContactUpdate)
        return;

    m_total = WTFMove(shippingContactUpdate->newTotal);
    m_lineItems = WTFMove(shippingContactUpdate->newLineItems);
    m_shippingMethods = WTFMove(shippingContactUpdate->newShippingMethods);
    m_errors = convert(WTFMove(shippingContactUpdate->errors));
#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    m_recurringPaymentRequest = WTFMove(shippingContactUpdate->newRecurringPaymentRequest);
#endif
#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    m_automaticReloadPaymentRequest = WTFMove(shippingContactUpdate->newAutomaticReloadPaymentRequest);
#endif
#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    m_multiTokenContexts = WTFMove(shippingContactUpdate->newMultiTokenContexts);
#endif
#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
    m_deferredPaymentRequest = WTFMove(shippingContactUpdate->newDeferredPaymentRequest);
#endif
}

void MockPaymentCoordinator::completePaymentMethodSelection(std::optional<ApplePayPaymentMethodUpdate>&& paymentMethodUpdate)
{
    if (!paymentMethodUpdate)
        return;

    m_total = WTFMove(paymentMethodUpdate->newTotal);
    m_lineItems = WTFMove(paymentMethodUpdate->newLineItems);
#if ENABLE(APPLE_PAY_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_LINE_ITEMS)
    m_shippingMethods = WTFMove(paymentMethodUpdate->newShippingMethods);
    m_errors = convert(WTFMove(paymentMethodUpdate->errors));
#endif
#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    m_recurringPaymentRequest = WTFMove(paymentMethodUpdate->newRecurringPaymentRequest);
#endif
#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    m_automaticReloadPaymentRequest = WTFMove(paymentMethodUpdate->newAutomaticReloadPaymentRequest);
#endif
#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    m_multiTokenContexts = WTFMove(paymentMethodUpdate->newMultiTokenContexts);
#endif
#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
    m_deferredPaymentRequest = WTFMove(paymentMethodUpdate->newDeferredPaymentRequest);
#endif
}

#if ENABLE(APPLE_PAY_COUPON_CODE)

void MockPaymentCoordinator::completeCouponCodeChange(std::optional<ApplePayCouponCodeUpdate>&& couponCodeUpdate)
{
    if (!couponCodeUpdate)
        return;

    m_total = WTFMove(couponCodeUpdate->newTotal);
    m_lineItems = WTFMove(couponCodeUpdate->newLineItems);
    m_shippingMethods = WTFMove(couponCodeUpdate->newShippingMethods);
    m_errors = convert(WTFMove(couponCodeUpdate->errors));
#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    m_recurringPaymentRequest = WTFMove(couponCodeUpdate->newRecurringPaymentRequest);
#endif
#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    m_automaticReloadPaymentRequest = WTFMove(couponCodeUpdate->newAutomaticReloadPaymentRequest);
#endif
#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    m_multiTokenContexts = WTFMove(couponCodeUpdate->newMultiTokenContexts);
#endif
#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
    m_deferredPaymentRequest = WTFMove(couponCodeUpdate->newDeferredPaymentRequest);
#endif
}

#endif // ENABLE(APPLE_PAY_COUPON_CODE)

void MockPaymentCoordinator::changeShippingOption(String&& shippingOption)
{
    dispatchIfShowing([page = &m_page, shippingOption = WTFMove(shippingOption)]() mutable {
        ApplePayShippingMethod shippingMethod;
        shippingMethod.identifier = WTFMove(shippingOption);
        page->paymentCoordinator().didSelectShippingMethod(shippingMethod);
    });
}

void MockPaymentCoordinator::changePaymentMethod(ApplePayPaymentMethod&& paymentMethod)
{
    dispatchIfShowing([page = &m_page, paymentMethod = WTFMove(paymentMethod)]() mutable {
        page->paymentCoordinator().didSelectPaymentMethod(MockPaymentMethod { WTFMove(paymentMethod) });
    });
}

#if ENABLE(APPLE_PAY_COUPON_CODE)

void MockPaymentCoordinator::changeCouponCode(String&& couponCode)
{
    dispatchIfShowing([page = &m_page, couponCode = WTFMove(couponCode)]() mutable {
        page->paymentCoordinator().didChangeCouponCode(WTFMove(couponCode));
    });
}

#endif // ENABLE(APPLE_PAY_COUPON_CODE)

void MockPaymentCoordinator::acceptPayment()
{
    dispatchIfShowing([page = &m_page, shippingAddress = m_shippingAddress]() mutable {
        ApplePayPayment payment;
        payment.shippingContact = WTFMove(shippingAddress);
        page->paymentCoordinator().didAuthorizePayment(MockPayment { WTFMove(payment) });
    });
}

void MockPaymentCoordinator::cancelPayment()
{
    dispatchIfShowing([page = &m_page] {
        page->paymentCoordinator().didCancelPaymentSession({ });
        ++hideCount;
        ASSERT(showCount == hideCount);
    });
}

void MockPaymentCoordinator::completePaymentSession(ApplePayPaymentAuthorizationResult&& result)
{
    auto isFinalState = result.isFinalState();
    m_errors = convert(WTFMove(result.errors));

    if (!isFinalState)
        return;

    ++hideCount;
    ASSERT(showCount == hideCount);
}

void MockPaymentCoordinator::abortPaymentSession()
{
    ++hideCount;
    ASSERT(showCount == hideCount);
}

void MockPaymentCoordinator::cancelPaymentSession()
{
    ++hideCount;
    ASSERT(showCount == hideCount);
}

void MockPaymentCoordinator::addSetupFeature(ApplePaySetupFeatureState state, ApplePaySetupFeatureType type, bool supportsInstallments)
{
    m_setupFeatures.append(MockApplePaySetupFeature::create(state, type, supportsInstallments));
}

void MockPaymentCoordinator::getSetupFeatures(const ApplePaySetupConfiguration& configuration, const URL&, CompletionHandler<void(Vector<Ref<ApplePaySetupFeature>>&&)>&& completionHandler)
{
    m_setupConfiguration = configuration;
    auto setupFeaturesCopy = m_setupFeatures;
    completionHandler(WTFMove(setupFeaturesCopy));
}

void MockPaymentCoordinator::beginApplePaySetup(const ApplePaySetupConfiguration& configuration, const URL&, Vector<RefPtr<ApplePaySetupFeature>>&&, CompletionHandler<void(bool)>&& completionHandler)
{
    m_setupConfiguration = configuration;
    completionHandler(true);
}

bool MockPaymentCoordinator::installmentConfigurationReturnsNil() const
{
#if HAVE(PASSKIT_INSTALLMENTS)
    return !PaymentInstallmentConfiguration(nullptr).platformConfiguration();
#else
    return true;
#endif
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
