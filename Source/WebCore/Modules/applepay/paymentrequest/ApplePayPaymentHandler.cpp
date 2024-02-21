/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "AddressErrors.h"
#include "ApplePayContactField.h"
#include "ApplePayCouponCodeDetails.h"
#include "ApplePayCouponCodeUpdate.h"
#include "ApplePayError.h"
#include "ApplePayErrorCode.h"
#include "ApplePayErrorContactField.h"
#include "ApplePayLineItem.h"
#include "ApplePayMerchantCapability.h"
#include "ApplePayModifier.h"
#include "ApplePayPayment.h"
#include "ApplePayPaymentAuthorizationResult.h"
#include "ApplePayPaymentCompleteDetails.h"
#include "ApplePayPaymentMethodUpdate.h"
#include "ApplePaySessionPaymentRequest.h"
#include "ApplePayShippingContactUpdate.h"
#include "ApplePayShippingMethod.h"
#include "ApplePayShippingMethodUpdate.h"
#include "Document.h"
#include "EventNames.h"
#include "JSApplePayCouponCodeDetails.h"
#include "JSApplePayError.h"
#include "JSApplePayPayment.h"
#include "JSApplePayPaymentMethod.h"
#include "JSApplePayRequest.h"
#include "JSDOMConvert.h"
#include "LinkIconCollector.h"
#include "LocalFrame.h"
#include "MerchantValidationEvent.h"
#include "Page.h"
#include "PayerErrorFields.h"
#include "Payment.h"
#include "PaymentComplete.h"
#include "PaymentContact.h"
#include "PaymentCoordinator.h"
#include "PaymentDetailsModifier.h"
#include "PaymentMerchantSession.h"
#include "PaymentMethod.h"
#include "PaymentRequestUtilities.h"
#include "PaymentRequestValidator.h"
#include "PaymentResponse.h"
#include "PaymentValidationErrors.h"
#include "Settings.h"
#include <JavaScriptCore/JSONObject.h>

