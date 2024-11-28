/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#include "MessageSenderInlines.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxy.h"
#include "WebPaymentCoordinatorMessages.h"
#include "WebPaymentCoordinatorProxyMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/ApplePayCouponCodeUpdate.h>
#include <WebCore/ApplePayPaymentAuthorizationResult.h>
#include <WebCore/ApplePayPaymentMethodUpdate.h>
#include <WebCore/ApplePayShippingContactUpdate.h>
#include <WebCore/ApplePayShippingMethodUpdate.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, *messageSenderConnection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, *messageSenderConnection(), completion)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPaymentCoordinatorProxy);

static WeakPtr<WebPaymentCoordinatorProxy>& activePaymentCoordinatorProxy()
{
    static NeverDestroyed<WeakPtr<WebPaymentCoordinatorProxy>> activePaymentCoordinatorProxy;
    return activePaymentCoordinatorProxy.get();
}

Ref<WorkQueue> WebPaymentCoordinatorProxy::protectedCanMakePaymentsQueue() const
{
    return m_canMakePaymentsQueue;
}

IPC::Connection* WebPaymentCoordinatorProxy::messageSenderConnection() const
{
    CheckedPtr client = m_client.get();
    return client ? client->paymentCoordinatorConnection(*this) : nullptr;
}

uint64_t WebPaymentCoordinatorProxy::messageSenderDestinationID() const
{
    return m_destinationID->toUInt64();
}

void WebPaymentCoordinatorProxy::canMakePayments(CompletionHandler<void(bool)>&& reply)
{
    platformCanMakePayments(WTFMove(reply));
}

void WebPaymentCoordinatorProxy::canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!merchantIdentifier.isNull(), completionHandler(false));
    MESSAGE_CHECK_COMPLETION(!domainName.isNull(), completionHandler(false));
    platformCanMakePaymentsWithActiveCard(merchantIdentifier, domainName, WTFMove(completionHandler));
}

void WebPaymentCoordinatorProxy::openPaymentSetup(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!merchantIdentifier.isNull(), completionHandler(false));
    MESSAGE_CHECK_COMPLETION(!domainName.isNull(), completionHandler(false));
    platformOpenPaymentSetup(merchantIdentifier, domainName, WTFMove(completionHandler));
}

void WebPaymentCoordinatorProxy::showPaymentUI(WebCore::PageIdentifier destinationID, WebPageProxyIdentifier webPageProxyID, const String& originatingURLString, const Vector<String>& linkIconURLStrings, const WebCore::ApplePaySessionPaymentRequest& paymentRequest, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto& coordinator = activePaymentCoordinatorProxy())
        coordinator->didReachFinalState();
    activePaymentCoordinatorProxy() = *this;

    MESSAGE_CHECK_COMPLETION(canBegin(), completionHandler(false));
    MESSAGE_CHECK_COMPLETION(!m_destinationID, completionHandler(false));
    MESSAGE_CHECK_COMPLETION(!m_authorizationPresenter, completionHandler(false));

    m_destinationID = destinationID;
    m_state = State::Activating;

    URL originatingURL { originatingURLString };

    auto linkIconURLs = linkIconURLStrings.map([](auto& linkIconURLString) {
        return URL { linkIconURLString };
    });

    platformShowPaymentUI(webPageProxyID, originatingURL, linkIconURLs, paymentRequest, [this, weakThis = WeakPtr { *this }](bool result) {
        if (!weakThis)
            return;

        if (m_state == State::Idle) {
            ASSERT(!activePaymentCoordinatorProxy());
            ASSERT(!m_destinationID);
            ASSERT(m_merchantValidationState == MerchantValidationState::Idle);
            return;
        }

        ASSERT(m_state == State::Activating);
        if (!result) {
            didReachFinalState();
            return;
        }

        m_state = State::Active;
    });

    completionHandler(true);
}

void WebPaymentCoordinatorProxy::completeMerchantValidation(const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    MESSAGE_CHECK(m_merchantValidationState == MerchantValidationState::Validating);

    platformCompleteMerchantValidation(paymentMerchantSession);
    m_merchantValidationState = MerchantValidationState::ValidationComplete;
}

void WebPaymentCoordinatorProxy::completeShippingMethodSelection(std::optional<WebCore::ApplePayShippingMethodUpdate>&& update)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    MESSAGE_CHECK(m_state == State::ShippingMethodSelected);

    platformCompleteShippingMethodSelection(WTFMove(update));
    m_state = State::Active;
}

