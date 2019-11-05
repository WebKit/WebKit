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

#pragma once

#if ENABLE(APPLE_PAY)

#include "ActiveDOMObject.h"
#include "ApplePayPaymentRequest.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "PaymentSession.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace JSC {
class CallFrame;
class JSGlobalObject;
class JSValue;
}

namespace WebCore {

class DeferredPromise;
class Document;
class Payment;
class PaymentContact;
class PaymentCoordinator;
class PaymentMethod;
enum class PaymentAuthorizationStatus;
struct ApplePayLineItem;
struct ApplePayPaymentRequest;
struct ApplePayShippingMethod;
struct ApplePayPaymentAuthorizationResult;
struct ApplePayPaymentMethodUpdate;
struct ApplePayShippingContactUpdate;
struct ApplePayShippingMethodUpdate;

class ApplePaySession final : public PaymentSession, public ActiveDOMObject, public EventTargetWithInlineData {
    WTF_MAKE_ISO_ALLOCATED(ApplePaySession);
public:
    static ExceptionOr<Ref<ApplePaySession>> create(Document&, unsigned version, ApplePayPaymentRequest&&);
    virtual ~ApplePaySession();

    static const unsigned short STATUS_SUCCESS = 0;
    static const unsigned short STATUS_FAILURE = 1;
    static const unsigned short STATUS_INVALID_BILLING_POSTAL_ADDRESS = 2;
    static const unsigned short STATUS_INVALID_SHIPPING_POSTAL_ADDRESS = 3;
    static const unsigned short STATUS_INVALID_SHIPPING_CONTACT = 4;
    static const unsigned short STATUS_PIN_REQUIRED = 5;
    static const unsigned short STATUS_PIN_INCORRECT = 6;
    static const unsigned short STATUS_PIN_LOCKOUT = 7;

    static ExceptionOr<bool> supportsVersion(Document&, unsigned version);
    static ExceptionOr<bool> canMakePayments(Document&);
    static ExceptionOr<void> canMakePaymentsWithActiveCard(Document&, const String& merchantIdentifier, Ref<DeferredPromise>&&);
    static ExceptionOr<void> openPaymentSetup(Document&, const String& merchantIdentifier, Ref<DeferredPromise>&&);

    ExceptionOr<void> begin(Document&);
    ExceptionOr<void> abort();
    ExceptionOr<void> completeMerchantValidation(JSC::JSGlobalObject&, JSC::JSValue merchantSession);
    ExceptionOr<void> completeShippingMethodSelection(ApplePayShippingMethodUpdate&&);
    ExceptionOr<void> completeShippingContactSelection(ApplePayShippingContactUpdate&&);
    ExceptionOr<void> completePaymentMethodSelection(ApplePayPaymentMethodUpdate&&);
    ExceptionOr<void> completePayment(ApplePayPaymentAuthorizationResult&&);

    // Old functions.
    ExceptionOr<void> completeShippingMethodSelection(unsigned short status, ApplePayLineItem&& newTotal, Vector<ApplePayLineItem>&& newLineItems);
    ExceptionOr<void> completeShippingContactSelection(unsigned short status, Vector<ApplePayShippingMethod>&& newShippingMethods, ApplePayLineItem&& newTotal, Vector<ApplePayLineItem>&& newLineItems);
    ExceptionOr<void> completePaymentMethodSelection(ApplePayLineItem&& newTotal, Vector<ApplePayLineItem>&& newLineItems);
    ExceptionOr<void> completePayment(unsigned short status);

    const ApplePaySessionPaymentRequest& paymentRequest() const { return m_paymentRequest; }

    using PaymentSession::ref;
    using PaymentSession::deref;

private:
    ApplePaySession(Document&, unsigned version, ApplePaySessionPaymentRequest&&);

    // ActiveDOMObject.
    const char* activeDOMObjectName() const override;
    void stop() override;
    void suspend(ReasonForSuspension) override;

    // EventTargetWithInlineData.
    EventTargetInterface eventTargetInterface() const override { return ApplePaySessionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    // PaymentSession
    unsigned version() const override;
    void validateMerchant(URL&&) override;
    void didAuthorizePayment(const Payment&) override;
    void didSelectShippingMethod(const ApplePaySessionPaymentRequest::ShippingMethod&) override;
    void didSelectShippingContact(const PaymentContact&) override;
    void didSelectPaymentMethod(const PaymentMethod&) override;
    void didCancelPaymentSession(PaymentSessionError&&) override;

    PaymentCoordinator& paymentCoordinator() const;

    bool canBegin() const;
    bool canAbort() const;
    bool canCancel() const;
    bool canCompleteMerchantValidation() const;
    bool canCompleteShippingMethodSelection() const;
    bool canCompleteShippingContactSelection() const;
    bool canCompletePaymentMethodSelection() const;
    bool canCompletePayment() const;
    bool canSuspendWithoutCanceling() const;

    bool isFinalState() const;
    void didReachFinalState();

    enum class State {
        Idle,

        Active,
        ShippingMethodSelected,
        ShippingContactSelected,
        PaymentMethodSelected,
        CancelRequested,
        Authorized,
        Completed,

        Aborted,
        Canceled,
    } m_state { State::Idle };

    enum class MerchantValidationState {
        Idle,
        ValidatingMerchant,
        ValidationComplete,
    } m_merchantValidationState { MerchantValidationState::Idle };

    const ApplePaySessionPaymentRequest m_paymentRequest;
    unsigned m_version;
};

}

#endif