namespace WebCore {

static inline PaymentCoordinator& paymentCoordinator(Document& document)
{
    ASSERT(document.page());
    return document.page()->paymentCoordinator();
}

static ExceptionOr<ApplePayRequest> convertAndValidateApplePayRequest(Document& document, JSC::JSValue data)
{
    if (data.isEmpty())
        return Exception { ExceptionCode::TypeError, "Missing payment method data."_s };

    auto throwScope = DECLARE_THROW_SCOPE(document.vm());
    auto applePayRequest = convertDictionary<ApplePayRequest>(*document.globalObject(), data);
    if (throwScope.exception())
        return Exception { ExceptionCode::ExistingExceptionError };

    auto validatedRequest = convertAndValidate(document, applePayRequest.version, applePayRequest, paymentCoordinator(document));
    if (validatedRequest.hasException())
        return validatedRequest.releaseException();

    constexpr OptionSet fieldsToValidate = {
        PaymentRequestValidator::Field::MerchantCapabilities,
        PaymentRequestValidator::Field::SupportedNetworks,
        PaymentRequestValidator::Field::CountryCode,
    };
    auto exception = PaymentRequestValidator::validate(validatedRequest.releaseReturnValue(), fieldsToValidate);
    if (exception.hasException())
        return exception.releaseException();

    return WTFMove(applePayRequest);
}

ExceptionOr<void> ApplePayPaymentHandler::validateData(Document& document, JSC::JSValue data)
{
    auto requestOrException = convertAndValidateApplePayRequest(document, data);
    if (requestOrException.hasException())
        return requestOrException.releaseException();

    return { };
}

bool ApplePayPaymentHandler::handlesIdentifier(const PaymentRequest::MethodIdentifier& identifier)
{
    if (!std::holds_alternative<URL>(identifier))
        return false;

    auto& url = std::get<URL>(identifier);
    return url.host() == "apple.com"_s && url.path() == "/apple-pay"_s;
}

bool ApplePayPaymentHandler::hasActiveSession(Document& document)
{
    return WebCore::paymentCoordinator(document).hasActiveSession();
}

ApplePayPaymentHandler::ApplePayPaymentHandler(Document& document, const PaymentRequest::MethodIdentifier& identifier, PaymentRequest& paymentRequest)
    : ContextDestructionObserver { &document }
    , m_identifier { identifier }
    , m_paymentRequest { paymentRequest }
{
    ASSERT(handlesIdentifier(m_identifier));
}

Document& ApplePayPaymentHandler::document() const
{
    return downcast<Document>(*scriptExecutionContext());
}

PaymentCoordinator& ApplePayPaymentHandler::paymentCoordinator() const
{
    return WebCore::paymentCoordinator(document());
}

static ExceptionOr<void> validate(const PaymentCurrencyAmount& amount, const String& expectedCurrency)
{
    if (amount.currency != expectedCurrency)
        return Exception { ExceptionCode::TypeError, makeString("\"", amount.currency, "\" does not match the expected currency of \"", expectedCurrency, "\". Apple Pay requires all PaymentCurrencyAmounts to use the same currency code.") };
    return { };
}

static ExceptionOr<ApplePayLineItem> convertAndValidate(const PaymentItem& item, const String& expectedCurrency)
{
    auto exception = validate(item.amount, expectedCurrency);
    if (exception.hasException())
        return exception.releaseException();


    ApplePayLineItem lineItem;
    lineItem.amount = item.amount.value;
    lineItem.type = item.pending ? ApplePayLineItem::Type::Pending : ApplePayLineItem::Type::Final;
    lineItem.label = item.label;
    return { WTFMove(lineItem) };
}

static ExceptionOr<Vector<ApplePayLineItem>> convertAndValidate(const Vector<PaymentItem>& lineItems, const String& expectedCurrency)
{
    Vector<ApplePayLineItem> result;
    result.reserveInitialCapacity(lineItems.size());
    for (auto& lineItem : lineItems) {
        auto convertedLineItem = convertAndValidate(lineItem, expectedCurrency);
        if (convertedLineItem.hasException())
            return convertedLineItem.releaseException();
        result.append(convertedLineItem.releaseReturnValue());
    }
    return { WTFMove(result) };
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

static ExceptionOr<ApplePayShippingMethod> convertAndValidate(const PaymentShippingOption& shippingOption, const String& expectedCurrency)
{
    auto exception = validate(shippingOption.amount, expectedCurrency);
    if (exception.hasException())
        return exception.releaseException();

    ApplePayShippingMethod result;
    result.amount = shippingOption.amount.value;
    result.label = shippingOption.label;
    result.identifier = shippingOption.id;
    return { WTFMove(result) };
}

ExceptionOr<void> ApplePayPaymentHandler::convertData(Document& document, JSC::JSValue data)
{
    auto requestOrException = convertAndValidateApplePayRequest(document, data);
    if (requestOrException.hasException())
        return requestOrException.releaseException();

    m_applePayRequest = requestOrException.releaseReturnValue();
    return { };
}

static void mergePaymentOptions(const PaymentOptions& options, ApplePaySessionPaymentRequest& request)
{
    auto requiredShippingContactFields = request.requiredShippingContactFields();
    requiredShippingContactFields.email |= options.requestPayerEmail;
    requiredShippingContactFields.name |= options.requestPayerName;
    requiredShippingContactFields.phone |= options.requestPayerPhone;
    requiredShippingContactFields.postalAddress |= options.requestShipping;
    request.setRequiredShippingContactFields(requiredShippingContactFields);

    auto requiredBillingContactFields = request.requiredBillingContactFields();
    requiredBillingContactFields.postalAddress |= options.requestBillingAddress;
    request.setRequiredBillingContactFields(requiredBillingContactFields);
    
    if (options.requestShipping)
        request.setShippingType(convert(options.shippingType));
}

ExceptionOr<void> ApplePayPaymentHandler::show(Document& document)
{
    auto validatedRequest = convertAndValidate(document, m_applePayRequest->version, *m_applePayRequest, paymentCoordinator());
    if (validatedRequest.hasException())
        return validatedRequest.releaseException();

    ApplePaySessionPaymentRequest request = validatedRequest.releaseReturnValue();
    request.setRequester(ApplePaySessionPaymentRequest::Requester::PaymentRequest);

    auto& details = m_paymentRequest->paymentDetails();

    String expectedCurrency = details.total.amount.currency;
    request.setCurrencyCode(expectedCurrency);

    auto total = convertAndValidate(details.total, expectedCurrency);
    ASSERT(!total.hasException());
    request.setTotal(total.releaseReturnValue());

    if (details.displayItems) {
        auto convertedLineItems = convertAndValidate(*details.displayItems, expectedCurrency);
        if (convertedLineItems.hasException())
            return convertedLineItems.releaseException();
        request.setLineItems(convertedLineItems.releaseReturnValue());
    }

    mergePaymentOptions(m_paymentRequest->paymentOptions(), request);

    auto shippingMethods = computeShippingMethods();
    if (shippingMethods.hasException())
        return shippingMethods.releaseException();
    request.setShippingMethods(shippingMethods.releaseReturnValue());

    auto modifierException = firstApplicableModifier();
    if (modifierException.hasException())
        return modifierException.releaseException();
    if (auto modifierData = modifierException.releaseReturnValue()) {
        auto applePayModifier = WTFMove(std::get<1>(*modifierData));
        UNUSED_VARIABLE(applePayModifier);

#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
        request.setRecurringPaymentRequest(WTFMove(applePayModifier.recurringPaymentRequest));
#endif

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
        request.setAutomaticReloadPaymentRequest(WTFMove(applePayModifier.automaticReloadPaymentRequest));
#endif

#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
        request.setMultiTokenContexts(WTFMove(applePayModifier.multiTokenContexts));
#endif

#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
        request.setDeferredPaymentRequest(WTFMove(applePayModifier.deferredPaymentRequest));
#endif
    }

    constexpr OptionSet fieldsToValidate = {
        PaymentRequestValidator::Field::CurrencyCode,
        PaymentRequestValidator::Field::Total,
        PaymentRequestValidator::Field::ShippingMethods,
    };
    auto exception = PaymentRequestValidator::validate(request, fieldsToValidate);
    if (exception.hasException())
        return exception.releaseException();

    if (!paymentCoordinator().beginPaymentSession(document, *this, request))
        return Exception { ExceptionCode::AbortError };

    return { };
}

void ApplePayPaymentHandler::hide()
{
    paymentCoordinator().abortPaymentSession();
}

void ApplePayPaymentHandler::canMakePayment(Document&, Function<void(bool)>&& completionHandler)
{
    completionHandler(paymentCoordinator().canMakePayments());
}

ExceptionOr<Vector<ApplePayShippingMethod>> ApplePayPaymentHandler::computeShippingMethods() const
{
    auto& details = m_paymentRequest->paymentDetails();

    Vector<ApplePayShippingMethod> shippingOptions;
    if (details.shippingOptions) {
        auto& currency = details.total.amount.currency;

        shippingOptions.reserveInitialCapacity(details.shippingOptions->size());
        for (auto& shippingOption : *details.shippingOptions) {
            auto shippingMethodOrException = convertAndValidate(shippingOption, currency);
            if (shippingMethodOrException.hasException())
                return shippingMethodOrException.releaseException();

            auto shippingMethod = shippingMethodOrException.releaseReturnValue();

#if ENABLE(APPLE_PAY_SELECTED_SHIPPING_METHOD)
            if (shippingMethod.identifier == m_paymentRequest->shippingOption())
                shippingMethod.selected = true;
#endif

            shippingOptions.append(WTFMove(shippingMethod));
        }
    }

#if ENABLE(APPLE_PAY_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_LINE_ITEMS)
    auto modifierException = firstApplicableModifier();
    if (modifierException.hasException())
        return modifierException.releaseException();
    if (auto modifierData = modifierException.releaseReturnValue()) {
        auto applePayModifier = WTFMove(std::get<1>(*modifierData));

        shippingOptions.appendVector(WTFMove(applePayModifier.additionalShippingMethods));
    }
#endif

    return WTFMove(shippingOptions);
}

ExceptionOr<std::tuple<ApplePayLineItem, Vector<ApplePayLineItem>>> ApplePayPaymentHandler::computeTotalAndLineItems() const
{
    auto& details = m_paymentRequest->paymentDetails();
    auto& currency = details.total.amount.currency;

    auto convertedTotal = convertAndValidate(details.total, currency);
    if (convertedTotal.hasException())
        return convertedTotal.releaseException();
    auto total = convertedTotal.releaseReturnValue();

    Vector<ApplePayLineItem> lineItems;
    if (details.displayItems) {
        auto convertedLineItems = convertAndValidate(*details.displayItems, currency);
        if (convertedLineItems.hasException())
            return convertedLineItems.releaseException();
        lineItems = convertedLineItems.releaseReturnValue();
    }

    auto modifierException = firstApplicableModifier();
    if (modifierException.hasException())
        return modifierException.releaseException();
    if (auto modifierData = modifierException.releaseReturnValue()) {
        auto& [modifier, applePayModifier] = *modifierData;

        if (modifier.total) {
            auto totalOverride = convertAndValidate(*modifier.total, currency);
            if (totalOverride.hasException())
                return totalOverride.releaseException();
            total = totalOverride.releaseReturnValue();
        }

        auto additionalDisplayItems = convertAndValidate(modifier.additionalDisplayItems, currency);
        if (additionalDisplayItems.hasException())
            return additionalDisplayItems.releaseException();
        lineItems.appendVector(additionalDisplayItems.releaseReturnValue());

        if (applePayModifier.total)
            total = *applePayModifier.total;

        lineItems.appendVector(applePayModifier.additionalLineItems);
    }

    return {{ WTFMove(total), WTFMove(lineItems) }};
}

static inline void appendShippingContactInvalidError(String&& message, std::optional<ApplePayErrorContactField> contactField, Vector<RefPtr<ApplePayError>>& errors)
{
    if (!message.isNull())
        errors.append(ApplePayError::create(ApplePayErrorCode::ShippingContactInvalid, WTFMove(contactField), WTFMove(message)));
}

Vector<RefPtr<ApplePayError>> ApplePayPaymentHandler::computeErrors(String&& error, AddressErrors&& addressErrors, PayerErrorFields&& payerErrors, JSC::JSObject* paymentMethodErrors) const
{
    Vector<RefPtr<ApplePayError>> errors;

    auto& details = m_paymentRequest->paymentDetails();

    if (!details.shippingOptions || details.shippingOptions->isEmpty())
        computeAddressErrors(WTFMove(error), WTFMove(addressErrors), errors);

    computePayerErrors(WTFMove(payerErrors), errors);

    auto scope = DECLARE_CATCH_SCOPE(scriptExecutionContext()->vm());
    auto exception = computePaymentMethodErrors(paymentMethodErrors, errors);
    if (exception.hasException()) {
        ASSERT(scope.exception());
        scope.clearException();
    }

    return errors;
}

Vector<RefPtr<ApplePayError>> ApplePayPaymentHandler::computeErrors(JSC::JSObject* paymentMethodErrors) const
{
    Vector<RefPtr<ApplePayError>> errors;

    auto scope = DECLARE_CATCH_SCOPE(scriptExecutionContext()->vm());
    auto exception = computePaymentMethodErrors(paymentMethodErrors, errors);
    if (exception.hasException()) {
        ASSERT(scope.exception());
        scope.clearException();
    }

    return errors;
}

void ApplePayPaymentHandler::computeAddressErrors(String&& error, AddressErrors&& addressErrors, Vector<RefPtr<ApplePayError>>& errors) const
{
    if (!m_paymentRequest->paymentOptions().requestShipping)
        return;

    appendShippingContactInvalidError(WTFMove(error), std::nullopt, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.addressLine), ApplePayErrorContactField::AddressLines, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.city), ApplePayErrorContactField::Locality, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.country), ApplePayErrorContactField::Country, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.dependentLocality), ApplePayErrorContactField::SubLocality, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.phone), ApplePayErrorContactField::PhoneNumber, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.postalCode), ApplePayErrorContactField::PostalCode, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.recipient), ApplePayErrorContactField::Name, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.region), ApplePayErrorContactField::AdministrativeArea, errors);
}

