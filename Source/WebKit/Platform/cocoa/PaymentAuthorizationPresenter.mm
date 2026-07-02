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

#import "AutomaticReloadPaymentRequest.h"
#import "DeferredPaymentRequest.h"
#import "PaymentTokenContext.h"
#import "RecurringPaymentRequest.h"
#import "WKPaymentAuthorizationDelegate.h"
#import "WebPaymentCoordinatorProxyCocoa.h"
#import <WebCore/ApplePayCouponCodeUpdate.h>
#import <WebCore/ApplePayError.h>
#import <WebCore/ApplePayErrorCode.h>
#import <WebCore/ApplePayErrorContactField.h>
#import <WebCore/ApplePayPaymentAuthorizationResult.h>
#import <WebCore/ApplePayPaymentMethodUpdate.h>
#import <WebCore/ApplePayShippingContactUpdate.h>
#import <WebCore/ApplePayShippingMethodUpdate.h>
#import <WebCore/PaymentMerchantSession.h>
#import <WebCore/PaymentSummaryItems.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cocoa/PassKitSoftLink.h>

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

static PKPaymentAuthorizationStatus NODELETE toPKPaymentAuthorizationStatus(WebCore::ApplePayPaymentAuthorizationResult::Status status)
{
    switch (status) {
    case WebCore::ApplePayPaymentAuthorizationResult::Success:
        return PKPaymentAuthorizationStatusSuccess;

    case WebCore::ApplePayPaymentAuthorizationResult::Failure:
        return PKPaymentAuthorizationStatusFailure;

    case WebCore::ApplePayPaymentAuthorizationResult::PINRequired:
        return PKPaymentAuthorizationStatusPINRequired;

    case WebCore::ApplePayPaymentAuthorizationResult::PINIncorrect:
        return PKPaymentAuthorizationStatusPINIncorrect;

    case WebCore::ApplePayPaymentAuthorizationResult::PINLockout:
        return PKPaymentAuthorizationStatusPINLockout;

    default:
        ASSERT_NOT_REACHED();
        return PKPaymentAuthorizationStatusFailure;
    }
}

static PKPaymentErrorCode NODELETE toPKPaymentErrorCode(WebCore::ApplePayErrorCode code)
{
    switch (code) {
    case WebCore::ApplePayErrorCode::Unknown:
        return PKPaymentUnknownError;

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
    default:
        ASSERT_NOT_REACHED();
        return PKPaymentUnknownError;
    }

    return PKPaymentUnknownError;
}

#if HAVE(PASSKIT_DISBURSEMENTS)
static RetainPtr<PKContactField> toPKContactField(WebCore::ApplePayErrorContactField contactField)
{
    switch (contactField) {
    case WebCore::ApplePayErrorContactField::PhoneNumber:
        return PKContactFieldPhoneNumber;

    case WebCore::ApplePayErrorContactField::EmailAddress:
        return PKContactFieldEmailAddress;

    case WebCore::ApplePayErrorContactField::Name:
        return PKContactFieldName;

    case WebCore::ApplePayErrorContactField::PhoneticName:
        return PKContactFieldPhoneticName;

    case WebCore::ApplePayErrorContactField::PostalAddress:
        return PKContactFieldPostalAddress;

    case WebCore::ApplePayErrorContactField::AddressLines:
        return PKContactFieldPostalAddress;

    case WebCore::ApplePayErrorContactField::SubLocality:
        return PKContactFieldPostalAddress;

    case WebCore::ApplePayErrorContactField::Locality:
        return PKContactFieldPostalAddress;

    case WebCore::ApplePayErrorContactField::PostalCode:
        return PKContactFieldPostalAddress;

    case WebCore::ApplePayErrorContactField::SubAdministrativeArea:
        return PKContactFieldPostalAddress;

    case WebCore::ApplePayErrorContactField::AdministrativeArea:
        return PKContactFieldPostalAddress;

    case WebCore::ApplePayErrorContactField::Country:
        return PKContactFieldPostalAddress;

    case WebCore::ApplePayErrorContactField::CountryCode:
        return PKContactFieldPostalAddress;
    }
}
#endif

