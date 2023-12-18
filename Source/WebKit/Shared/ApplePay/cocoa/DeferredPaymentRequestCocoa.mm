/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "DeferredPaymentRequest.h"

#if HAVE(PASSKIT_DEFERRED_PAYMENTS)

#import <WebCore/ApplePayDeferredPaymentRequest.h>
#import <WebCore/PaymentSummaryItems.h>
#import <wtf/RetainPtr.h>

#import <pal/cocoa/PassKitSoftLink.h>

namespace WebKit {
using namespace WebCore;

RetainPtr<PKDeferredPaymentRequest> platformDeferredPaymentRequest(const ApplePayDeferredPaymentRequest& webDeferredPaymentRequest)
{
    auto pkDeferredPaymentRequest = adoptNS([PAL::allocPKDeferredPaymentRequestInstance()
        initWithPaymentDescription:webDeferredPaymentRequest.paymentDescription
        deferredBilling:platformDeferredSummaryItem(webDeferredPaymentRequest.deferredBilling)
        managementURL:[NSURL URLWithString:webDeferredPaymentRequest.managementURL]]);
    if (auto& billingAgreement = webDeferredPaymentRequest.billingAgreement; !billingAgreement.isNull())
        [pkDeferredPaymentRequest setBillingAgreement:billingAgreement];
    if (auto& freeCancellationDate = webDeferredPaymentRequest.freeCancellationDate; !freeCancellationDate.isNaN()) {
        if (auto& freeCancellationDateTimeZone = webDeferredPaymentRequest.freeCancellationDateTimeZone; !freeCancellationDateTimeZone.isNull()) {
            if (auto timeZone = [NSTimeZone timeZoneWithName:freeCancellationDateTimeZone]) {
                [pkDeferredPaymentRequest setFreeCancellationDate:[NSDate dateWithTimeIntervalSince1970:freeCancellationDate.secondsSinceEpoch().value()]];
                [pkDeferredPaymentRequest setFreeCancellationDateTimeZone:timeZone];
            }
        }
    }
    if (auto& tokenNotificationURL = webDeferredPaymentRequest.tokenNotificationURL; !tokenNotificationURL.isNull())
        [pkDeferredPaymentRequest setTokenNotificationURL:[NSURL URLWithString:tokenNotificationURL]];
    return pkDeferredPaymentRequest;
}

} // namespace WebKit

#endif // HAVE(PASSKIT_DEFERRED_PAYMENTS)