void ApplePayPaymentHandler::computePayerErrors(PayerErrorFields&& payerErrors, Vector<RefPtr<ApplePayError>>& errors) const
{
    auto& options = m_paymentRequest->paymentOptions();

    if (options.requestPayerName)
        appendShippingContactInvalidError(WTFMove(payerErrors.name), ApplePayErrorContactField::Name, errors);

    if (options.requestPayerEmail)
        appendShippingContactInvalidError(WTFMove(payerErrors.email), ApplePayErrorContactField::EmailAddress, errors);

    if (options.requestPayerPhone)
        appendShippingContactInvalidError(WTFMove(payerErrors.phone), ApplePayErrorContactField::PhoneNumber, errors);
}

ExceptionOr<void> ApplePayPaymentHandler::computePaymentMethodErrors(JSC::JSObject* paymentMethodErrors, Vector<RefPtr<ApplePayError>>& errors) const
{
    if (!paymentMethodErrors)
        return { };

    auto& context = *scriptExecutionContext();
    auto throwScope = DECLARE_THROW_SCOPE(context.vm());
    auto applePayErrors = convert<IDLSequence<IDLInterface<ApplePayError>>>(*context.globalObject(), paymentMethodErrors);
    if (throwScope.exception())
        return Exception { ExceptionCode::ExistingExceptionError };

    for (auto&& applePayError : WTFMove(applePayErrors)) {
        if (applePayError)
            errors.append(WTFMove(applePayError));
    }

    return { };
}

