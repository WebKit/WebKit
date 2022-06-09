/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include "PaymentSessionBase.h"

namespace WebCore {

class Document;
class Payment;
class PaymentContact;
class PaymentMethod;
class PaymentSessionError;
class ScriptExecutionContext;
struct ApplePayShippingMethod;

class PaymentSession : public virtual PaymentSessionBase {
public:
    static ExceptionOr<void> canCreateSession(Document&);
    static bool enabledForContext(ScriptExecutionContext&);

    virtual unsigned version() const = 0;
    virtual void validateMerchant(URL&&) = 0;
    virtual void didAuthorizePayment(const Payment&) = 0;
    virtual void didSelectShippingMethod(const ApplePayShippingMethod&) = 0;
    virtual void didSelectShippingContact(const PaymentContact&) = 0;
    virtual void didSelectPaymentMethod(const PaymentMethod&) = 0;
#if ENABLE(APPLE_PAY_COUPON_CODE)
    virtual void didChangeCouponCode(String&& couponCode) = 0;
#endif
    virtual void didCancelPaymentSession(PaymentSessionError&&) = 0;
};

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
