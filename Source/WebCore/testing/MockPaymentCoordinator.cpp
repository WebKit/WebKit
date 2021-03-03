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

#include "ApplePayPaymentMethodModeUpdate.h"
#include "ApplePayPaymentMethodUpdate.h"
#include "ApplePaySessionPaymentRequest.h"
#include "ApplePayShippingContactUpdate.h"
#include "ApplePayShippingMethodUpdate.h"
#include "MockApplePaySetupFeature.h"
#include "MockPayment.h"
#include "MockPaymentContact.h"
#include "MockPaymentMethod.h"
#include "Page.h"
#include "PaymentCoordinator.h"
#include "PaymentSessionError.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>

namespace WebCore {

MockPaymentCoordinator::MockPaymentCoordinator(Page& page)
    : m_page { page }
{
    m_availablePaymentNetworks.add("amex");
    m_availablePaymentNetworks.add("carteBancaire");
    m_availablePaymentNetworks.add("chinaUnionPay");
    m_availablePaymentNetworks.add("discover");
    m_availablePaymentNetworks.add("interac");
    m_availablePaymentNetworks.add("jcb");
    m_availablePaymentNetworks.add("masterCard");
    m_availablePaymentNetworks.add("privateLabel");
    m_availablePaymentNetworks.add("visa");
}

Optional<String> MockPaymentCoordinator::validatedPaymentNetwork(const String& paymentNetwork)
{
    auto result = m_availablePaymentNetworks.find(paymentNetwork);
    if (result == m_availablePaymentNetworks.end())
        return WTF::nullopt;
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

static void dispatchIfShowing(Function<void()>&& function)
{
    ASSERT(showCount > hideCount);
    RunLoop::main().dispatch([currentShowCount = showCount, function = WTFMove(function)]() {
        if (showCount > hideCount && showCount == currentShowCount)
            function();
    });
}

bool MockPaymentCoordinator::showPaymentUI(const URL&, const Vector<URL>&, const ApplePaySessionPaymentRequest& request)
{
    if (request.shippingContact().pkContact())
        m_shippingAddress = request.shippingContact().toApplePayPaymentContact(request.version());
    m_shippingMethods = request.shippingMethods();
    m_requiredBillingContactFields = request.requiredBillingContactFields();
    m_requiredShippingContactFields = request.requiredShippingContactFields();
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    m_installmentConfiguration = request.installmentConfiguration().applePayInstallmentConfiguration();
#endif

    ASSERT(showCount == hideCount);
    ++showCount;
    dispatchIfShowing([page = &m_page]() {
        page->paymentCoordinator().validateMerchant({ URL(), "https://webkit.org/"_s });
    });
    return true;
}

void MockPaymentCoordinator::completeMerchantValidation(const PaymentMerchantSession&)
{
    dispatchIfShowing([page = &m_page, shippingAddress = m_shippingAddress]() mutable {
        page->paymentCoordinator().didSelectShippingContact(MockPaymentContact { WTFMove(shippingAddress) });
    });
}

void MockPaymentCoordinator::completeShippingMethodSelection(Optional<ApplePayShippingMethodUpdate>&& shippingMethodUpdate)
{
    if (!shippingMethodUpdate)
        return;

    m_total = WTFMove(shippingMethodUpdate->newTotal);
    m_lineItems = WTFMove(shippingMethodUpdate->newLineItems);
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

void MockPaymentCoordinator::completeShippingContactSelection(Optional<ApplePayShippingContactUpdate>&& shippingContactUpdate)
{
    if (!shippingContactUpdate)
        return;

    m_total = WTFMove(shippingContactUpdate->newTotal);
    m_lineItems = WTFMove(shippingContactUpdate->newLineItems);
    m_errors = convert(WTFMove(shippingContactUpdate->errors));
    m_shippingMethods = WTFMove(shippingContactUpdate->newShippingMethods);
}

void MockPaymentCoordinator::completePaymentMethodSelection(Optional<ApplePayPaymentMethodUpdate>&& paymentMethodUpdate)
{
    if (!paymentMethodUpdate)
        return;

    m_total = WTFMove(paymentMethodUpdate->newTotal);
    m_lineItems = WTFMove(paymentMethodUpdate->newLineItems);
}

#if ENABLE(APPLE_PAY_PAYMENT_METHOD_MODE)

void MockPaymentCoordinator::completePaymentMethodModeChange(Optional<ApplePayPaymentMethodModeUpdate>&& paymentMethodModeUpdate)
{
    if (!paymentMethodModeUpdate)
        return;

    m_total = WTFMove(paymentMethodModeUpdate->newTotal);
    m_lineItems = WTFMove(paymentMethodModeUpdate->newLineItems);
    m_errors = convert(WTFMove(paymentMethodModeUpdate->errors));
    m_shippingMethods = WTFMove(paymentMethodModeUpdate->newShippingMethods);
}

#endif // ENABLE(APPLE_PAY_PAYMENT_METHOD_MODE)

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

void MockPaymentCoordinator::completePaymentSession(Optional<PaymentAuthorizationResult>&& result)
{
    auto isFinalState = isFinalStateResult(result);
    m_errors = convert(WTFMove(result->errors));

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

void MockPaymentCoordinator::paymentCoordinatorDestroyed()
{
    ASSERT(showCount == hideCount);
    delete this;
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

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