static ExceptionOr<void> validate(const ApplePayModifier& applePayModifier)
{
#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    if (auto& recurringPaymentRequest = applePayModifier.recurringPaymentRequest) {
        auto& regularBilling = recurringPaymentRequest->regularBilling;
        if (regularBilling.paymentTiming != ApplePayPaymentTiming::Recurring)
            return Exception(ExceptionCode::TypeError, "'regularBilling' must be a 'recurring' line item."_s);
        if (!regularBilling.label)
            return Exception(ExceptionCode::TypeError, "Missing label for 'regularBilling'."_s);
        if (!isValidDecimalMonetaryValue(regularBilling.amount) && regularBilling.type != ApplePayLineItem::Type::Pending)
            return Exception(ExceptionCode::TypeError, makeString('"', regularBilling.amount, "\" is not a valid amount."));

        if (auto& trialBilling = recurringPaymentRequest->trialBilling) {
            if (trialBilling->paymentTiming != ApplePayPaymentTiming::Recurring)
                return Exception(ExceptionCode::TypeError, "'trialBilling' must be a 'recurring' line item."_s);
            if (!trialBilling->label)
                return Exception(ExceptionCode::TypeError, "Missing label for 'trialBilling'."_s);
            if (!isValidDecimalMonetaryValue(trialBilling->amount) && trialBilling->type != ApplePayLineItem::Type::Pending)
                return Exception(ExceptionCode::TypeError, makeString('"', trialBilling->amount, "\" is not a valid amount."));
        }

        if (auto& managementURL = recurringPaymentRequest->managementURL; !URL { managementURL }.isValid())
            return Exception(ExceptionCode::TypeError, makeString('"', managementURL, "\" is not a valid URL."));

        if (auto& tokenNotificationURL = recurringPaymentRequest->tokenNotificationURL; !tokenNotificationURL.isNull() && !URL { tokenNotificationURL }.isValid())
            return Exception(ExceptionCode::TypeError, makeString('"', tokenNotificationURL, "\" is not a valid URL."));
    }
