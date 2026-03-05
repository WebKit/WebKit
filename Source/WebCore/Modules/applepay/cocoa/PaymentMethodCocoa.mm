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
#import "PaymentMethod.h"

#if ENABLE(APPLE_PAY)

#import "ApplePayPaymentMethod.h"
#import "ApplePayPaymentMethodType.h"
#import <pal/spi/cocoa/PassKitSPI.h>

namespace WebCore {

static void finishConverting(PKPaymentMethod *paymentMethod, ApplePayPaymentMethod& result)
{
#if HAVE(PASSKIT_INSTALLMENTS)
    if (RetainPtr<NSString> bindToken = paymentMethod.bindToken)
        result.bindToken = bindToken.get();
#else
    UNUSED_PARAM(paymentMethod);
    UNUSED_PARAM(result);
#endif
}

static ApplePayPaymentPass::ActivationState NODELETE convert(PKPaymentPassActivationState paymentPassActivationState)
{
    switch (paymentPassActivationState) {
    case PKPaymentPassActivationStateActivated:
        return ApplePayPaymentPass::ActivationState::Activated;
    case PKPaymentPassActivationStateRequiresActivation:
        return ApplePayPaymentPass::ActivationState::RequiresActivation;
    case PKPaymentPassActivationStateActivating:
        return ApplePayPaymentPass::ActivationState::Activating;
    case PKPaymentPassActivationStateSuspended:
        return ApplePayPaymentPass::ActivationState::Suspended;
    case PKPaymentPassActivationStateDeactivated:
        return ApplePayPaymentPass::ActivationState::Deactivated;
    }
}

static std::optional<ApplePayPaymentPass> convert(PKPaymentPass *paymentPass)
{
    if (!paymentPass)
        return std::nullopt;

    ApplePayPaymentPass result;

    result.primaryAccountIdentifier = paymentPass.primaryAccountIdentifier;
    result.primaryAccountNumberSuffix = paymentPass.primaryAccountNumberSuffix;

    if (RetainPtr<NSString> deviceAccountIdentifier = paymentPass.deviceAccountIdentifier)
        result.deviceAccountIdentifier = deviceAccountIdentifier.get();
    if (RetainPtr<NSString> deviceAccountNumberSuffix = paymentPass.deviceAccountNumberSuffix)
        result.deviceAccountNumberSuffix = deviceAccountNumberSuffix.get();

    result.activationState = convert(paymentPass.activationState);

    return result;
}

static std::optional<ApplePayPaymentMethod::Type> NODELETE convert(PKPaymentMethodType paymentMethodType)
{
    switch (paymentMethodType) {
    case PKPaymentMethodTypeDebit:
        return ApplePayPaymentMethod::Type::Debit;
    case PKPaymentMethodTypeCredit:
        return ApplePayPaymentMethod::Type::Credit;
    case PKPaymentMethodTypePrepaid:
        return ApplePayPaymentMethod::Type::Prepaid;
    case PKPaymentMethodTypeStore:
        return ApplePayPaymentMethod::Type::Store;
    case PKPaymentMethodTypeUnknown:
    default:
        return std::nullopt;
    }
}

static void convert(CNLabeledValue<CNPostalAddress*> *postalAddress, ApplePayPaymentContact &result)
{
    if (RetainPtr<NSString> street = postalAddress.value.street)
        result.addressLines = { String { street.get() } };
    result.subLocality = postalAddress.value.subLocality;
    result.locality = postalAddress.value.city;
    result.subAdministrativeArea = postalAddress.value.subAdministrativeArea;
    result.administrativeArea = postalAddress.value.state;
    result.postalCode = postalAddress.value.postalCode;
    result.country = postalAddress.value.country;
    result.countryCode = postalAddress.value.ISOCountryCode;
}

static std::optional<ApplePayPaymentContact> convert(CNContact *billingContact)
{
    if (!billingContact)
        return std::nullopt;

    ApplePayPaymentContact result;

    if (auto firstPhoneNumber = retainPtr(billingContact.phoneNumbers.firstObject))
        result.phoneNumber = firstPhoneNumber.get().value.stringValue;

    if (auto firstEmailAddress = retainPtr(billingContact.emailAddresses.firstObject))
        result.emailAddress = firstEmailAddress.get().value;

    result.givenName = billingContact.givenName;
    result.familyName = billingContact.familyName;

    result.phoneticGivenName = billingContact.phoneticGivenName;
    result.phoneticFamilyName = billingContact.phoneticFamilyName;

    if (RetainPtr<CNLabeledValue<CNPostalAddress*>> firstPostalAddress = billingContact.postalAddresses.firstObject)
        convert(firstPostalAddress.get(), result);

    return result;
}

static ApplePayPaymentMethod convert(PKPaymentMethod *paymentMethod)
{
    ApplePayPaymentMethod result;
    
    if (RetainPtr<NSString> displayName = paymentMethod.displayName)
        result.displayName = displayName.get();
    if (RetainPtr<NSString> network = paymentMethod.network)
        result.network = network.get();
    result.billingContact = convert(retainPtr(paymentMethod.billingAddress).get());
    result.type = convert(paymentMethod.type);
    result.paymentPass = convert(retainPtr(paymentMethod.paymentPass).get());

    finishConverting(paymentMethod, result);

    return result;
}

PaymentMethod::PaymentMethod() = default;

PaymentMethod::PaymentMethod(RetainPtr<PKPaymentMethod>&& pkPaymentMethod)
    : m_pkPaymentMethod { WTF::move(pkPaymentMethod) }
{
}

PaymentMethod::~PaymentMethod() = default;

ApplePayPaymentMethod PaymentMethod::toApplePayPaymentMethod() const
{
    return convert(m_pkPaymentMethod.get());
}

PKPaymentMethod *NODELETE PaymentMethod::pkPaymentMethod() const
{
    return m_pkPaymentMethod.get();
}

}

#endif