void WebPaymentCoordinatorProxy::completeShippingContactSelection(std::optional<WebCore::ApplePayShippingContactUpdate>&& update)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    MESSAGE_CHECK(m_state == State::ShippingContactSelected);

    platformCompleteShippingContactSelection(WTFMove(update));
    m_state = State::Active;
}

void WebPaymentCoordinatorProxy::completePaymentMethodSelection(std::optional<WebCore::ApplePayPaymentMethodUpdate>&& update)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    MESSAGE_CHECK(m_state == State::PaymentMethodSelected);

    platformCompletePaymentMethodSelection(WTFMove(update));
    m_state = State::Active;
}

#if ENABLE(APPLE_PAY_COUPON_CODE)

void WebPaymentCoordinatorProxy::completeCouponCodeChange(std::optional<WebCore::ApplePayCouponCodeUpdate>&& update)
{
    // It's possible that the payment has been canceled already.
    if (m_state == State::Idle)
        return;

    MESSAGE_CHECK(m_state == State::CouponCodeChanged);

    platformCompleteCouponCodeChange(WTFMove(update));
    m_state = State::Active;
}

#endif // ENABLE(APPLE_PAY_COUPON_CODE)

void WebPaymentCoordinatorProxy::completePaymentSession(WebCore::ApplePayPaymentAuthorizationResult&& result)
{
    // It's possible that the payment has been canceled already.
    if (!canCompletePayment())
        return;

    bool isFinalState = result.isFinalState();

    platformCompletePaymentSession(WTFMove(result));

    if (!isFinalState) {
        m_state = State::Active;
        return;
    }

    m_state = State::Completing;
}

void WebPaymentCoordinatorProxy::abortPaymentSession()
{
    // It's possible that the payment has been canceled already.
    if (!canAbort())
        return;

    didReachFinalState();
}

void WebPaymentCoordinatorProxy::cancelPaymentSession()
{
    if (!canCancel())
        return;

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

void WebPaymentCoordinatorProxy::presenterDidFinish(PaymentAuthorizationPresenter&, WebCore::PaymentSessionError&& error)
{
    didReachFinalState(WTFMove(error));
}

void WebPaymentCoordinatorProxy::presenterDidSelectShippingMethod(PaymentAuthorizationPresenter&, const WebCore::ApplePayShippingMethod& shippingMethod)
{
    ASSERT(m_state == State::Active);

    m_state = State::ShippingMethodSelected;
    send(Messages::WebPaymentCoordinator::DidSelectShippingMethod(shippingMethod));
}

#if ENABLE(APPLE_PAY_COUPON_CODE)

void WebPaymentCoordinatorProxy::presenterDidChangeCouponCode(PaymentAuthorizationPresenter&, const String& couponCode)
{
    ASSERT(m_state == State::Active);

    m_state = State::CouponCodeChanged;
    send(Messages::WebPaymentCoordinator::DidChangeCouponCode(couponCode));
}

#endif // ENABLE(APPLE_PAY_COUPON_CODE)

void WebPaymentCoordinatorProxy::presenterDidSelectShippingContact(PaymentAuthorizationPresenter&, const WebCore::PaymentContact& shippingContact)
{
    ASSERT(m_state == State::Active);

    m_state = State::ShippingContactSelected;
    send(Messages::WebPaymentCoordinator::DidSelectShippingContact(shippingContact));
}

CocoaWindow* WebPaymentCoordinatorProxy::presentingWindowForPaymentAuthorization(PaymentAuthorizationPresenter&) const
{
    CheckedPtr client = m_client.get();
    return client ? client->paymentCoordinatorPresentingWindow(*this) : nullptr;
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
    case State::Completing:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
    case State::Deactivating:
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
        return true;

    case State::Completing:
    case State::Idle:
    case State::Deactivating:
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
    case State::Completing:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
    case State::Deactivating:
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
        return true;

    case State::Completing:
    case State::Idle:
    case State::Deactivating:
        return false;
    }
}

void WebPaymentCoordinatorProxy::webProcessExited()
{
    if (m_state != State::Idle)
        m_state = State::Deactivating;
}

void WebPaymentCoordinatorProxy::didReachFinalState(WebCore::PaymentSessionError&& error)
{
    ASSERT(!canBegin());

    if (canCancel())
        send(Messages::WebPaymentCoordinator::DidCancelPaymentSession(error));

    platformHidePaymentUI();

    m_authorizationPresenter = nullptr;
    m_destinationID = std::nullopt;
    m_merchantValidationState = MerchantValidationState::Idle;
    m_state = State::Idle;

    ASSERT(activePaymentCoordinatorProxy() == this);
    activePaymentCoordinatorProxy() = nullptr;
}

}

#undef MESSAGE_CHECK

#endif