#endif

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    if (auto& automaticReloadPaymentRequest = applePayModifier.automaticReloadPaymentRequest) {
        auto& automaticReloadBilling = automaticReloadPaymentRequest->automaticReloadBilling;
        if (automaticReloadBilling.paymentTiming != ApplePayPaymentTiming::AutomaticReload)
            return Exception(ExceptionCode::TypeError, "'automaticReloadBilling' must be an 'automaticReload' line item."_s);
        if (!automaticReloadBilling.label)
            return Exception(ExceptionCode::TypeError, "Missing label for 'automaticReloadBilling'."_s);
        if (!isValidDecimalMonetaryValue(automaticReloadBilling.amount) && automaticReloadBilling.type != ApplePayLineItem::Type::Pending)
            return Exception(ExceptionCode::TypeError, makeString('"', automaticReloadBilling.amount, "\" is not a valid amount."));
        if (!isValidDecimalMonetaryValue(automaticReloadBilling.automaticReloadPaymentThresholdAmount))
            return Exception(ExceptionCode::TypeError, makeString('"', automaticReloadBilling.automaticReloadPaymentThresholdAmount, "\" is not a valid automaticReloadPaymentThresholdAmount."));

        if (auto& managementURL = automaticReloadPaymentRequest->managementURL; !URL { managementURL }.isValid())
            return Exception(ExceptionCode::TypeError, makeString('"', managementURL, "\" is not a valid URL."));

        if (auto& tokenNotificationURL = automaticReloadPaymentRequest->tokenNotificationURL; !tokenNotificationURL.isNull() && !URL { tokenNotificationURL }.isValid())
            return Exception(ExceptionCode::TypeError, makeString('"', tokenNotificationURL, "\" is not a valid URL."));
    }
#endif

#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    if (auto& multiTokenContexts = applePayModifier.multiTokenContexts) {
        for (auto& tokenContext : *multiTokenContexts) {
            if (!isValidDecimalMonetaryValue(tokenContext.amount))
                return Exception(ExceptionCode::TypeError, makeString('"', tokenContext.amount, "\" is not a valid amount."));
        }
    }
#endif

#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
    if (const auto& deferredPaymentRequest = applePayModifier.deferredPaymentRequest) {
        if (auto validity = deferredPaymentRequest->validate(); validity.hasException())
            return validity.releaseException();
    }
#endif

    UNUSED_PARAM(applePayModifier);
    return { };
}

ExceptionOr<std::optional<std::tuple<PaymentDetailsModifier, ApplePayModifier>>> ApplePayPaymentHandler::firstApplicableModifier() const
{
    auto& details = m_paymentRequest->paymentDetails();
    if (!details.modifiers)
        return { std::nullopt };

    auto& lexicalGlobalObject = *document().globalObject();

    auto& serializedModifierData = m_paymentRequest->serializedModifierData();
    ASSERT(details.modifiers->size() == serializedModifierData.size());
    for (size_t i = 0; i < details.modifiers->size(); ++i) {
        auto& modifier = details.modifiers->at(i);

        auto convertedIdentifier = convertAndValidatePaymentMethodIdentifier(modifier.supportedMethods);
        if (!convertedIdentifier || !handlesIdentifier(*convertedIdentifier))
            continue;

        if (serializedModifierData[i].isEmpty())
            continue;

        auto scope = DECLARE_THROW_SCOPE(lexicalGlobalObject.vm());
        JSC::JSValue data;
        {
            JSC::JSLockHolder lock(&lexicalGlobalObject);
            data = JSONParse(&lexicalGlobalObject, serializedModifierData[i]);
            if (scope.exception())
                return Exception(ExceptionCode::ExistingExceptionError);
        }

        auto applePayModifier = convertDictionary<ApplePayModifier>(lexicalGlobalObject, WTFMove(data));
        if (scope.exception())
            return Exception(ExceptionCode::ExistingExceptionError);

        auto validateApplePayModifierResult = validate(applePayModifier);
        if (validateApplePayModifierResult.hasException())
            return validateApplePayModifierResult.releaseException();

        if (applePayModifier.paymentMethodType && *applePayModifier.paymentMethodType != m_selectedPaymentMethodType)
            continue;

        return { { { modifier, WTFMove(applePayModifier) } } };
    }

    return { std::nullopt };
}

