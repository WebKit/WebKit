/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Payment.h"

#if ENABLE(APPLE_PAY)

#import "ApplePayPayment.h"
#import "PaymentContact.h"
#import "PaymentMethod.h"
#import <pal/spi/cocoa/PassKitSPI.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/PaymentCocoaAdditions.mm>
#else
namespace WebCore {
static void finishConverting(PKPayment *, ApplePayPayment&) { }
}
#endif

namespace WebCore {

static ApplePayPayment::Token convert(PKPaymentToken *paymentToken)
{
    ASSERT(paymentToken);

    ApplePayPayment::Token result;

    result.paymentMethod = PaymentMethod(paymentToken.paymentMethod).toApplePayPaymentMethod();

    if (NSString *transactionIdentifier = paymentToken.transactionIdentifier)
        result.transactionIdentifier = transactionIdentifier;
    if (NSData *paymentData = paymentToken.paymentData)
        result.paymentData = String::fromUTF8((const char*)paymentData.bytes, paymentData.length);

    return result;
}

static ApplePayPayment convert(unsigned version, PKPayment *payment)
{
    ASSERT(payment);

    ApplePayPayment result;

    result.token = convert(payment.token);

    if (payment.billingContact)
        result.billingContact = PaymentContact(payment.billingContact).toApplePayPaymentContact(version);
    if (payment.shippingContact)
        result.shippingContact = PaymentContact(payment.shippingContact).toApplePayPaymentContact(version);

    finishConverting(payment, result);

    return result;
}
    
Payment::Payment() = default;

Payment::Payment(RetainPtr<PKPayment>&& pkPayment)
    : m_pkPayment { WTFMove(pkPayment) }
{
}

Payment::~Payment() = default;

ApplePayPayment Payment::toApplePayPayment(unsigned version) const
{
    return convert(version, m_pkPayment.get());
}

PKPayment *Payment::pkPayment() const
{
    return m_pkPayment.get();
}

}

#endif
