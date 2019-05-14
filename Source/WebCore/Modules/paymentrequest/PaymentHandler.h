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

#if ENABLE(PAYMENT_REQUEST)

#include "PaymentRequest.h"
#include "PaymentSessionBase.h"
#include <wtf/Function.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

class Document;
struct AddressErrors;
struct PayerErrorFields;
struct PaymentValidationErrors;

class PaymentHandler : public virtual PaymentSessionBase {
public:
    static RefPtr<PaymentHandler> create(Document&, PaymentRequest&, const PaymentRequest::MethodIdentifier&);
    static ExceptionOr<void> canCreateSession(Document&);
    static bool enabledForContext(ScriptExecutionContext&);
    static bool hasActiveSession(Document&);

    virtual ExceptionOr<void> convertData(JSC::JSValue&&) = 0;
    virtual ExceptionOr<void> show(Document&) = 0;
    virtual void hide() = 0;
    virtual void canMakePayment(Document&, WTF::Function<void(bool)>&& completionHandler) = 0;
    virtual ExceptionOr<void> detailsUpdated(PaymentRequest::UpdateReason, String&& error, AddressErrors&&, PayerErrorFields&&, JSC::JSObject* paymentMethodErrors) = 0;
    virtual ExceptionOr<void> merchantValidationCompleted(JSC::JSValue&&) = 0;
    virtual void complete(Optional<PaymentComplete>&&) = 0;
    virtual ExceptionOr<void> retry(PaymentValidationErrors&&) = 0;
};

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
