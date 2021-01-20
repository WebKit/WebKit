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

#if ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)

#include "ApplePayPaymentMethodType.h"
#include "ApplePayRequest.h"
#include "ContextDestructionObserver.h"
#include "PaymentHandler.h"
#include "PaymentRequest.h"
#include "PaymentSession.h"
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>

namespace WebCore {

class PaymentCoordinator;

class ApplePayPaymentHandler final : public PaymentHandler, public PaymentSession, private ContextDestructionObserver {
public:
    static ExceptionOr<void> validateData(Document&, JSC::JSValue);
    static bool handlesIdentifier(const PaymentRequest::MethodIdentifier&);
    static bool hasActiveSession(Document&);

private:
    friend class PaymentHandler;
    explicit ApplePayPaymentHandler(Document&, const PaymentRequest::MethodIdentifier&, PaymentRequest&);

    Document& document() const;
    PaymentCoordinator& paymentCoordinator() const;

    ExceptionOr<Vector<ApplePaySessionPaymentRequest::ShippingMethod>> computeShippingMethods();
    ExceptionOr<ApplePaySessionPaymentRequest::TotalAndLineItems> computeTotalAndLineItems() const;
    Vector<PaymentError> computeErrors(String&& error, AddressErrors&&, PayerErrorFields&&, JSC::JSObject* paymentMethodErrors) const;
    void computeAddressErrors(String&& error, AddressErrors&&, Vector<PaymentError>&) const;
    void computePayerErrors(PayerErrorFields&&, Vector<PaymentError>&) const;
    ExceptionOr<void> computePaymentMethodErrors(JSC::JSObject* paymentMethodErrors, Vector<PaymentError>&) const;

    ExceptionOr<void> shippingAddressUpdated(Vector<PaymentError>&& errors);
    ExceptionOr<void> shippingOptionUpdated();
    ExceptionOr<void> paymentMethodUpdated();

    // PaymentHandler
    ExceptionOr<void> convertData(JSC::JSValue) final;
    ExceptionOr<void> show(Document&) final;
    void hide() final;
    void canMakePayment(Document&, WTF::Function<void(bool)>&& completionHandler) final;
    ExceptionOr<void> detailsUpdated(PaymentRequest::UpdateReason, String&& error, AddressErrors&&, PayerErrorFields&&, JSC::JSObject* paymentMethodErrors) final;
    ExceptionOr<void> merchantValidationCompleted(JSC::JSValue&&) final;
    void complete(Optional<PaymentComplete>&&) final;
    ExceptionOr<void> retry(PaymentValidationErrors&&) final;

    // PaymentSession
    unsigned version() const final;
    void validateMerchant(URL&&) final;
    void didAuthorizePayment(const Payment&) final;
    void didSelectShippingMethod(const ApplePaySessionPaymentRequest::ShippingMethod&) final;
    void didSelectShippingContact(const PaymentContact&) final;
    void didSelectPaymentMethod(const PaymentMethod&) final;
    void didCancelPaymentSession(PaymentSessionError&&) final;

    PaymentRequest::MethodIdentifier m_identifier;
    Ref<PaymentRequest> m_paymentRequest;
    Optional<ApplePayRequest> m_applePayRequest;
    Optional<ApplePayPaymentMethodType> m_selectedPaymentMethodType;
    bool m_isUpdating { false };
};

} // namespace WebCore

#endif // ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)
