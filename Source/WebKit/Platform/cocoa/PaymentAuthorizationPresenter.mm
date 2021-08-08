/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#import "PaymentAuthorizationPresenter.h"

#if USE(PASSKIT) && ENABLE(APPLE_PAY)

#import "WKPaymentAuthorizationDelegate.h"
#import "WebPaymentCoordinatorProxyCocoa.h"
#import <WebCore/ApplePayCouponCodeUpdate.h>
#import <WebCore/ApplePayDetailsUpdateData.h>
#import <WebCore/ApplePayError.h>
#import <WebCore/ApplePayErrorCode.h>
#import <WebCore/ApplePayErrorContactField.h>
#import <WebCore/ApplePayPaymentMethodUpdate.h>
#import <WebCore/ApplePayShippingContactUpdate.h>
#import <WebCore/ApplePayShippingMethodUpdate.h>
#import <WebCore/PaymentAuthorizationStatus.h>
#import <WebCore/PaymentMerchantSession.h>
#import <WebCore/PaymentSummaryItems.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cocoa/PassKitSoftLink.h>

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/PaymentAuthorizationPresenterAdditions.mm>
#endif

// FIXME: Stop soft linking Contacts once the dependency cycle is removed on macOS (<rdar://problem/70887934>),
// or when Contacts can be upward linked (<rdar://problem/36135137>).
SOFT_LINK_FRAMEWORK(Contacts);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressCityKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressCountryKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressISOCountryCodeKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressPostalCodeKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressStateKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressStreetKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressSubAdministrativeAreaKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressSubLocalityKey, NSString *);

