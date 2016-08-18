/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#include "DOMWindow.h"
#include "PageConsoleClient.h"
#include "PaymentRequest.h"
#include <unicode/ucurr.h>
#include <unicode/uloc.h>

namespace WebCore {

PaymentRequestValidator::PaymentRequestValidator(DOMWindow& window)
    : m_window(window)
{
}

PaymentRequestValidator::~PaymentRequestValidator()
{
}

bool PaymentRequestValidator::validate(const PaymentRequest& paymentRequest) const
{
    if (!validateCountryCode(paymentRequest.countryCode()))
        return false;
    if (!validateCurrencyCode(paymentRequest.currencyCode()))
        return false;
    if (!validateSupportedNetworks(paymentRequest.supportedNetworks()))
        return false;
    if (!validateMerchantCapabilities(paymentRequest.merchantCapabilities()))
        return false;
    if (!validateTotal(paymentRequest.total()))
        return false;
    if (!validateShippingMethods(paymentRequest.shippingMethods()))
        return false;
    return true;
}

bool PaymentRequestValidator::validateTotal(const PaymentRequest::LineItem& total) const
{
    if (!total.label) {
        m_window.printErrorMessage("Missing total label.");
        return false;
    }

    if (!total.amount) {
        m_window.printErrorMessage("Missing total amount.");
        return false;
    }

    if (*total.amount <= 0) {
        m_window.printErrorMessage("Total amount must be greater than zero.");
        return false;
    }

    if (*total.amount > 10000000000) {
        m_window.printErrorMessage("Total amount is too big.");
        return false;
    }

    return true;
}

bool PaymentRequestValidator::validateCountryCode(const String& countryCode) const
{
    if (!countryCode) {
        m_window.printErrorMessage("Missing country code.");
        return false;
    }

    for (auto *countryCodePtr = uloc_getISOCountries(); *countryCodePtr; ++countryCodePtr) {
        if (countryCode == *countryCodePtr)
            return true;
    }

    auto message = makeString("\"" + countryCode, "\" is not a valid country code.");
    m_window.printErrorMessage(message);

    return false;
}

bool PaymentRequestValidator::validateCurrencyCode(const String& currencyCode) const
{
    if (!currencyCode) {
        m_window.printErrorMessage("Missing currency code.");
        return false;
    }

    UErrorCode errorCode = U_ZERO_ERROR;
    auto currencyCodes = std::unique_ptr<UEnumeration, void (*)(UEnumeration*)>(ucurr_openISOCurrencies(UCURR_ALL, &errorCode), uenum_close);

    int32_t length;
    while (auto *currencyCodePtr = uenum_next(currencyCodes.get(), &length, &errorCode)) {
        if (currencyCodePtr == currencyCode)
            return true;
    }

    auto message = makeString("\"" + currencyCode, "\" is not a valid currency code.");
    m_window.printErrorMessage(message);

    return false;
}

bool PaymentRequestValidator::validateMerchantCapabilities(const PaymentRequest::MerchantCapabilities& merchantCapabilities) const
{
    if (!merchantCapabilities.supports3DS && !merchantCapabilities.supportsEMV && !merchantCapabilities.supportsCredit && !merchantCapabilities.supportsDebit) {
        m_window.printErrorMessage("Missing merchant capabilities");
        return false;
    }

    return true;
}

bool PaymentRequestValidator::validateShippingMethod(const PaymentRequest::ShippingMethod& shippingMethod) const
{
    if (shippingMethod.amount < 0) {
        m_window.printErrorMessage("Shipping method amount must be greater than or equal to zero.");
        return false;
    }

    return true;
}

bool PaymentRequestValidator::validateSupportedNetworks(const Vector<String>& supportedNetworks) const
{
    if (supportedNetworks.isEmpty()) {
        m_window.printErrorMessage("Missing supported networks");
        return false;
    }

    return true;
}

bool PaymentRequestValidator::validateShippingMethods(const Vector<PaymentRequest::ShippingMethod>& shippingMethods) const
{
    for (const auto& shippingMethod : shippingMethods) {
        if (!validateShippingMethod(shippingMethod))
            return false;
    }

    return true;
}

}

#endif
