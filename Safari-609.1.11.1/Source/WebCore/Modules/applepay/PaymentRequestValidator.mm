/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PaymentRequestValidator.h"

#if ENABLE(APPLE_PAY)

#include "ApplePaySessionPaymentRequest.h"
#include <unicode/ucurr.h>
#include <unicode/uloc.h>

namespace WebCore {

static ExceptionOr<void> validateCountryCode(const String&);
static ExceptionOr<void> validateCurrencyCode(const String&);
static ExceptionOr<void> validateMerchantCapabilities(const ApplePaySessionPaymentRequest::MerchantCapabilities&);
static ExceptionOr<void> validateSupportedNetworks(const Vector<String>&);
static ExceptionOr<void> validateShippingMethods(const Vector<ApplePaySessionPaymentRequest::ShippingMethod>&);
static ExceptionOr<void> validateShippingMethod(const ApplePaySessionPaymentRequest::ShippingMethod&);

ExceptionOr<void> PaymentRequestValidator::validate(const ApplePaySessionPaymentRequest& paymentRequest)
{
    auto validatedCountryCode = validateCountryCode(paymentRequest.countryCode());
    if (validatedCountryCode.hasException())
        return validatedCountryCode.releaseException();

    auto validatedCurrencyCode = validateCurrencyCode(paymentRequest.currencyCode());
    if (validatedCurrencyCode.hasException())
        return validatedCurrencyCode.releaseException();

    auto validatedSupportedNetworks = validateSupportedNetworks(paymentRequest.supportedNetworks());
    if (validatedSupportedNetworks.hasException())
        return validatedSupportedNetworks.releaseException();

    auto validatedMerchantCapabilities = validateMerchantCapabilities(paymentRequest.merchantCapabilities());
    if (validatedMerchantCapabilities.hasException())
        return validatedMerchantCapabilities.releaseException();

    auto validatedTotal = validateTotal(paymentRequest.total());
    if (validatedTotal.hasException())
        return validatedTotal.releaseException();

    auto validatedShippingMethods = validateShippingMethods(paymentRequest.shippingMethods());
    if (validatedShippingMethods.hasException())
        return validatedShippingMethods.releaseException();

    for (auto& countryCode : paymentRequest.supportedCountries()) {
        auto validatedCountryCode = validateCountryCode(countryCode);
        if (validatedCountryCode.hasException())
            return validatedCountryCode.releaseException();
    }

    return { };
}

ExceptionOr<void> PaymentRequestValidator::validateTotal(const ApplePaySessionPaymentRequest::LineItem& total)
{
    if (!total.label)
        return Exception { TypeError, "Missing total label." };

    if (!total.amount)
        return Exception { TypeError, "Missing total amount." };

    double amount = [NSDecimalNumber decimalNumberWithString:total.amount locale:@{ NSLocaleDecimalSeparator : @"." }].doubleValue;

    if (amount < 0)
        return Exception { TypeError, "Total amount must not be negative." };

    if (amount > 100000000)
        return Exception { TypeError, "Total amount is too big." };

    return { };
}

static ExceptionOr<void> validateCountryCode(const String& countryCode)
{
    if (!countryCode)
        return Exception { TypeError, "Missing country code." };

    for (auto *countryCodePtr = uloc_getISOCountries(); *countryCodePtr; ++countryCodePtr) {
        if (countryCode == *countryCodePtr)
            return { };
    }

    return Exception { TypeError, makeString("\"" + countryCode, "\" is not a valid country code.") };
}

static ExceptionOr<void> validateCurrencyCode(const String& currencyCode)
{
    if (!currencyCode)
        return Exception { TypeError, "Missing currency code." };

    UErrorCode errorCode = U_ZERO_ERROR;
    auto currencyCodes = std::unique_ptr<UEnumeration, void (*)(UEnumeration*)>(ucurr_openISOCurrencies(UCURR_ALL, &errorCode), uenum_close);

    int32_t length;
    while (auto *currencyCodePtr = uenum_next(currencyCodes.get(), &length, &errorCode)) {
        if (currencyCodePtr == currencyCode)
            return { };
    }

    return Exception { TypeError, makeString("\"" + currencyCode, "\" is not a valid currency code.") };
}

static ExceptionOr<void> validateMerchantCapabilities(const ApplePaySessionPaymentRequest::MerchantCapabilities& merchantCapabilities)
{
    if (!merchantCapabilities.supports3DS && !merchantCapabilities.supportsEMV && !merchantCapabilities.supportsCredit && !merchantCapabilities.supportsDebit)
        return Exception { TypeError, "Missing merchant capabilities." };

    return { };
}

static ExceptionOr<void> validateSupportedNetworks(const Vector<String>& supportedNetworks)
{
    if (supportedNetworks.isEmpty())
        return Exception { TypeError, "Missing supported networks." };

    return { };
}

static ExceptionOr<void> validateShippingMethod(const ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    NSDecimalNumber *amount = [NSDecimalNumber decimalNumberWithString:shippingMethod.amount locale:@{ NSLocaleDecimalSeparator : @"." }];
    if (amount.integerValue < 0)
        return Exception { TypeError, "Shipping method amount must be greater than or equal to zero." };

    return { };
}

static ExceptionOr<void> validateShippingMethods(const Vector<ApplePaySessionPaymentRequest::ShippingMethod>& shippingMethods)
{
    for (const auto& shippingMethod : shippingMethods) {
        auto validatedShippingMethod = validateShippingMethod(shippingMethod);
        if (validatedShippingMethod.hasException())
            return validatedShippingMethod.releaseException();
    }

    return { };
}

}

#endif
