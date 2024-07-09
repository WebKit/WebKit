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

#import "config.h"
#import "PaymentRequestValidator.h"

#if ENABLE(APPLE_PAY)

#import "ApplePaySessionPaymentRequest.h"
#import "ApplePayShippingMethod.h"
#import <unicode/ucurr.h>
#import <unicode/uloc.h>
#import <wtf/text/MakeString.h>
#import <wtf/unicode/icu/ICUHelpers.h>

namespace WebCore {

static ExceptionOr<void> validateCountryCode(const String&);
static ExceptionOr<void> validateCurrencyCode(const String&);
static ExceptionOr<void> validateMerchantCapabilities(const ApplePaySessionPaymentRequest::MerchantCapabilities&);
static ExceptionOr<void> validateSupportedNetworks(const Vector<String>&);
static ExceptionOr<void> validateShippingMethods(const Vector<ApplePayShippingMethod>&);
static ExceptionOr<void> validateShippingMethod(const ApplePayShippingMethod&);

ExceptionOr<void> PaymentRequestValidator::validate(const ApplePaySessionPaymentRequest& paymentRequest, OptionSet<Field> fieldsToValidate)
{
    if (fieldsToValidate.contains(Field::CountryCode)) {
        auto validatedCountryCode = validateCountryCode(paymentRequest.countryCode());
        if (validatedCountryCode.hasException())
            return validatedCountryCode.releaseException();
    }

    if (fieldsToValidate.contains(Field::CurrencyCode)) {
        auto validatedCurrencyCode = validateCurrencyCode(paymentRequest.currencyCode());
        if (validatedCurrencyCode.hasException())
            return validatedCurrencyCode.releaseException();
    }

    if (fieldsToValidate.contains(Field::SupportedNetworks)) {
        auto validatedSupportedNetworks = validateSupportedNetworks(paymentRequest.supportedNetworks());
        if (validatedSupportedNetworks.hasException())
            return validatedSupportedNetworks.releaseException();
    }

    if (fieldsToValidate.contains(Field::MerchantCapabilities)) {
        auto validatedMerchantCapabilities = validateMerchantCapabilities(paymentRequest.merchantCapabilities());
        if (validatedMerchantCapabilities.hasException())
            return validatedMerchantCapabilities.releaseException();
    }

    if (fieldsToValidate.contains(Field::Total)) {
        auto validatedTotal = validateTotal(paymentRequest.total());
        if (validatedTotal.hasException())
            return validatedTotal.releaseException();
    }

    if (fieldsToValidate.contains(Field::ShippingMethods)) {
        auto validatedShippingMethods = validateShippingMethods(paymentRequest.shippingMethods());
        if (validatedShippingMethods.hasException())
            return validatedShippingMethods.releaseException();
    }

    if (fieldsToValidate.contains(Field::CountryCode)) {
        for (auto& countryCode : paymentRequest.supportedCountries()) {
            auto validatedCountryCode = validateCountryCode(countryCode);
            if (validatedCountryCode.hasException())
                return validatedCountryCode.releaseException();
        }
    }

    return { };
}

ExceptionOr<void> PaymentRequestValidator::validateTotal(const ApplePayLineItem& total)
{
    if (!total.label)
        return Exception { ExceptionCode::TypeError, "Missing total label."_s };

    if (!total.amount)
        return Exception { ExceptionCode::TypeError, "Missing total amount."_s };

    double amount = [NSDecimalNumber decimalNumberWithString:total.amount locale:@{ NSLocaleDecimalSeparator : @"." }].doubleValue;

    if (amount < 0)
        return Exception { ExceptionCode::TypeError, "Total amount must not be negative."_s };

    // We can safely defer a maximum amount check to the underlying payment system, instead.
    // The downside is we lose an informative error mode and get an opaque payment sheet error for too large total amounts.
    // FIXME: <https://webkit.org/b/276088> PaymentRequestValidator should adopt per-currency checks for total amounts.

    return { };
}

static ExceptionOr<void> validateCountryCode(const String& countryCode)
{
    if (!countryCode)
        return Exception { ExceptionCode::TypeError, "Missing country code."_s };

    for (auto *countryCodePtr = uloc_getISOCountries(); *countryCodePtr; ++countryCodePtr) {
        if (countryCode == StringView::fromLatin1(*countryCodePtr))
            return { };
    }

    return Exception { ExceptionCode::TypeError, makeString("\""_s, countryCode, "\" is not a valid country code."_s) };
}

static ExceptionOr<void> validateCurrencyCode(const String& currencyCode)
{
    if (!currencyCode)
        return Exception { ExceptionCode::TypeError, "Missing currency code."_s };

    UErrorCode errorCode = U_ZERO_ERROR;
    auto currencyCodes = std::unique_ptr<UEnumeration, ICUDeleter<uenum_close>>(ucurr_openISOCurrencies(UCURR_ALL, &errorCode));

    int32_t length;
    while (auto *currencyCodePtr = uenum_next(currencyCodes.get(), &length, &errorCode)) {
        if (currencyCode == StringView::fromLatin1(currencyCodePtr))
            return { };
    }

    return Exception { ExceptionCode::TypeError, makeString("\""_s, currencyCode, "\" is not a valid currency code."_s) };
}

static ExceptionOr<void> validateMerchantCapabilities(const ApplePaySessionPaymentRequest::MerchantCapabilities& merchantCapabilities)
{
    if (!merchantCapabilities.supports3DS && !merchantCapabilities.supportsEMV && !merchantCapabilities.supportsCredit && !merchantCapabilities.supportsDebit)
        return Exception { ExceptionCode::TypeError, "Missing merchant capabilities."_s };

    return { };
}

static ExceptionOr<void> validateSupportedNetworks(const Vector<String>& supportedNetworks)
{
    if (supportedNetworks.isEmpty())
        return Exception { ExceptionCode::TypeError, "Missing supported networks."_s };

    return { };
}

static ExceptionOr<void> validateShippingMethod(const ApplePayShippingMethod& shippingMethod)
{
    NSDecimalNumber *amount = [NSDecimalNumber decimalNumberWithString:shippingMethod.amount locale:@{ NSLocaleDecimalSeparator : @"." }];
    if (amount.integerValue < 0)
        return Exception { ExceptionCode::TypeError, "Shipping method amount must be greater than or equal to zero."_s };

    return { };
}

static ExceptionOr<void> validateShippingMethods(const Vector<ApplePayShippingMethod>& shippingMethods)
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
