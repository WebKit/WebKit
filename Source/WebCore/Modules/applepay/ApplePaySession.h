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

#pragma once

#if ENABLE(APPLE_PAY)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "PaymentRequest.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class ArrayValue;
class DeferredWrapper;
class Dictionary;
class Document;
class Payment;
class PaymentContact;
class PaymentCoordinator;
class PaymentMethod;
class URL;

class ApplePaySession final : public RefCounted<ApplePaySession>, public ActiveDOMObject, public EventTargetWithInlineData {
public:
    static RefPtr<ApplePaySession> create(Document&, unsigned version, const Dictionary&, ExceptionCode&);
    virtual ~ApplePaySession();

    // DOM API.
    static const unsigned short STATUS_SUCCESS = 0;
    static const unsigned short STATUS_FAILURE = 1;
    static const unsigned short STATUS_INVALID_BILLING_POSTAL_ADDRESS = 2;
    static const unsigned short STATUS_INVALID_SHIPPING_POSTAL_ADDRESS = 3;
    static const unsigned short STATUS_INVALID_SHIPPING_CONTACT = 4;
    static const unsigned short STATUS_PIN_REQUIRED = 5;
    static const unsigned short STATUS_PIN_INCORRECT = 6;
    static const unsigned short STATUS_PIN_LOCKOUT = 7;

    static bool supportsVersion(ScriptExecutionContext&, unsigned version, ExceptionCode&);
    static bool canMakePayments(ScriptExecutionContext&, ExceptionCode&);
    static void canMakePaymentsWithActiveCard(ScriptExecutionContext&, const String& merchantIdentifier, DeferredWrapper&&, ExceptionCode&);

    void begin(ExceptionCode&);
    void abort(ExceptionCode&);
    void completeMerchantValidation(const Dictionary& merchantSessionDictionary, ExceptionCode&);
    void completeShippingMethodSelection(unsigned short status, const Dictionary& newTotal, const ArrayValue& newLineItems, ExceptionCode&);
    void completeShippingContactSelection(unsigned short status, const ArrayValue& newShippingMethods, const Dictionary& newTotal, const ArrayValue& newLineItems, ExceptionCode&);
    void completePaymentMethodSelection(const Dictionary& newTotal, const ArrayValue& newLineItems, ExceptionCode&);
    void completePayment(unsigned short status, ExceptionCode&);

    const PaymentRequest& paymentRequest() const { return m_paymentRequest; }

    void validateMerchant(const URL&);
    void didAuthorizePayment(const Payment&);
    void didSelectShippingMethod(const PaymentRequest::ShippingMethod&);
    void didSelectShippingContact(const PaymentContact&);
    void didSelectPaymentMethod(const PaymentMethod&);
    void didCancelPayment();

    using RefCounted<ApplePaySession>::ref;
    using RefCounted<ApplePaySession>::deref;

private:
    ApplePaySession(Document&, PaymentRequest&&);

    // ActiveDOMObject.
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;
    void stop() override;

    // EventTargetWithInlineData.
    EventTargetInterface eventTargetInterface() const override { return ApplePaySessionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    PaymentCoordinator& paymentCoordinator() const;

    bool canBegin() const;
    bool canAbort() const;
    bool canCancel() const;
    bool canCompleteMerchantValidation() const;
    bool canCompleteShippingMethodSelection() const;
    bool canCompleteShippingContactSelection() const;
    bool canCompletePaymentMethodSelection() const;
    bool canCompletePayment() const;

    bool isFinalState() const;
    void didReachFinalState();

    enum class State {
        Idle,

        Active,
        ShippingMethodSelected,
        ShippingContactSelected,
        PaymentMethodSelected,
        Authorized,
        Completed,

        Aborted,
        Canceled,
    } m_state;

    enum class MerchantValidationState {
        Idle,
        ValidatingMerchant,
        ValidationComplete,
    } m_merchantValidationState;

    const PaymentRequest m_paymentRequest;
};

}

#endif