static NSError *toNSError(const WebCore::ApplePayError& error)
{
    auto userInfo = adoptNS([[NSMutableDictionary alloc] init]);
    [userInfo setObject:error.message().createNSString().get() forKey:NSLocalizedDescriptionKey];

#if HAVE(PASSKIT_DISBURSEMENTS)
    if (error.domain() == WebCore::ApplePayError::Domain::Disbursement) {
        switch (error.code()) {
        case WebCore::ApplePayErrorCode::UnsupportedCard:
            return [PAL::getPKDisbursementRequestClassSingleton() disbursementCardUnsupportedError];
        case WebCore::ApplePayErrorCode::RecipientContactInvalid:
            if (error.contactField())
                return [PAL::getPKDisbursementRequestClassSingleton() disbursementContactInvalidErrorWithContactField:toPKContactField(error.contactField().value()).get() localizedDescription:error.message().createNSString().get()];
            break;
        default:
            return [NSError errorWithDomain:PKDisbursementErrorDomain code:PKDisbursementUnknownError userInfo:userInfo.get()];
        }
    }
#endif

    if (auto contactField = error.contactField()) {
        RetainPtr<NSString> pkContactField;
        RetainPtr<NSString> postalAddressKey;

        switch (*contactField) {
        case WebCore::ApplePayErrorContactField::PhoneNumber:
            pkContactField = PKContactFieldPhoneNumber;
            break;
            
        case WebCore::ApplePayErrorContactField::EmailAddress:
            pkContactField = PKContactFieldEmailAddress;
            break;
            
        case WebCore::ApplePayErrorContactField::Name:
            pkContactField = PKContactFieldName;
            break;
            
        case WebCore::ApplePayErrorContactField::PhoneticName:
            pkContactField = PKContactFieldPhoneticName;
            break;
            
        case WebCore::ApplePayErrorContactField::PostalAddress:
            pkContactField = PKContactFieldPostalAddress;
            break;
            
        case WebCore::ApplePayErrorContactField::AddressLines:
            pkContactField = PKContactFieldPostalAddress;
            postalAddressKey = getCNPostalAddressStreetKeySingleton();
            break;
            
        case WebCore::ApplePayErrorContactField::SubLocality:
            pkContactField = PKContactFieldPostalAddress;
            postalAddressKey = getCNPostalAddressSubLocalityKeySingleton();
            break;
            
        case WebCore::ApplePayErrorContactField::Locality:
            pkContactField = PKContactFieldPostalAddress;
            postalAddressKey = getCNPostalAddressCityKeySingleton();
            break;
            
        case WebCore::ApplePayErrorContactField::PostalCode:
            pkContactField = PKContactFieldPostalAddress;
            postalAddressKey = getCNPostalAddressPostalCodeKeySingleton();
            break;
            
        case WebCore::ApplePayErrorContactField::SubAdministrativeArea:
            pkContactField = PKContactFieldPostalAddress;
            postalAddressKey = getCNPostalAddressSubAdministrativeAreaKeySingleton();
            break;
            
        case WebCore::ApplePayErrorContactField::AdministrativeArea:
            pkContactField = PKContactFieldPostalAddress;
            postalAddressKey = getCNPostalAddressStateKeySingleton();
            break;
            
        case WebCore::ApplePayErrorContactField::Country:
            pkContactField = PKContactFieldPostalAddress;
            postalAddressKey = getCNPostalAddressCountryKeySingleton();
            break;
            
        case WebCore::ApplePayErrorContactField::CountryCode:
            pkContactField = PKContactFieldPostalAddress;
            postalAddressKey = getCNPostalAddressISOCountryCodeKeySingleton();
            break;
        }

        [userInfo setObject:pkContactField.get() forKey:PKPaymentErrorContactFieldUserInfoKey];
        if (postalAddressKey)
            [userInfo setObject:postalAddressKey.get() forKey:PKPaymentErrorPostalAddressUserInfoKey];
    }

    return [NSError errorWithDomain:PKPaymentErrorDomain code:toPKPaymentErrorCode(error.code()) userInfo:userInfo.get()];
}

