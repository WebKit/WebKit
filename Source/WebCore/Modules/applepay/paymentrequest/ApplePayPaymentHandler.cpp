/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ApplePayPaymentHandler.h"

#if ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)

#include "ApplePayContactField.h"
#include "ApplePayMerchantCapability.h"
#include "ApplePaySessionPaymentRequest.h"
#include "Document.h"
#include "JSApplePayRequest.h"
#include "LinkIconCollector.h"
#include "MainFrame.h"
#include "Page.h"
#include "PaymentContact.h"
#include "PaymentCoordinator.h"
#include "PaymentRequestValidator.h"
#include "Settings.h"

namespace WebCore {

bool ApplePayPaymentHandler::handlesIdentifier(const PaymentRequest::MethodIdentifier& identifier)
{
    if (!WTF::holds_alternative<URL>(identifier))
        return false;

    auto& url = WTF::get<URL>(identifier);
    return url.host() == "apple.com" && url.path() == "/apple-pay";
}

static inline PaymentCoordinator& paymentCoordinator(Document& document)
{
    return document.frame()->mainFrame().paymentCoordinator();
}

bool ApplePayPaymentHandler::hasActiveSession(Document& document)
{
    return paymentCoordinator(document).hasActiveSession();
}

ApplePayPaymentHandler::ApplePayPaymentHandler(PaymentRequest& paymentRequest)
    : m_paymentRequest { paymentRequest }
{
}

static ExceptionOr<void> validate(const PaymentCurrencyAmount& amount, const String& expectedCurrency)
{
    if (amount.currency != expectedCurrency)
        return Exception { TypeError, makeString("\"", amount.currency, "\" does not match the expected currency of \"", expectedCurrency, "\". Apple Pay requires all PaymentCurrencyAmounts to use the same currency code.") };
    return { };
}

static ExceptionOr<ApplePaySessionPaymentRequest::LineItem> convertAndValidate(const PaymentItem& item, const String& expectedCurrency)
{
    auto exception = validate(item.amount, expectedCurrency);
    if (exception.hasException())
        return exception.releaseException();

    ApplePaySessionPaymentRequest::LineItem lineItem;
    lineItem.amount = item.amount.value;
    lineItem.type = item.pending ? ApplePaySessionPaymentRequest::LineItem::Type::Pending : ApplePaySessionPaymentRequest::LineItem::Type::Final;
    lineItem.label = item.label;
    return lineItem;
}

static ExceptionOr<Vector<ApplePaySessionPaymentRequest::LineItem>> convertAndValidate(const Vector<PaymentItem>& lineItems, const String& expectedCurrency)
{
    Vector<ApplePaySessionPaymentRequest::LineItem> result;
    result.reserveInitialCapacity(lineItems.size());
    for (auto& lineItem : lineItems) {
        auto convertedLineItem = convertAndValidate(lineItem, expectedCurrency);
        if (convertedLineItem.hasException())
            return convertedLineItem.releaseException();
        result.uncheckedAppend(convertedLineItem.releaseReturnValue());
    }
    return result;
}

static ApplePaySessionPaymentRequest::ContactFields convert(const PaymentOptions& options)
{
    ApplePaySessionPaymentRequest::ContactFields result;
    result.email = options.requestPayerEmail;
    result.name = options.requestPayerName;
    result.phone = options.requestPayerPhone;
    result.postalAddress = options.requestShipping;
    return result;
}

static ApplePaySessionPaymentRequest::ShippingType convert(PaymentShippingType type)
{
    switch (type) {
    case PaymentShippingType::Shipping:
        return ApplePaySessionPaymentRequest::ShippingType::Shipping;
    case PaymentShippingType::Delivery:
        return ApplePaySessionPaymentRequest::ShippingType::Delivery;
    case PaymentShippingType::Pickup:
        return ApplePaySessionPaymentRequest::ShippingType::StorePickup;
    }

    ASSERT_NOT_REACHED();
    return ApplePaySessionPaymentRequest::ShippingType::Shipping;
}

static ExceptionOr<ApplePaySessionPaymentRequest::ShippingMethod> convertAndValidate(const PaymentShippingOption& shippingOption, const String& expectedCurrency)
{
    auto exception = validate(shippingOption.amount, expectedCurrency);
    if (exception.hasException())
        return exception.releaseException();

    ApplePaySessionPaymentRequest::ShippingMethod result;
    result.amount = shippingOption.amount.value;
    result.label = shippingOption.label;
    result.identifier = shippingOption.id;
    return result;
}

ExceptionOr<void> ApplePayPaymentHandler::convertData(JSC::ExecState& execState, JSC::JSValue&& data)
{
    auto throwScope = DECLARE_THROW_SCOPE(execState.vm());
    auto applePayRequest = convertDictionary<ApplePayRequest>(execState, WTFMove(data));
    if (throwScope.exception())
        return Exception { ExistingExceptionError };

    m_applePayRequest = WTFMove(applePayRequest);
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::show(Document& document)
{
    auto validatedRequest = convertAndValidate(m_applePayRequest->version, *m_applePayRequest);
    if (validatedRequest.hasException())
        return validatedRequest.releaseException();

    ApplePaySessionPaymentRequest request = validatedRequest.releaseReturnValue();

    String expectedCurrency = m_paymentRequest->paymentDetails().total.amount.currency;
    request.setCurrencyCode(expectedCurrency);

    auto total = convertAndValidate(m_paymentRequest->paymentDetails().total, expectedCurrency);
    ASSERT(!total.hasException());
    request.setTotal(total.releaseReturnValue());

    auto convertedLineItems = convertAndValidate(m_paymentRequest->paymentDetails().displayItems, expectedCurrency);
    if (convertedLineItems.hasException())
        return convertedLineItems.releaseException();

    auto lineItems = convertedLineItems.releaseReturnValue();
    for (auto& modifier : m_paymentRequest->paymentDetails().modifiers) {
        auto convertedIdentifier = convertAndValidatePaymentMethodIdentifier(modifier.supportedMethods);
        if (!convertedIdentifier || !handlesIdentifier(*convertedIdentifier))
            continue;

        auto additionalDisplayItems = convertAndValidate(modifier.additionalDisplayItems, expectedCurrency);
        if (additionalDisplayItems.hasException())
            return additionalDisplayItems.releaseException();

        lineItems.appendVector(additionalDisplayItems.releaseReturnValue());
    }
    request.setLineItems(lineItems);

    request.setRequiredShippingContactFields(convert(m_paymentRequest->paymentOptions()));
    if (m_paymentRequest->paymentOptions().requestShipping)
        request.setShippingType(convert(m_paymentRequest->paymentOptions().shippingType));

    Vector<ApplePaySessionPaymentRequest::ShippingMethod> shippingMethods;
    shippingMethods.reserveInitialCapacity(m_paymentRequest->paymentDetails().shippingOptions.size());
    for (auto& shippingOption : m_paymentRequest->paymentDetails().shippingOptions) {
        auto convertedShippingOption = convertAndValidate(shippingOption, expectedCurrency);
        if (convertedShippingOption.hasException())
            return convertedShippingOption.releaseException();
        shippingMethods.uncheckedAppend(convertedShippingOption.releaseReturnValue());
    }
    request.setShippingMethods(shippingMethods);

    auto exception = PaymentRequestValidator::validate(request);
    if (exception.hasException())
        return exception.releaseException();

    Vector<URL> linkIconURLs;
    for (auto& icon : LinkIconCollector { document }.iconsOfTypes({ LinkIconType::TouchIcon, LinkIconType::TouchPrecomposedIcon }))
        linkIconURLs.append(icon.url);

    paymentCoordinator(document).beginPaymentSession(*this, document.url(), linkIconURLs, request);
    return { };
}

void ApplePayPaymentHandler::hide(Document& document)
{
    paymentCoordinator(document).abortPaymentSession();
}

static bool shouldDiscloseApplePayCapability(Document& document)
{
    auto* page = document.page();
    if (!page || page->usesEphemeralSession())
        return false;

    return document.settings().applePayCapabilityDisclosureAllowed();
}

void ApplePayPaymentHandler::canMakePayment(Document& document, WTF::Function<void(bool)>&& completionHandler)
{
    if (!shouldDiscloseApplePayCapability(document)) {
        completionHandler(paymentCoordinator(document).canMakePayments());
        return;
    }

    paymentCoordinator(document).canMakePaymentsWithActiveCard(m_applePayRequest->merchantIdentifier, document.domain(), WTFMove(completionHandler));
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)
