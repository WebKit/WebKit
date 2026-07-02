/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "DisbursementRequest.h"

#if HAVE(PASSKIT_DISBURSEMENTS)

#import "WebPaymentCoordinatorProxyCocoa.h"

#import <WebCore/ApplePayContactField.h>
#import <WebCore/ApplePayDisbursementRequest.h>
#import <WebCore/ApplePaySessionPaymentRequest.h>
#import <WebCore/PaymentSummaryItems.h>

#import <pal/spi/cocoa/PassKitSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cocoa/PassKitSoftLink.h>

namespace WebKit {
using namespace WebCore;

static PKContactField platformContactField(ApplePayContactField contactField)
{
    using enum ApplePayContactField;
    switch (contactField) {
    case Email:
        return PKContactFieldEmailAddress;
    case Name:
        return PKContactFieldName;
    case PhoneticName:
        return PKContactFieldPhoneticName;
    case Phone:
        return PKContactFieldPhoneNumber;
    case PostalAddress:
        return PKContactFieldPostalAddress;
    }
    ASSERT_NOT_REACHED();
    return nil;
}

RetainPtr<PKDisbursementPaymentRequest> platformDisbursementRequest(const ApplePaySessionPaymentRequest& request, const URL& originatingURL, const std::optional<Vector<ApplePayContactField>>& requiredRecipientContactFields)
{
    // This merchantID is not actually used for web payments, passing an empty string here is fine
    auto disbursementRequest = adoptNS([PAL::allocPKDisbursementRequestInstance() initWithMerchantIdentifier:@"" currencyCode:request.currencyCode().createNSString().get() regionCode:request.countryCode().createNSString().get() supportedNetworks:createNSArray(request.supportedNetworks()).get() merchantCapabilities:toPKMerchantCapabilities(request.merchantCapabilities()) summaryItems:WebCore::platformDisbursementSummaryItems(request.lineItems()).get()]);

    // FIXME: we should consolidate the types for various contact fields in the system(WebCore::ApplePayContactField, WebCore::ApplePaySessionPaymentRequest::ContactFields etc.)
    if (requiredRecipientContactFields)
        [disbursementRequest setRequiredRecipientContactFields:createNSArray(*requiredRecipientContactFields, platformContactField).get()];

    auto disbursementPaymentRequest = adoptNS([PAL::allocPKDisbursementPaymentRequestInstance() initWithDisbursementRequest:disbursementRequest.get()]);
    [disbursementPaymentRequest setOriginatingURL:originatingURL.createNSURL().get()];
    [disbursementPaymentRequest setAPIType:PKPaymentRequestAPITypeWebPaymentRequest];
    return disbursementPaymentRequest;
}

} // namespace WebKit

#endif // HAVE(PASSKIT_DISBURSEMENTS)