static RetainPtr<NSArray> toNSErrors(const Vector<Ref<WebCore::ApplePayError>>& errors)
{
    return createNSArray(errors, [] (auto& error) -> NSError * {
        return toNSError(error);
    });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(PaymentAuthorizationPresenter);

void PaymentAuthorizationPresenter::completeMerchantValidation(const WebCore::PaymentMerchantSession& merchantSession)
{
    ASSERT(platformDelegate());
    [protect(platformDelegate()) completeMerchantValidation:merchantSession.pkPaymentMerchantSession().get() error:nil];
}

void PaymentAuthorizationPresenter::completePaymentMethodSelection(std::optional<WebCore::ApplePayPaymentMethodUpdate>&& update)
{
    ASSERT(platformDelegate());
    if (!update) {
        [protect(platformDelegate()) completePaymentMethodSelection:nil];
        return;
    }

    RetainPtr<PKPaymentRequestPaymentMethodUpdate> paymentMethodUpdate;
#if HAVE(PASSKIT_DISBURSEMENTS)
    bool isDisbursementRequestBasedOnSummaryItems = update->newLineItems.isEmpty() ? NO : update->newLineItems.last().disbursementLineItemType == WebCore::ApplePayLineItem::DisbursementLineItemType::Disbursement;
    if (isDisbursementRequestBasedOnSummaryItems)
        paymentMethodUpdate = adoptNS([PAL::allocPKPaymentRequestPaymentMethodUpdateInstance() initWithPaymentSummaryItems:WebCore::platformDisbursementSummaryItems(WTF::move(update->newLineItems)).get()]);
    else
#endif
        paymentMethodUpdate = adoptNS([PAL::allocPKPaymentRequestPaymentMethodUpdateInstance() initWithPaymentSummaryItems:WebCore::platformSummaryItems(WTF::move(update->newTotal), WTF::move(update->newLineItems)).get()]);
#if HAVE(PASSKIT_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_SUMMARY_ITEMS)
    [paymentMethodUpdate setErrors:toNSErrors(WTF::move(update->errors)).get()];
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
    [paymentMethodUpdate setAvailableShippingMethods:toPKShippingMethods(WTF::move(update->newShippingMethods)).get()];
#else
    [paymentMethodUpdate setShippingMethods:createNSArray(WTF::move(update->newShippingMethods), [] (auto& method) {
        return toPKShippingMethod(method);
    }).get()];
#endif
#endif
#if HAVE(PASSKIT_RECURRING_PAYMENTS)
    if (auto& recurringPaymentRequest = update->newRecurringPaymentRequest)
        [paymentMethodUpdate setRecurringPaymentRequest:platformRecurringPaymentRequest(WTF::move(*recurringPaymentRequest)).get()];
#endif
#if HAVE(PASSKIT_AUTOMATIC_RELOAD_PAYMENTS)
    if (auto& automaticReloadPaymentRequest = update->newAutomaticReloadPaymentRequest)
        [paymentMethodUpdate setAutomaticReloadPaymentRequest:platformAutomaticReloadPaymentRequest(WTF::move(*automaticReloadPaymentRequest)).get()];
#endif
#if HAVE(PASSKIT_MULTI_MERCHANT_PAYMENTS)
    if (auto& multiTokenContexts = update->newMultiTokenContexts)
        [paymentMethodUpdate setMultiTokenContexts:platformPaymentTokenContexts(WTF::move(*multiTokenContexts)).get()];
#endif
#if HAVE(PASSKIT_INSTALLMENTS) && ENABLE(APPLE_PAY_INSTALLMENTS)
    [paymentMethodUpdate setInstallmentGroupIdentifier:update->installmentGroupIdentifier.createNSString().get()];
#endif // HAVE(PASSKIT_INSTALLMENTS) && ENABLE(APPLE_PAY_INSTALLMENTS)
    [protect(platformDelegate()) completePaymentMethodSelection:paymentMethodUpdate.get()];
#if HAVE(PASSKIT_DEFERRED_PAYMENTS)
    if (auto& deferredPaymentRequest = update->newDeferredPaymentRequest)
        [paymentMethodUpdate setDeferredPaymentRequest:platformDeferredPaymentRequest(WTF::move(*deferredPaymentRequest)).get()];
#endif
}

void PaymentAuthorizationPresenter::completePaymentSession(WebCore::ApplePayPaymentAuthorizationResult&& result)
{
    ASSERT(platformDelegate());
    ASSERT(result.isFinalState());

    auto status = toPKPaymentAuthorizationStatus(result.status);

    auto errors = toNSErrors(result.errors);

#if HAVE(PASSKIT_PAYMENT_ORDER_DETAILS)
    if (auto orderDetails = WTF::move(result.orderDetails)) {
        auto platformOrderDetails = adoptNS([PAL::allocPKPaymentOrderDetailsInstance() initWithOrderTypeIdentifier:orderDetails->orderTypeIdentifier.createNSString().get() orderIdentifier:orderDetails->orderIdentifier.createNSString().get() webServiceURL:adoptNS([[NSURL alloc] initWithString:orderDetails->webServiceURL.createNSString().get()]).get() authenticationToken:orderDetails->authenticationToken.createNSString().get()]);
        [protect(platformDelegate()) completePaymentSession:status errors:errors.get() orderDetails:platformOrderDetails.get()];
        return;
    }
#endif

    [protect(platformDelegate()) completePaymentSession:status errors:errors.get()];
}

void PaymentAuthorizationPresenter::completeShippingContactSelection(std::optional<WebCore::ApplePayShippingContactUpdate>&& update)
{
    ASSERT(platformDelegate());
    if (!update) {
        [protect(platformDelegate()) completeShippingContactSelection:nil];
        return;
    }

    RetainPtr<PKPaymentRequestShippingContactUpdate> shippingContactUpdate;
#if HAVE(PASSKIT_DISBURSEMENTS)
    if (update->newDisbursementRequest)
        shippingContactUpdate = adoptNS([PAL::allocPKPaymentRequestShippingContactUpdateInstance() initWithPaymentSummaryItems:WebCore::platformDisbursementSummaryItems(WTF::move(update->newLineItems)).get()]);
    else
#endif // HAVE(PASSKIT_DISBURSEMENTS)
        shippingContactUpdate = adoptNS([PAL::allocPKPaymentRequestShippingContactUpdateInstance() initWithPaymentSummaryItems:WebCore::platformSummaryItems(WTF::move(update->newTotal), WTF::move(update->newLineItems)).get()]);
    [shippingContactUpdate setErrors:toNSErrors(WTF::move(update->errors)).get()];
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
    [shippingContactUpdate setAvailableShippingMethods:toPKShippingMethods(WTF::move(update->newShippingMethods)).get()];
#else
    [shippingContactUpdate setShippingMethods:createNSArray(WTF::move(update->newShippingMethods), [] (auto& method) {
        return toPKShippingMethod(method);
    }).get()];
#endif
#if HAVE(PASSKIT_RECURRING_PAYMENTS)
    if (auto& recurringPaymentRequest = update->newRecurringPaymentRequest)
        [shippingContactUpdate setRecurringPaymentRequest:platformRecurringPaymentRequest(WTF::move(*recurringPaymentRequest)).get()];
#endif
#if HAVE(PASSKIT_AUTOMATIC_RELOAD_PAYMENTS)
    if (auto& automaticReloadPaymentRequest = update->newAutomaticReloadPaymentRequest)
        [shippingContactUpdate setAutomaticReloadPaymentRequest:platformAutomaticReloadPaymentRequest(WTF::move(*automaticReloadPaymentRequest)).get()];
#endif
#if HAVE(PASSKIT_MULTI_MERCHANT_PAYMENTS)
    if (auto& multiTokenContexts = update->newMultiTokenContexts)
        [shippingContactUpdate setMultiTokenContexts:platformPaymentTokenContexts(WTF::move(*multiTokenContexts)).get()];
#endif
#if HAVE(PASSKIT_DEFERRED_PAYMENTS)
    if (auto& deferredPaymentRequest = update->newDeferredPaymentRequest)
        [shippingContactUpdate setDeferredPaymentRequest:platformDeferredPaymentRequest(WTF::move(*deferredPaymentRequest)).get()];
#endif
    [protect(platformDelegate()) completeShippingContactSelection:shippingContactUpdate.get()];
}

void PaymentAuthorizationPresenter::completeShippingMethodSelection(std::optional<WebCore::ApplePayShippingMethodUpdate>&& update)
{
    ASSERT(platformDelegate());
    if (!update) {
        [protect(platformDelegate()) completeShippingMethodSelection:nil];
        return;
    }

    auto shippingMethodUpdate = adoptNS([PAL::allocPKPaymentRequestShippingMethodUpdateInstance() initWithPaymentSummaryItems:WebCore::platformSummaryItems(WTF::move(update->newTotal), WTF::move(update->newLineItems)).get()]);
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
    [shippingMethodUpdate setAvailableShippingMethods:toPKShippingMethods(WTF::move(update->newShippingMethods)).get()];
#elif HAVE(PASSKIT_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_SUMMARY_ITEMS)
    [shippingMethodUpdate setShippingMethods:createNSArray(WTF::move(update->newShippingMethods), [] (auto& method) {
        return toPKShippingMethod(method);
    }).get()];
#endif
#if HAVE(PASSKIT_RECURRING_PAYMENTS)
    if (auto& recurringPaymentRequest = update->newRecurringPaymentRequest)
        [shippingMethodUpdate setRecurringPaymentRequest:platformRecurringPaymentRequest(WTF::move(*recurringPaymentRequest)).get()];
#endif
#if HAVE(PASSKIT_AUTOMATIC_RELOAD_PAYMENTS)
    if (auto& automaticReloadPaymentRequest = update->newAutomaticReloadPaymentRequest)
        [shippingMethodUpdate setAutomaticReloadPaymentRequest:platformAutomaticReloadPaymentRequest(WTF::move(*automaticReloadPaymentRequest)).get()];
#endif
#if HAVE(PASSKIT_MULTI_MERCHANT_PAYMENTS)
    if (auto& multiTokenContexts = update->newMultiTokenContexts)
        [shippingMethodUpdate setMultiTokenContexts:platformPaymentTokenContexts(WTF::move(*multiTokenContexts)).get()];
#endif
#if HAVE(PASSKIT_DEFERRED_PAYMENTS)
    if (auto& deferredPaymentRequest = update->newDeferredPaymentRequest)
        [shippingMethodUpdate setDeferredPaymentRequest:platformDeferredPaymentRequest(WTF::move(*deferredPaymentRequest)).get()];
#endif
    [protect(platformDelegate()) completeShippingMethodSelection:shippingMethodUpdate.get()];
}

#if HAVE(PASSKIT_COUPON_CODE)

void PaymentAuthorizationPresenter::completeCouponCodeChange(std::optional<WebCore::ApplePayCouponCodeUpdate>&& update)
{
    ASSERT(platformDelegate());
    if (!update) {
        [protect(platformDelegate()) completeCouponCodeChange:nil];
        return;
    }

    auto couponCodeUpdate = adoptNS([PAL::allocPKPaymentRequestCouponCodeUpdateInstance() initWithPaymentSummaryItems:WebCore::platformSummaryItems(WTF::move(update->newTotal), WTF::move(update->newLineItems)).get()]);
    [couponCodeUpdate setErrors:toNSErrors(WTF::move(update->errors)).get()];
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
    [couponCodeUpdate setAvailableShippingMethods:toPKShippingMethods(WTF::move(update->newShippingMethods)).get()];
#else
    [couponCodeUpdate setShippingMethods:createNSArray(WTF::move(update->newShippingMethods), [] (auto& method) {
        return toPKShippingMethod(method);
    }).get()];
#endif
#if HAVE(PASSKIT_RECURRING_PAYMENTS)
    if (auto& recurringPaymentRequest = update->newRecurringPaymentRequest)
        [couponCodeUpdate setRecurringPaymentRequest:platformRecurringPaymentRequest(WTF::move(*recurringPaymentRequest)).get()];
#endif
#if HAVE(PASSKIT_AUTOMATIC_RELOAD_PAYMENTS)
    if (auto& automaticReloadPaymentRequest = update->newAutomaticReloadPaymentRequest)
        [couponCodeUpdate setAutomaticReloadPaymentRequest:platformAutomaticReloadPaymentRequest(WTF::move(*automaticReloadPaymentRequest)).get()];
#endif
#if HAVE(PASSKIT_MULTI_MERCHANT_PAYMENTS)
    if (auto& multiTokenContexts = update->newMultiTokenContexts)
        [couponCodeUpdate setMultiTokenContexts:platformPaymentTokenContexts(WTF::move(*multiTokenContexts)).get()];
#endif
#if HAVE(PASSKIT_DEFERRED_PAYMENTS)
    if (auto& deferredPaymentRequest = update->newDeferredPaymentRequest)
        [couponCodeUpdate setDeferredPaymentRequest:platformDeferredPaymentRequest(WTF::move(*deferredPaymentRequest)).get()];
#endif
    [protect(platformDelegate()) completeCouponCodeChange:couponCodeUpdate.get()];
}

#endif // HAVE(PASSKIT_COUPON_CODE)

} // namespace WebKit

#endif // USE(PASSKIT) && ENABLE(APPLE_PAY)
