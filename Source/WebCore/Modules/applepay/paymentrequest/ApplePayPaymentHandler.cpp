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
#include "Frame.h"
#include "JSApplePayRequest.h"
#include "MainFrame.h"
#include "PaymentContact.h"
#include "PaymentCoordinator.h"
#include "PaymentRequestValidator.h"

namespace WebCore {

bool ApplePayPaymentHandler::handlesIdentifier(const PaymentRequest::MethodIdentifier& identifier)
{
    if (!WTF::holds_alternative<URL>(identifier))
        return false;

    auto& url = WTF::get<URL>(identifier);
    return url.host() == "apple.com" && url.path() == "/apple-pay";
}

ApplePayPaymentHandler::ApplePayPaymentHandler(PaymentRequest& paymentRequest)
    : m_paymentRequest { paymentRequest }
{
}

static ApplePaySessionPaymentRequest::LineItem convert(const PaymentItem& item)
{
    ApplePaySessionPaymentRequest::LineItem lineItem;
    lineItem.amount = item.amount.value;
    lineItem.type = item.pending ? ApplePaySessionPaymentRequest::LineItem::Type::Pending : ApplePaySessionPaymentRequest::LineItem::Type::Final;
    lineItem.label = item.label;
    return lineItem;
}

static Vector<ApplePaySessionPaymentRequest::LineItem> convert(const Vector<PaymentItem>& lineItems)
{
    Vector<ApplePaySessionPaymentRequest::LineItem> result;
    result.reserveInitialCapacity(lineItems.size());
    for (auto& lineItem : lineItems)
        result.uncheckedAppend(convert(lineItem));
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
    
static ApplePaySessionPaymentRequest::ShippingMethod convert(const PaymentShippingOption& shippingOption)
{
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

    auto validatedRequest = convertAndValidate(applePayRequest.version, applePayRequest);
    if (validatedRequest.hasException())
        return validatedRequest.releaseException();

    ApplePaySessionPaymentRequest request = validatedRequest.releaseReturnValue();
    request.setTotal(convert(m_paymentRequest->paymentDetails().total));

    auto lineItems = convert(m_paymentRequest->paymentDetails().displayItems);
    for (auto& modifier : m_paymentRequest->paymentDetails().modifiers) {
        auto convertedIdentifier = convertAndValidatePaymentMethodIdentifier(modifier.supportedMethods);
        if (convertedIdentifier && handlesIdentifier(*convertedIdentifier))
            lineItems.appendVector(convert(modifier.additionalDisplayItems));
    }
    request.setLineItems(lineItems);

    request.setRequiredShippingContactFields(convert(m_paymentRequest->paymentOptions()));
    if (m_paymentRequest->paymentOptions().requestShipping)
        request.setShippingType(convert(m_paymentRequest->paymentOptions().shippingType));

    Vector<ApplePaySessionPaymentRequest::ShippingMethod> shippingMethods;
    shippingMethods.reserveInitialCapacity(m_paymentRequest->paymentDetails().shippingOptions.size());
    for (auto& shippingOption : m_paymentRequest->paymentDetails().shippingOptions)
        shippingMethods.uncheckedAppend(convert(shippingOption));
    request.setShippingMethods(shippingMethods);

    auto exception = PaymentRequestValidator::validate(request);
    if (exception.hasException())
        return exception.releaseException();

    m_applePayRequest = WTFMove(request);
    return { };
}

void ApplePayPaymentHandler::show()
{
    // FIXME: Call PaymentCoordinator::beginPaymentSession() with m_applePayRequest
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)
