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

#pragma once

// FIXME: (rdar://167375656) Remove the `__has_feature(modules)` condition when possible.
#if !__has_feature(modules)

#if USE(PASSKIT)

#import <pal/spi/cocoa/PassKitSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER_REQUIRED(PAL, PassKitCore)
#if HAVE(PASSKIT_MODULARIZATION)
#if PLATFORM(MAC)
#if HAVE(PASSKIT_MAC_HELPER_TEMP)
SOFT_LINK_FRAMEWORK_FOR_HEADER_REQUIRED(PAL, PassKitMacHelperTemp)
#else
SOFT_LINK_FRAMEWORK_FOR_HEADER_REQUIRED(PAL, PassKitMacHelper)
#endif
#endif
SOFT_LINK_FRAMEWORK_FOR_HEADER_REQUIRED(PAL, PassKitUI)
#endif

SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKContact)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPassLibrary)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPayment)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentAuthorizationViewController)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentMethod)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentMerchantSession)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentPass)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentRequest)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentSetupConfiguration)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentSetupController)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentSetupFeature)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentSetupRequest)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentSetupViewController)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentSummaryItem)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentToken)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKSecureElementPass)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKShippingMethod)

#if HAVE(PASSKIT_INSTALLMENTS)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentInstallmentConfiguration)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentInstallmentItem)
#endif

#if HAVE(PASSKIT_RECURRING_SUMMARY_ITEM)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKRecurringPaymentSummaryItem)
#endif

#if HAVE(PASSKIT_RECURRING_PAYMENTS)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKRecurringPaymentRequest)
#endif

#if HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKDeferredPaymentSummaryItem)
#endif

#if HAVE(PASSKIT_DEFERRED_PAYMENTS)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKDeferredPaymentRequest)
#endif

#if HAVE(PASSKIT_DISBURSEMENTS)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKDisbursementPaymentRequest)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKDisbursementRequest)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKDisbursementSummaryItem)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKInstantFundsOutFeeSummaryItem)
#endif

#if HAVE(PASSKIT_AUTOMATIC_RELOAD_SUMMARY_ITEM)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKAutomaticReloadPaymentSummaryItem)
#endif

#if HAVE(PASSKIT_AUTOMATIC_RELOAD_PAYMENTS)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKAutomaticReloadPaymentRequest)
#endif

#if HAVE(PASSKIT_MULTI_MERCHANT_PAYMENTS)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentTokenContext)
#endif

#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKShippingMethods)
#endif

#if HAVE(PASSKIT_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKDateComponentsRange)
#endif

#if HAVE(PASSKIT_COUPON_CODE)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentRequestCouponCodeUpdate)
#endif

#if HAVE(PASSKIT_PAYMENT_ORDER_DETAILS)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentOrderDetails)
#endif

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentAuthorizationController)
#endif

SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKCanMakePaymentsWithMerchantIdentifierAndDomain, void, (NSString *identifier, NSString *domain, PKCanMakePaymentsCompletion completion), (identifier, domain, completion))
#define PKCanMakePaymentsWithMerchantIdentifierAndDomain PAL::softLink_PassKitCore_PKCanMakePaymentsWithMerchantIdentifierAndDomain
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKDrawApplePayButtonWithCornerRadius, void, (CGContextRef context, CGRect drawRect, CGFloat scale, CGFloat cornerRadius, PKPaymentButtonType type, PKPaymentButtonStyle style, NSString *languageCode), (context, drawRect, scale, cornerRadius, type, style, languageCode))
#define PKDrawApplePayButtonWithCornerRadius PAL::softLink_PassKitCore_PKDrawApplePayButtonWithCornerRadius


SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentAuthorizationResult)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentRequestPaymentMethodUpdate)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentRequestShippingContactUpdate)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, PKPaymentRequestShippingMethodUpdate)

SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKApplePayButtonDefaultCornerRadius, CGFloat)
#define PKApplePayButtonDefaultCornerRadius PAL::get_PassKitCore_PKApplePayButtonDefaultCornerRadiusSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKContactFieldEmailAddress, PKContactField)
#define PKContactFieldEmailAddress PAL::get_PassKitCore_PKContactFieldEmailAddressSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKContactFieldName, PKContactField)
#define PKContactFieldName PAL::get_PassKitCore_PKContactFieldNameSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKContactFieldPhoneNumber, PKContactField)
#define PKContactFieldPhoneNumber PAL::get_PassKitCore_PKContactFieldPhoneNumberSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKContactFieldPhoneticName, PKContactField)
#define PKContactFieldPhoneticName PAL::get_PassKitCore_PKContactFieldPhoneticNameSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKContactFieldPostalAddress, PKContactField)
#define PKContactFieldPostalAddress PAL::get_PassKitCore_PKContactFieldPostalAddressSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKPaymentErrorContactFieldUserInfoKey, PKPaymentErrorKey)
#define PKPaymentErrorContactFieldUserInfoKey PAL::get_PassKitCore_PKPaymentErrorContactFieldUserInfoKeySingleton()
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKPassKitErrorDomain, NSString *)
#define PKPassKitErrorDomain PAL::get_PassKitCore_PKPassKitErrorDomainSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKPaymentErrorDomain, NSString *)
#define PKPaymentErrorDomain PAL::get_PassKitCore_PKPaymentErrorDomainSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKPaymentErrorPostalAddressUserInfoKey, PKPaymentErrorKey)
#define PKPaymentErrorPostalAddressUserInfoKey PAL::get_PassKitCore_PKPaymentErrorPostalAddressUserInfoKeySingleton()

SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKDisbursementErrorDomain, NSString *)
#define PKDisbursementErrorDomain PAL::get_PassKitCore_PKDisbursementErrorDomainSingleton()

#if HAVE(PASSKIT_MERCHANT_CATEGORY_CODE)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKMerchantCategoryCodeNone, PKMerchantCategoryCode)
#define PKMerchantCategoryCodeNone PAL::get_PassKitCore_PKMerchantCategoryCodeNoneSingleton()
#endif

SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, PassKitCore, PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication, void, (NSString *identifier, NSString *domain, NSString *sourceApplicationSecondaryIdentifier, PKCanMakePaymentsCompletion completion), (identifier, domain, sourceApplicationSecondaryIdentifier, completion))
#define PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication PAL::softLink_PassKitCore_PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication

#endif // USE(PASSKIT)

#endif // !__has_feature(modules)
