/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(APPLE_PAY_AMS_UI) && ENABLE(PAYMENT_REQUEST)

#include "ApplePayAMSUIRequest.h"
#include "ContextDestructionObserver.h"
#include "ExceptionOr.h"
#include "PaymentHandler.h"
#include "PaymentRequest.h"
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class JSObject;
class JSValue;
}

namespace WebCore {

class Document;
class Page;

struct AddressErrors;
struct PayerErrorFields;

enum class PaymentComplete;

class ApplePayAMSUIPaymentHandler final : public PaymentHandler, private ContextDestructionObserver {
public:
    static ExceptionOr<void> validateData(Document&, JSC::JSValue);
    static bool handlesIdentifier(const PaymentRequest::MethodIdentifier&);
    static bool hasActiveSession(Document&);

    void finishSession(std::optional<bool>&&);

private:
    friend class PaymentHandler;
    explicit ApplePayAMSUIPaymentHandler(Document&, const PaymentRequest::MethodIdentifier&, PaymentRequest&);

    Document& document() const;
    Page& page() const;

    // PaymentHandler
    ExceptionOr<void> convertData(Document&, JSC::JSValue) final;
    ExceptionOr<void> show(Document&) final;
    bool canAbortSession() final { return false; }
    void hide() final;
    void canMakePayment(Document&, Function<void(bool)>&& completionHandler) final;
    ExceptionOr<void> detailsUpdated(PaymentRequest::UpdateReason, String&& error, AddressErrors&&, PayerErrorFields&&, JSC::JSObject* paymentMethodErrors) final;
    ExceptionOr<void> merchantValidationCompleted(JSC::JSValue&&) final;
    ExceptionOr<void> complete(Document&, std::optional<PaymentComplete>&&, String&& serializedData) final;
    ExceptionOr<void> retry(PaymentValidationErrors&&) final;

    PaymentRequest::MethodIdentifier m_identifier;
    Ref<PaymentRequest> m_paymentRequest;
    std::optional<ApplePayAMSUIRequest> m_applePayAMSUIRequest;
};

} // namespace WebCore

#endif // ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)