ExceptionOr<void> ApplePayPaymentHandler::detailsUpdated(PaymentRequest::UpdateReason reason, String&& error, AddressErrors&& addressErrors, PayerErrorFields&& payerErrors, JSC::JSObject* paymentMethodErrors)
{
    using Reason = PaymentRequest::UpdateReason;
    switch (reason) {
    case Reason::ShowDetailsResolved:
        return { };
    case Reason::ShippingAddressChanged: {
        auto errors = computeErrors(WTFMove(error), WTFMove(addressErrors), WTFMove(payerErrors), paymentMethodErrors);
        // computeErrors() may run JavaScript, which may abort the request, so we need to make sure
        // sure we still have an active session.
        if (!paymentCoordinator().hasActiveSession())
            return Exception { ExceptionCode::InvalidStateError };
        return shippingAddressUpdated(WTFMove(errors));
    }
    case Reason::ShippingOptionChanged:
        return shippingOptionUpdated();
    case Reason::PaymentMethodChanged: {
        auto errors = computeErrors(WTFMove(error), WTFMove(addressErrors), WTFMove(payerErrors), paymentMethodErrors);
        // computeErrors() may run JavaScript, which may abort the request, so we need to make sure
        // sure we still have an active session.
        if (!paymentCoordinator().hasActiveSession())
            return Exception { ExceptionCode::InvalidStateError };
        return paymentMethodUpdated(WTFMove(errors));
    }
    }

    ASSERT_NOT_REACHED();
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::merchantValidationCompleted(JSC::JSValue&& merchantSessionValue)
{
    if (!paymentCoordinator().hasActiveSession())
        return Exception { ExceptionCode::InvalidStateError };

    if (!merchantSessionValue.isObject())
        return Exception { ExceptionCode::TypeError };

    String errorMessage;
    auto merchantSession = PaymentMerchantSession::fromJS(*document().globalObject(), asObject(merchantSessionValue), errorMessage);
    if (!merchantSession)
        return Exception { ExceptionCode::TypeError, WTFMove(errorMessage) };

    // PaymentMerchantSession::fromJS() may run JS, which may abort the request so we need to
    // check again if there is an active session.
    if (!paymentCoordinator().hasActiveSession())
        return Exception { ExceptionCode::InvalidStateError };

    paymentCoordinator().completeMerchantValidation(*merchantSession);
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::shippingAddressUpdated(Vector<RefPtr<ApplePayError>>&& errors)
{
    ASSERT(m_updateState == UpdateState::ShippingAddress);
    m_updateState = UpdateState::None;

    ApplePayShippingContactUpdate update;
    update.errors = WTFMove(errors);

    auto newShippingMethods = computeShippingMethods();
    if (newShippingMethods.hasException())
        return newShippingMethods.releaseException();
    update.newShippingMethods = newShippingMethods.releaseReturnValue();

    auto newTotalAndLineItems = computeTotalAndLineItems();
    if (newTotalAndLineItems.hasException())
        return newTotalAndLineItems.releaseException();
    std::tie(update.newTotal, update.newLineItems) = newTotalAndLineItems.releaseReturnValue();

    auto modifierException = firstApplicableModifier();
    if (modifierException.hasException())
        return modifierException.releaseException();
    if (auto modifierData = modifierException.releaseReturnValue()) {
        auto applePayModifier = WTFMove(std::get<1>(*modifierData));
        UNUSED_VARIABLE(applePayModifier);

#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
        update.newRecurringPaymentRequest = WTFMove(applePayModifier.recurringPaymentRequest);
#endif

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
        update.newAutomaticReloadPaymentRequest = WTFMove(applePayModifier.automaticReloadPaymentRequest);
#endif

#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
        update.newMultiTokenContexts = WTFMove(applePayModifier.multiTokenContexts);
#endif

#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
        update.newDeferredPaymentRequest = WTFMove(applePayModifier.deferredPaymentRequest);
#endif
    }

    paymentCoordinator().completeShippingContactSelection(WTFMove(update));
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::shippingOptionUpdated()
{
    ASSERT(m_updateState == UpdateState::ShippingOption);
    m_updateState = UpdateState::None;

    ApplePayShippingMethodUpdate update;

#if ENABLE(APPLE_PAY_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_LINE_ITEMS)
    auto newShippingMethods = computeShippingMethods();
    if (newShippingMethods.hasException())
        return newShippingMethods.releaseException();
    update.newShippingMethods = newShippingMethods.releaseReturnValue();
#endif

    auto newTotalAndLineItems = computeTotalAndLineItems();
    if (newTotalAndLineItems.hasException())
        return newTotalAndLineItems.releaseException();
    std::tie(update.newTotal, update.newLineItems) = newTotalAndLineItems.releaseReturnValue();

    auto modifierException = firstApplicableModifier();
    if (modifierException.hasException())
        return modifierException.releaseException();
    if (auto modifierData = modifierException.releaseReturnValue()) {
        auto applePayModifier = WTFMove(std::get<1>(*modifierData));
        UNUSED_VARIABLE(applePayModifier);

#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
        update.newRecurringPaymentRequest = WTFMove(applePayModifier.recurringPaymentRequest);
#endif

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
        update.newAutomaticReloadPaymentRequest = WTFMove(applePayModifier.automaticReloadPaymentRequest);
#endif

#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
        update.newMultiTokenContexts = WTFMove(applePayModifier.multiTokenContexts);
#endif

#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
        update.newDeferredPaymentRequest = WTFMove(applePayModifier.deferredPaymentRequest);
#endif
    }

    paymentCoordinator().completeShippingMethodSelection(WTFMove(update));
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::paymentMethodUpdated(Vector<RefPtr<ApplePayError>>&& errors)
{
#if ENABLE(APPLE_PAY_COUPON_CODE)
    if (m_updateState == UpdateState::CouponCode) {
        m_updateState = UpdateState::None;

        ApplePayCouponCodeUpdate update;
        update.errors = WTFMove(errors);

        auto newShippingMethods = computeShippingMethods();
        if (newShippingMethods.hasException())
            return newShippingMethods.releaseException();
        update.newShippingMethods = newShippingMethods.releaseReturnValue();

        auto newTotalAndLineItems = computeTotalAndLineItems();
        if (newTotalAndLineItems.hasException())
            return newTotalAndLineItems.releaseException();
        std::tie(update.newTotal, update.newLineItems) = newTotalAndLineItems.releaseReturnValue();

        auto modifierException = firstApplicableModifier();
        if (modifierException.hasException())
            return modifierException.releaseException();
        if (auto modifierData = modifierException.releaseReturnValue()) {
            auto applePayModifier = WTFMove(std::get<1>(*modifierData));
            UNUSED_VARIABLE(applePayModifier);

#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
            update.newRecurringPaymentRequest = WTFMove(applePayModifier.recurringPaymentRequest);
#endif

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
            update.newAutomaticReloadPaymentRequest = WTFMove(applePayModifier.automaticReloadPaymentRequest);
#endif

#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
            update.newMultiTokenContexts = WTFMove(applePayModifier.multiTokenContexts);
#endif

#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
            update.newDeferredPaymentRequest = WTFMove(applePayModifier.deferredPaymentRequest);
#endif
        }

        paymentCoordinator().completeCouponCodeChange(WTFMove(update));
        return { };
    }
#endif // ENABLE(APPLE_PAY_COUPON_CODE)

    ASSERT(m_updateState == UpdateState::PaymentMethod);
    m_updateState = UpdateState::None;

    ApplePayPaymentMethodUpdate update;

#if ENABLE(APPLE_PAY_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_LINE_ITEMS)
    update.errors = WTFMove(errors);

    auto newShippingMethods = computeShippingMethods();
    if (newShippingMethods.hasException())
        return newShippingMethods.releaseException();
    update.newShippingMethods = newShippingMethods.releaseReturnValue();
#else
    UNUSED_PARAM(errors);
#endif

    auto newTotalAndLineItems = computeTotalAndLineItems();
    if (newTotalAndLineItems.hasException())
        return newTotalAndLineItems.releaseException();
    std::tie(update.newTotal, update.newLineItems) = newTotalAndLineItems.releaseReturnValue();

    auto modifierException = firstApplicableModifier();
    if (modifierException.hasException())
        return modifierException.releaseException();
    if (auto modifierData = modifierException.releaseReturnValue()) {
        auto applePayModifier = WTFMove(std::get<1>(*modifierData));
        UNUSED_VARIABLE(applePayModifier);

#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
        update.newRecurringPaymentRequest = WTFMove(applePayModifier.recurringPaymentRequest);
#endif

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
        update.newAutomaticReloadPaymentRequest = WTFMove(applePayModifier.automaticReloadPaymentRequest);
#endif

#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
        update.newMultiTokenContexts = WTFMove(applePayModifier.multiTokenContexts);
#endif

#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
        update.newDeferredPaymentRequest = WTFMove(applePayModifier.deferredPaymentRequest);
#endif
    }

    paymentCoordinator().completePaymentMethodSelection(WTFMove(update));
    return { };
}

#if ENABLE(APPLE_PAY_PAYMENT_ORDER_DETAILS)

static ExceptionOr<ApplePayPaymentOrderDetails> convertAndValidate(ApplePayPaymentOrderDetails&& orderDetails)
{
    if (auto& webServiceURL = orderDetails.webServiceURL; !URL { webServiceURL }.isValid())
        return Exception(ExceptionCode::TypeError, makeString('"', webServiceURL, "\" is not a valid URL."));

    return WTFMove(orderDetails);
}

#endif // ENABLE(APPLE_PAY_PAYMENT_ORDER_DETAILS)

static ExceptionOr<ApplePayPaymentCompleteDetails> convertAndValidate(ApplePayPaymentCompleteDetails&& details)
{
#if ENABLE(APPLE_PAY_PAYMENT_ORDER_DETAILS)
    if (auto orderDetails = WTFMove(details.orderDetails)) {
        auto convertedOrderDetails = convertAndValidate(WTFMove(*orderDetails));
        if (convertedOrderDetails.hasException())
            return convertedOrderDetails.releaseException();
        details.orderDetails = convertedOrderDetails.releaseReturnValue();
    }
#endif

    return WTFMove(details);
}

ExceptionOr<void> ApplePayPaymentHandler::complete(Document& document, std::optional<PaymentComplete>&& result, String&& serializedData)
{
    ApplePayPaymentAuthorizationResult authorizationResult;

    switch (result.value_or(PaymentComplete::Success)) {
    case PaymentComplete::Fail:
    case PaymentComplete::Unknown:
        authorizationResult.status = ApplePayPaymentAuthorizationResult::Failure;
        break;

    case PaymentComplete::Success:
        authorizationResult.status = ApplePayPaymentAuthorizationResult::Success;
        break;
    }

    if (!serializedData.isEmpty()) {
        auto throwScope = DECLARE_THROW_SCOPE(document.vm());

        auto parsedData = JSONParse(document.globalObject(), WTFMove(serializedData));
        if (throwScope.exception())
            return Exception { ExceptionCode::ExistingExceptionError };

        auto details = convertDictionary<ApplePayPaymentCompleteDetails>(*document.globalObject(), WTFMove(parsedData));
        if (throwScope.exception())
            return Exception { ExceptionCode::ExistingExceptionError };

        auto convertedDetails = convertAndValidate(WTFMove(details));
        if (convertedDetails.hasException())
            return convertedDetails.releaseException();

        details = convertedDetails.releaseReturnValue();

#if ENABLE(APPLE_PAY_PAYMENT_ORDER_DETAILS)
        authorizationResult.orderDetails = details.orderDetails;
#endif
    }

    ASSERT(authorizationResult.isFinalState());
    paymentCoordinator().completePaymentSession(WTFMove(authorizationResult));
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::retry(PaymentValidationErrors&& validationErrors)
{
    Vector<RefPtr<ApplePayError>> errors;

    computeAddressErrors(WTFMove(validationErrors.error), WTFMove(validationErrors.shippingAddress), errors);
    computePayerErrors(WTFMove(validationErrors.payer), errors);

    auto exception = computePaymentMethodErrors(validationErrors.paymentMethod.get(), errors);
    if (exception.hasException())
        return exception.releaseException();

    // computePaymentMethodErrors() may run JS, which may abort the request so we need to
    // make sure we still have an active session.
    if (!paymentCoordinator().hasActiveSession())
        return Exception { ExceptionCode::AbortError };

    // Ensure there is always at least one error to avoid having a final result.
    if (errors.isEmpty())
        errors.append(ApplePayError::create(ApplePayErrorCode::Unknown, std::nullopt, nullString()));

    ApplePayPaymentAuthorizationResult authorizationResult;
    authorizationResult.status = ApplePayPaymentAuthorizationResult::Failure;
    authorizationResult.errors = WTFMove(errors);
    ASSERT(!authorizationResult.isFinalState());
    paymentCoordinator().completePaymentSession(WTFMove(authorizationResult));
    return { };
}

unsigned ApplePayPaymentHandler::version() const
{
    return m_applePayRequest ? m_applePayRequest->version : 0;
}

void ApplePayPaymentHandler::validateMerchant(URL&& validationURL)
{
    if (validationURL.isValid())
        m_paymentRequest->dispatchEvent(MerchantValidationEvent::create(eventNames().merchantvalidationEvent, std::get<URL>(m_identifier).string(), WTFMove(validationURL)).get());
}

static Ref<PaymentAddress> convert(const ApplePayPaymentContact& contact)
{
    return PaymentAddress::create(contact.countryCode, valueOrDefault(contact.addressLines), contact.administrativeArea, contact.locality, contact.subLocality, contact.postalCode, String(), String(), contact.localizedName, contact.phoneNumber);
}

template<typename T>
static JSC::Strong<JSC::JSObject> toJSDictionary(JSC::JSGlobalObject& lexicalGlobalObject, const T& value)
{
    JSC::JSLockHolder lock { &lexicalGlobalObject };
    return { lexicalGlobalObject.vm(), asObject(toJS<IDLDictionary<T>>(lexicalGlobalObject, *JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject), value)) };
}

void ApplePayPaymentHandler::didAuthorizePayment(const Payment& payment)
{
    ASSERT(m_updateState == UpdateState::None);

    auto applePayPayment = payment.toApplePayPayment(version());
    auto shippingContact = valueOrDefault(applePayPayment.shippingContact);
    auto detailsFunction = [applePayPayment = WTFMove(applePayPayment)](JSC::JSGlobalObject& lexicalGlobalObject) {
        return toJSDictionary(lexicalGlobalObject, applePayPayment);
    };

    m_paymentRequest->accept(std::get<URL>(m_identifier).string(), WTFMove(detailsFunction), convert(shippingContact), shippingContact.localizedName, shippingContact.emailAddress, shippingContact.phoneNumber);
}

void ApplePayPaymentHandler::didSelectShippingMethod(const ApplePayShippingMethod& shippingMethod)
{
    ASSERT(m_updateState == UpdateState::None);
    m_updateState = UpdateState::ShippingOption;

    m_paymentRequest->shippingOptionChanged(shippingMethod.identifier);
}

void ApplePayPaymentHandler::didSelectShippingContact(const PaymentContact& shippingContact)
{
    ASSERT(m_updateState == UpdateState::None);
    m_updateState = UpdateState::ShippingAddress;

    m_paymentRequest->shippingAddressChanged(convert(shippingContact.toApplePayPaymentContact(version())));
}

void ApplePayPaymentHandler::didSelectPaymentMethod(const PaymentMethod& paymentMethod)
{
    ASSERT(m_updateState == UpdateState::None);
    m_updateState = UpdateState::PaymentMethod;

    auto applePayPaymentMethod = paymentMethod.toApplePayPaymentMethod();
    m_selectedPaymentMethodType = applePayPaymentMethod.type;
    m_paymentRequest->paymentMethodChanged(std::get<URL>(m_identifier).string(), [applePayPaymentMethod = WTFMove(applePayPaymentMethod)](JSC::JSGlobalObject& lexicalGlobalObject) {
        return toJSDictionary(lexicalGlobalObject, applePayPaymentMethod);
    });
}

#if ENABLE(APPLE_PAY_COUPON_CODE)

void ApplePayPaymentHandler::didChangeCouponCode(String&& couponCode)
{
    ASSERT(m_updateState == UpdateState::None);
    m_updateState = UpdateState::CouponCode;

    ApplePayCouponCodeDetails applePayCouponCodeDetails { WTFMove(couponCode) };
    m_paymentRequest->paymentMethodChanged(std::get<URL>(m_identifier).string(), [applePayCouponCodeDetails = WTFMove(applePayCouponCodeDetails)] (JSC::JSGlobalObject& lexicalGlobalObject) {
        return toJSDictionary(lexicalGlobalObject, applePayCouponCodeDetails);
    });
}

#endif // ENABLE(APPLE_PAY_COUPON_CODE)

void ApplePayPaymentHandler::didCancelPaymentSession(PaymentSessionError&&)
{
    m_paymentRequest->cancel();
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)
