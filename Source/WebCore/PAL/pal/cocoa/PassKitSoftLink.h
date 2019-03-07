/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if USE(PASSKIT)

#import <pal/spi/cocoa/PassKitSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, PassKit)

SOFT_LINK_CLASS_FOR_HEADER(PAL, PKContact)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPassLibrary)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPayment)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentAuthorizationViewController)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentMethod)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentMerchantSession)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentRequest)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentSummaryItem)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKShippingMethod)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, PassKit, PKCanMakePaymentsWithMerchantIdentifierAndDomain, void, (NSString *identifier, NSString *domain, PKCanMakePaymentsCompletion completion), (identifier, domain, completion))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, PassKit, PKDrawApplePayButton, void, (CGContextRef context, CGRect drawRect, CGFloat scale, PKPaymentButtonType type, PKPaymentButtonStyle style, NSString *languageCode), (context, drawRect, scale, type, style, languageCode))

#if HAVE(PASSKIT_GRANULAR_ERRORS)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentAuthorizationResult)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentRequestPaymentMethodUpdate)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentRequestShippingContactUpdate)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentRequestShippingMethodUpdate)

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKit, PKContactFieldEmailAddress, PKContactField)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKit, PKContactFieldName, PKContactField)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKit, PKContactFieldPhoneNumber, PKContactField)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKit, PKContactFieldPhoneticName, PKContactField)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKit, PKContactFieldPostalAddress, PKContactField)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKit, PKPaymentErrorContactFieldUserInfoKey, PKPaymentErrorKey)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKit, PKPaymentErrorDomain, NSString *)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKit, PKPaymentErrorPostalAddressUserInfoKey, PKPaymentErrorKey)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, PassKit, PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication, void, (NSString *identifier, NSString *domain, NSString *sourceApplicationSecondaryIdentifier, PKCanMakePaymentsCompletion completion), (identifier, domain, sourceApplicationSecondaryIdentifier, completion))
#endif

#endif // USE(PASSKIT)
