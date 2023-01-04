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

#if USE(PASSKIT)

#import <pal/spi/cocoa/PassKitSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, PassKitCore)
#if HAVE(PASSKIT_MODULARIZATION)
#if PLATFORM(MAC)
#if HAVE(PASSKIT_MAC_HELPER_TEMP)
SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, PassKitMacHelperTemp)
#else
SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, PassKitMacHelper)
#endif
#endif
SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, PassKitUI)
#endif

SOFT_LINK_CLASS_FOR_HEADER(PAL, PKContact)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPassLibrary)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPayment)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentAuthorizationViewController)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentMethod)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentMerchantSession)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentRequest)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentSetupConfiguration)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentSetupController)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentSetupFeature)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentSetupRequest)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentSetupViewController)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentSummaryItem)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKShippingMethod)

#if HAVE(PASSKIT_INSTALLMENTS)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentInstallmentConfiguration)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentInstallmentItem)
#endif

#if HAVE(PASSKIT_RECURRING_SUMMARY_ITEM)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKRecurringPaymentSummaryItem)
#endif

#if HAVE(PASSKIT_RECURRING_PAYMENTS)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKRecurringPaymentRequest)
#endif

#if HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKDeferredPaymentSummaryItem)
#endif

#if HAVE(PASSKIT_AUTOMATIC_RELOAD_SUMMARY_ITEM)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKAutomaticReloadPaymentSummaryItem)
#endif

#if HAVE(PASSKIT_AUTOMATIC_RELOAD_PAYMENTS)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKAutomaticReloadPaymentRequest)
#endif

#if HAVE(PASSKIT_MULTI_MERCHANT_PAYMENTS)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentTokenContext)
#endif

#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKShippingMethods)
#endif

#if HAVE(PASSKIT_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKDateComponentsRange)
#endif

#if HAVE(PASSKIT_COUPON_CODE)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentRequestCouponCodeUpdate)
#endif

#if HAVE(PASSKIT_PAYMENT_ORDER_DETAILS)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentOrderDetails)
#endif

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentAuthorizationController)
#endif

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, PassKitCore, PKCanMakePaymentsWithMerchantIdentifierAndDomain, void, (NSString *identifier, NSString *domain, PKCanMakePaymentsCompletion completion), (identifier, domain, completion))
#define PKCanMakePaymentsWithMerchantIdentifierAndDomain PAL::softLink_PassKitCore_PKCanMakePaymentsWithMerchantIdentifierAndDomain
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, PassKitCore, PKDrawApplePayButtonWithCornerRadius, void, (CGContextRef context, CGRect drawRect, CGFloat scale, CGFloat cornerRadius, PKPaymentButtonType type, PKPaymentButtonStyle style, NSString *languageCode), (context, drawRect, scale, cornerRadius, type, style, languageCode))
#define PKDrawApplePayButtonWithCornerRadius PAL::softLink_PassKitCore_PKDrawApplePayButtonWithCornerRadius


SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentAuthorizationResult)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentRequestPaymentMethodUpdate)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentRequestShippingContactUpdate)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PKPaymentRequestShippingMethodUpdate)

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKitCore, PKApplePayButtonDefaultCornerRadius, CGFloat)
#define PKApplePayButtonDefaultCornerRadius PAL::get_PassKitCore_PKApplePayButtonDefaultCornerRadius()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKitCore, PKContactFieldEmailAddress, PKContactField)
#define PKContactFieldEmailAddress PAL::get_PassKitCore_PKContactFieldEmailAddress()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKitCore, PKContactFieldName, PKContactField)
#define PKContactFieldName PAL::get_PassKitCore_PKContactFieldName()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKitCore, PKContactFieldPhoneNumber, PKContactField)
#define PKContactFieldPhoneNumber PAL::get_PassKitCore_PKContactFieldPhoneNumber()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKitCore, PKContactFieldPhoneticName, PKContactField)
#define PKContactFieldPhoneticName PAL::get_PassKitCore_PKContactFieldPhoneticName()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKitCore, PKContactFieldPostalAddress, PKContactField)
#define PKContactFieldPostalAddress PAL::get_PassKitCore_PKContactFieldPostalAddress()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKitCore, PKPaymentErrorContactFieldUserInfoKey, PKPaymentErrorKey)
#define PKPaymentErrorContactFieldUserInfoKey PAL::get_PassKitCore_PKPaymentErrorContactFieldUserInfoKey()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKitCore, PKPassKitErrorDomain, NSString *)
#define PKPassKitErrorDomain PAL::get_PassKitCore_PKPassKitErrorDomain()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKitCore, PKPaymentErrorDomain, NSString *)
#define PKPaymentErrorDomain PAL::get_PassKitCore_PKPaymentErrorDomain()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PassKitCore, PKPaymentErrorPostalAddressUserInfoKey, PKPaymentErrorKey)
#define PKPaymentErrorPostalAddressUserInfoKey PAL::get_PassKitCore_PKPaymentErrorPostalAddressUserInfoKey()

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, PassKitCore, PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication, void, (NSString *identifier, NSString *domain, NSString *sourceApplicationSecondaryIdentifier, PKCanMakePaymentsCompletion completion), (identifier, domain, sourceApplicationSecondaryIdentifier, completion))
#define PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication PAL::softLink_PassKitCore_PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication

#endif // USE(PASSKIT)
