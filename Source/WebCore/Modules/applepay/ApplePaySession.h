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
#include "ExceptionOr.h"
#include "PaymentRequest.h"
#include <heap/Strong.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DeferredPromise;
class Document;
class Payment;
class PaymentContact;
class PaymentCoordinator;
class PaymentMethod;
class URL;

class ApplePaySession final : public RefCounted<ApplePaySession>, public ActiveDOMObject, public EventTargetWithInlineData {
public:
    enum class MerchantCapability { Supports3DS, SupportsEMV, SupportsCredit, SupportsDebit };
    enum class ContactField { Email, Name, Phone, PostalAddress };

    using ShippingType = WebCore::PaymentRequest::ShippingType;
    using LineItemType = WebCore::PaymentRequest::LineItem::Type;

    struct ShippingMethod {
        String label;
        String detail;
        String amount;
        String identifier;
    };

    struct LineItem {
        LineItemType type { LineItemType::Final };
        String label;
        String amount;
    };

    struct PaymentRequest {
        Vector<MerchantCapability> merchantCapabilities;
        Vector<String> supportedNetworks;
        String countryCode;
        String currencyCode;

        std::optional<Vector<ContactField>> requiredBillingContactFields;
        JSC::Strong<JSC::JSObject> billingContact;

        std::optional<Vector<ContactField>>  requiredShippingContactFields;
        JSC::Strong<JSC::JSObject> shippingContact;

        ShippingType shippingType { ShippingType::Shipping };
        std::optional<Vector<ShippingMethod>> shippingMethods;

        LineItem total;
        Vector<LineItem> lineItems;

        String applicationData;
    };

    static ExceptionOr<Ref<ApplePaySession>> create(JSC::ExecState&, Document&, unsigned version, PaymentRequest&&);
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

    static ExceptionOr<bool> supportsVersion(ScriptExecutionContext&, unsigned version);
    static ExceptionOr<bool> canMakePayments(ScriptExecutionContext&);
    static ExceptionOr<void> canMakePaymentsWithActiveCard(ScriptExecutionContext&, const String& merchantIdentifier, Ref<DeferredPromise>&&);
    static ExceptionOr<void> openPaymentSetup(ScriptExecutionContext&, const String& merchantIdentifier, Ref<DeferredPromise>&&);

    ExceptionOr<void> begin();
    ExceptionOr<void> abort();
    ExceptionOr<void> completeMerchantValidation(JSC::ExecState&, JSC::JSValue merchantSession);
    ExceptionOr<void> completeShippingMethodSelection(unsigned short status, LineItem&& newTotal, Vector<LineItem>&& newLineItems);
    ExceptionOr<void> completeShippingContactSelection(unsigned short status, Vector<ShippingMethod>&& newShippingMethods, LineItem&& newTotal, Vector<LineItem>&& newLineItems);
    ExceptionOr<void> completePaymentMethodSelection(LineItem&& newTotal, Vector<LineItem>&& newLineItems);
    ExceptionOr<void> completePayment(unsigned short status);

    const WebCore::PaymentRequest& paymentRequest() const { return m_paymentRequest; }

    void validateMerchant(const URL&);
    void didAuthorizePayment(const Payment&);
    void didSelectShippingMethod(const WebCore::PaymentRequest::ShippingMethod&);
    void didSelectShippingContact(const PaymentContact&);
    void didSelectPaymentMethod(const PaymentMethod&);
    void didCancelPayment();

    using RefCounted<ApplePaySession>::ref;
    using RefCounted<ApplePaySession>::deref;

private:
    ApplePaySession(Document&, WebCore::PaymentRequest&&);

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
    } m_state { State::Idle };

    enum class MerchantValidationState {
        Idle,
        ValidatingMerchant,
        ValidationComplete,
    } m_merchantValidationState { MerchantValidationState::Idle };

    const WebCore::PaymentRequest m_paymentRequest;
};

}

#endif
