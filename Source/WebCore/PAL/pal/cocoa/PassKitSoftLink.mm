/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#if USE(PASSKIT)

#import "PassKitSPI.h"
#import <wtf/SoftLinking.h>

#if PLATFORM(MAC)

SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PAL_EXPORT)

SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(PAL, PassKit, PKPaymentSetupConfiguration, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(PAL, PassKit, PKPaymentSetupController, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(PAL, PassKit, PKPaymentSetupFeature, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(PAL, PassKit, PKPaymentSetupRequest, PAL_EXPORT)

#else // PLATFORM(MAC)

SOFT_LINK_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PAL_EXPORT)

SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentSetupConfiguration, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentSetupController, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentSetupFeature, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentSetupRequest, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentSetupViewController, PAL_EXPORT)

#endif // PLATFORM(MAC)

#if HAVE(PASSKIT_INSTALLMENTS)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(PAL, PassKit, PKPaymentInstallmentConfiguration, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(PAL, PassKit, PKPaymentInstallmentItem, PAL_EXPORT)
#endif

#if HAVE(PASSKIT_RECURRING_SUMMARY_ITEM)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKRecurringPaymentSummaryItem, PAL_EXPORT)
#endif

#if HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKDeferredPaymentSummaryItem, PAL_EXPORT)
#endif

#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKShippingMethods, PAL_EXPORT)
#endif

#if HAVE(PASSKIT_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKDateComponentsRange, PAL_EXPORT)
#endif

#if HAVE(PASSKIT_COUPON_CODE)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentRequestCouponCodeUpdate, PAL_EXPORT)
#endif

SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKContact, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPassLibrary, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPayment, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentAuthorizationViewController, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentMerchantSession, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentMethod, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentRequest, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentSummaryItem, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKShippingMethod, PAL_EXPORT)

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentAuthorizationController, PAL_EXPORT)
#endif

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKCanMakePaymentsWithMerchantIdentifierAndDomain, void, (NSString *identifier, NSString *domain, PKCanMakePaymentsCompletion completion), (identifier, domain, completion), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, PassKit, PKDrawApplePayButtonWithCornerRadius, void, (CGContextRef context, CGRect drawRect, CGFloat scale, CGFloat cornerRadius, PKPaymentButtonType type, PKPaymentButtonStyle style, NSString *languageCode), (context, drawRect, scale, cornerRadius, type, style, languageCode))

SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentAuthorizationResult, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentRequestPaymentMethodUpdate, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentRequestShippingContactUpdate, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentRequestShippingMethodUpdate, PAL_EXPORT)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKApplePayButtonDefaultCornerRadius, CGFloat, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKContactFieldEmailAddress, PKContactField, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKContactFieldName, PKContactField, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKContactFieldPhoneNumber, PKContactField, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKContactFieldPhoneticName, PKContactField, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKContactFieldPostalAddress, PKContactField, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentErrorContactFieldUserInfoKey, PKPaymentErrorKey, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPassKitErrorDomain, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentErrorDomain, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKPaymentErrorPostalAddressUserInfoKey, PKPaymentErrorKey, PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, PassKit, PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication, void, (NSString *identifier, NSString *domain, NSString *sourceApplicationSecondaryIdentifier, PKCanMakePaymentsCompletion completion), (identifier, domain, sourceApplicationSecondaryIdentifier, completion), PAL_EXPORT)

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/PassKitSoftLinkAdditions.mm>
#endif

#endif // USE(PASSKIT)