namespace WebKit {

// FIXME: Rather than having these free functions scattered about, Apple Pay data types should know
// how to convert themselves to and from their platform representations.

static PKPaymentAuthorizationStatus toPKPaymentAuthorizationStatus(WebCore::PaymentAuthorizationStatus status)
{
    switch (status) {
    case WebCore::PaymentAuthorizationStatus::Success:
        return PKPaymentAuthorizationStatusSuccess;
    case WebCore::PaymentAuthorizationStatus::Failure:
        return PKPaymentAuthorizationStatusFailure;
    case WebCore::PaymentAuthorizationStatus::PINRequired:
        return PKPaymentAuthorizationStatusPINRequired;
    case WebCore::PaymentAuthorizationStatus::PINIncorrect:
        return PKPaymentAuthorizationStatusPINIncorrect;
    case WebCore::PaymentAuthorizationStatus::PINLockout:
        return PKPaymentAuthorizationStatusPINLockout;
    }
}

static PKPaymentErrorCode toPKPaymentErrorCode(WebCore::ApplePayErrorCode code)
{
    switch (code) {
    case WebCore::ApplePayErrorCode::Unknown:
        break;

    case WebCore::ApplePayErrorCode::ShippingContactInvalid:
        return PKPaymentShippingContactInvalidError;

    case WebCore::ApplePayErrorCode::BillingContactInvalid:
        return PKPaymentBillingContactInvalidError;

    case WebCore::ApplePayErrorCode::AddressUnserviceable:
        return PKPaymentShippingAddressUnserviceableError;

#if ENABLE(APPLE_PAY_COUPON_CODE)
    case WebCore::ApplePayErrorCode::CouponCodeInvalid:
        return PKPaymentCouponCodeInvalidError;

    case WebCore::ApplePayErrorCode::CouponCodeExpired:
        return PKPaymentCouponCodeExpiredError;
#endif
    }

    return PKPaymentUnknownError;
}

static NSError *toNSError(const WebCore::ApplePayError& error)
{
    auto userInfo = adoptNS([[NSMutableDictionary alloc] init]);
    [userInfo setObject:error.message() forKey:NSLocalizedDescriptionKey];

    if (auto contactField = error.contactField()) {
        NSString *pkContactField = nil;
        NSString *postalAddressKey = nil;

        switch (*contactField) {
        case WebCore::ApplePayErrorContactField::PhoneNumber:
            pkContactField = PAL::get_PassKit_PKContactFieldPhoneNumber();
            break;
            
        case WebCore::ApplePayErrorContactField::EmailAddress:
            pkContactField = PAL::get_PassKit_PKContactFieldEmailAddress();
            break;
            
        case WebCore::ApplePayErrorContactField::Name:
            pkContactField = PAL::get_PassKit_PKContactFieldName();
            break;
            
        case WebCore::ApplePayErrorContactField::PhoneticName:
            pkContactField = PAL::get_PassKit_PKContactFieldPhoneticName();
            break;
            
        case WebCore::ApplePayErrorContactField::PostalAddress:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            break;
            
        case WebCore::ApplePayErrorContactField::AddressLines:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressStreetKey();
            break;
            
        case WebCore::ApplePayErrorContactField::SubLocality:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressSubLocalityKey();
            break;
            
        case WebCore::ApplePayErrorContactField::Locality:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressCityKey();
            break;
            
        case WebCore::ApplePayErrorContactField::PostalCode:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressPostalCodeKey();
            break;
            
        case WebCore::ApplePayErrorContactField::SubAdministrativeArea:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressSubAdministrativeAreaKey();
            break;
            
        case WebCore::ApplePayErrorContactField::AdministrativeArea:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressStateKey();
            break;
            
        case WebCore::ApplePayErrorContactField::Country:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressCountryKey();
            break;
            
        case WebCore::ApplePayErrorContactField::CountryCode:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressISOCountryCodeKey();
            break;
        }

        [userInfo setObject:pkContactField forKey:PAL::get_PassKit_PKPaymentErrorContactFieldUserInfoKey()];
        if (postalAddressKey)
            [userInfo setObject:postalAddressKey forKey:PAL::get_PassKit_PKPaymentErrorPostalAddressUserInfoKey()];
    }

    return [NSError errorWithDomain:PAL::get_PassKit_PKPaymentErrorDomain() code:toPKPaymentErrorCode(error.code()) userInfo:userInfo.get()];
}

static RetainPtr<NSArray> toNSErrors(const Vector<RefPtr<WebCore::ApplePayError>>& errors)
{
    return createNSArray(errors, [] (auto& error) -> NSError * {
        return error ? toNSError(*error) : nil;
    });
}

static RetainPtr<NSArray> toPKShippingMethods(const Vector<WebCore::ApplePayShippingMethod>& shippingMethods)
{
    return createNSArray(shippingMethods, [] (auto& method) {
        return toPKShippingMethod(method);
    });
}

void PaymentAuthorizationPresenter::completeMerchantValidation(const WebCore::PaymentMerchantSession& merchantSession)
{
    ASSERT(platformDelegate());
    [platformDelegate() completeMerchantValidation:merchantSession.pkPaymentMerchantSession() error:nil];
}

void PaymentAuthorizationPresenter::completePaymentMethodSelection(std::optional<WebCore::ApplePayPaymentMethodUpdate>&& update)
{
    ASSERT(platformDelegate());
    if (!update) {
        [platformDelegate() completePaymentMethodSelection:nil];
        return;
    }

    auto paymentMethodUpdate = adoptNS([PAL::allocPKPaymentRequestPaymentMethodUpdateInstance() initWithPaymentSummaryItems:WebCore::platformSummaryItems(WTFMove(update->newTotal), WTFMove(update->newLineItems))]);
#if HAVE(PASSKIT_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_SUMMARY_ITEMS)
    [paymentMethodUpdate setErrors:toNSErrors(WTFMove(update->errors)).get()];
    [paymentMethodUpdate setShippingMethods:toPKShippingMethods(WTFMove(update->newShippingMethods)).get()];
#endif
#if HAVE(PASSKIT_INSTALLMENTS) && ENABLE(APPLE_PAY_INSTALLMENTS)
    [paymentMethodUpdate setInstallmentGroupIdentifier:WTFMove(update->installmentGroupIdentifier)];
#endif // HAVE(PASSKIT_INSTALLMENTS) && ENABLE(APPLE_PAY_INSTALLMENTS)
#if defined(PaymentAuthorizationPresenterAdditions_completePaymentMethodSelection)
    PaymentAuthorizationPresenterAdditions_completePaymentMethodSelection
#endif
    [platformDelegate() completePaymentMethodSelection:paymentMethodUpdate.get()];
}

void PaymentAuthorizationPresenter::completePaymentSession(const std::optional<WebCore::PaymentAuthorizationResult>& result)
{
    ASSERT(platformDelegate());
    auto status = result ? toPKPaymentAuthorizationStatus(result->status) : PKPaymentAuthorizationStatusSuccess;
    RetainPtr<NSArray> errors = result ? toNSErrors(result->errors) : @[ ];
    [platformDelegate() completePaymentSession:status errors:errors.get()];
}

void PaymentAuthorizationPresenter::completeShippingContactSelection(std::optional<WebCore::ApplePayShippingContactUpdate>&& update)
{
    ASSERT(platformDelegate());
    if (!update) {
        [platformDelegate() completeShippingContactSelection:nil];
        return;
    }

    auto shippingContactUpdate = adoptNS([PAL::allocPKPaymentRequestShippingContactUpdateInstance() initWithErrors:toNSErrors(WTFMove(update->errors)).get()
        paymentSummaryItems:WebCore::platformSummaryItems(WTFMove(update->newTotal), WTFMove(update->newLineItems))
        shippingMethods:toPKShippingMethods(WTFMove(update->newShippingMethods)).get()]);
#if defined(PaymentAuthorizationPresenterAdditions_completeShippingContactSelection)
    PaymentAuthorizationPresenterAdditions_completeShippingContactSelection
#endif
    [platformDelegate() completeShippingContactSelection:shippingContactUpdate.get()];
}

void PaymentAuthorizationPresenter::completeShippingMethodSelection(std::optional<WebCore::ApplePayShippingMethodUpdate>&& update)
{
    ASSERT(platformDelegate());
    if (!update) {
        [platformDelegate() completeShippingMethodSelection:nil];
        return;
    }

    auto shippingMethodUpdate = adoptNS([PAL::allocPKPaymentRequestShippingMethodUpdateInstance() initWithPaymentSummaryItems:WebCore::platformSummaryItems(WTFMove(update->newTotal), WTFMove(update->newLineItems))]);
#if HAVE(PASSKIT_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_SUMMARY_ITEMS)
    [shippingMethodUpdate setShippingMethods:toPKShippingMethods(WTFMove(update->newShippingMethods)).get()];
#endif
#if defined(PaymentAuthorizationPresenterAdditions_completeShippingMethodSelection)
    PaymentAuthorizationPresenterAdditions_completeShippingMethodSelection
#endif
    [platformDelegate() completeShippingMethodSelection:shippingMethodUpdate.get()];
}

#if HAVE(PASSKIT_COUPON_CODE)

void PaymentAuthorizationPresenter::completeCouponCodeChange(std::optional<WebCore::ApplePayCouponCodeUpdate>&& update)
{
    ASSERT(platformDelegate());
    if (!update) {
        [platformDelegate() completeCouponCodeChange:nil];
        return;
    }

    auto couponCodeUpdate = adoptNS([PAL::allocPKPaymentRequestCouponCodeUpdateInstance() initWithErrors:toNSErrors(WTFMove(update->errors)).get() paymentSummaryItems:WebCore::platformSummaryItems(WTFMove(update->newTotal), WTFMove(update->newLineItems)) shippingMethods:toPKShippingMethods(WTFMove(update->newShippingMethods)).get()]);
#if defined(PaymentAuthorizationPresenterAdditions_completeCouponCodeChange)
    PaymentAuthorizationPresenterAdditions_completeCouponCodeChange
#endif
    [platformDelegate() completeCouponCodeChange:couponCodeUpdate.get()];
}

#endif // HAVE(PASSKIT_COUPON_CODE)

} // namespace WebKit

#endif // USE(PASSKIT) && ENABLE(APPLE_PAY)
