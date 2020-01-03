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
#include "ApplePayMerchantCapability.h"
#include "ApplePayModifier.h"
#include "ApplePayPayment.h"
#include "ApplePaySessionPaymentRequest.h"
#include "Document.h"
#include "EventNames.h"
#include "Frame.h"
#include "JSApplePayError.h"
#include "JSApplePayPayment.h"
#include "JSApplePayPaymentMethod.h"
#include "JSApplePayRequest.h"
#include "JSDOMConvert.h"
#include "LinkIconCollector.h"
#include "MerchantValidationEvent.h"
#include "Page.h"
#include "PayerErrorFields.h"
#include "Payment.h"
#include "PaymentAuthorizationStatus.h"
#include "PaymentContact.h"
#include "PaymentCoordinator.h"
#include "PaymentMerchantSession.h"
#include "PaymentMethod.h"
#include "PaymentMethodUpdate.h"
#include "PaymentRequestValidator.h"
#include "PaymentResponse.h"
#include "PaymentValidationErrors.h"
#include "Settings.h"
#include <JavaScriptCore/JSONObject.h>

namespace WebCore {

static ExceptionOr<ApplePayRequest> convertAndValidate(ScriptExecutionContext& context, JSC::JSValue data)
{
    if (data.isEmpty())
        return Exception { TypeError, "Missing payment method data." };

    auto throwScope = DECLARE_THROW_SCOPE(context.vm());
    auto applePayRequest = convertDictionary<ApplePayRequest>(*context.execState(), data);
    if (throwScope.exception())
        return Exception { ExistingExceptionError };

    return WTFMove(applePayRequest);
}

ExceptionOr<void> ApplePayPaymentHandler::validateData(Document& document, JSC::JSValue data)
{
    auto requestOrException = convertAndValidate(document, data);
    if (requestOrException.hasException())
        return requestOrException.releaseException();

    return { };
}

bool ApplePayPaymentHandler::handlesIdentifier(const PaymentRequest::MethodIdentifier& identifier)
{
    if (!WTF::holds_alternative<URL>(identifier))
        return false;

    auto& url = WTF::get<URL>(identifier);
    return url.host() == "apple.com" && url.path() == "/apple-pay";
}

static inline PaymentCoordinator& paymentCoordinator(Document& document)
{
    ASSERT(document.page());
    return document.page()->paymentCoordinator();
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
    return { WTFMove(lineItem) };
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

static ExceptionOr<ApplePaySessionPaymentRequest::ShippingMethod> convertAndValidate(const PaymentShippingOption& shippingOption, const String& expectedCurrency)
{
    auto exception = validate(shippingOption.amount, expectedCurrency);
    if (exception.hasException())
        return exception.releaseException();

    ApplePaySessionPaymentRequest::ShippingMethod result;
    result.amount = shippingOption.amount.value;
    result.label = shippingOption.label;
    result.identifier = shippingOption.id;
    return { WTFMove(result) };
}

ExceptionOr<void> ApplePayPaymentHandler::convertData(JSC::JSValue data)
{
    auto requestOrException = convertAndValidate(*scriptExecutionContext(), data);
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

    String expectedCurrency = m_paymentRequest->paymentDetails().total.amount.currency;
    request.setCurrencyCode(expectedCurrency);

    auto total = convertAndValidate(m_paymentRequest->paymentDetails().total, expectedCurrency);
    ASSERT(!total.hasException());
    request.setTotal(total.releaseReturnValue());

    auto convertedLineItems = convertAndValidate(m_paymentRequest->paymentDetails().displayItems, expectedCurrency);
    if (convertedLineItems.hasException())
        return convertedLineItems.releaseException();
    request.setLineItems(convertedLineItems.releaseReturnValue());

    mergePaymentOptions(m_paymentRequest->paymentOptions(), request);

    auto shippingMethods = computeShippingMethods();
    if (shippingMethods.hasException())
        return shippingMethods.releaseException();
    request.setShippingMethods(shippingMethods.releaseReturnValue());

    auto exception = PaymentRequestValidator::validate(request);
    if (exception.hasException())
        return exception.releaseException();

    if (!paymentCoordinator().beginPaymentSession(document, *this, request))
        return Exception { AbortError };

    return { };
}

void ApplePayPaymentHandler::hide()
{
    paymentCoordinator().abortPaymentSession();
}

void ApplePayPaymentHandler::canMakePayment(Document& document, Function<void(bool)>&& completionHandler)
{
    completionHandler(paymentCoordinator().canMakePayments(document));
}

ExceptionOr<Vector<ApplePaySessionPaymentRequest::ShippingMethod>> ApplePayPaymentHandler::computeShippingMethods()
{
    auto& details = m_paymentRequest->paymentDetails();
    auto& currency = details.total.amount.currency;
    auto& shippingOptions = details.shippingOptions;

    Vector<ApplePaySessionPaymentRequest::ShippingMethod> shippingMethods;
    shippingMethods.reserveInitialCapacity(shippingOptions.size());

    for (auto& shippingOption : shippingOptions) {
        auto shippingMethod = convertAndValidate(shippingOption, currency);
        if (shippingMethod.hasException())
            return shippingMethod.releaseException();
        shippingMethods.uncheckedAppend(shippingMethod.releaseReturnValue());
    }

    return WTFMove(shippingMethods);
}

ExceptionOr<ApplePaySessionPaymentRequest::TotalAndLineItems> ApplePayPaymentHandler::computeTotalAndLineItems() const
{
    auto& details = m_paymentRequest->paymentDetails();
    String currency = details.total.amount.currency;

    auto convertedTotal = convertAndValidate(details.total, currency);
    if (convertedTotal.hasException())
        return convertedTotal.releaseException();
    auto total = convertedTotal.releaseReturnValue();

    auto convertedLineItems = convertAndValidate(details.displayItems, currency);
    if (convertedLineItems.hasException())
        return convertedLineItems.releaseException();
    auto lineItems = convertedLineItems.releaseReturnValue();

    if (!m_selectedPaymentMethodType)
        return ApplePaySessionPaymentRequest::TotalAndLineItems { WTFMove(total), WTFMove(lineItems) };

    auto& modifiers = details.modifiers;
    auto& serializedModifierData = m_paymentRequest->serializedModifierData();
    ASSERT(modifiers.size() == serializedModifierData.size());
    for (size_t i = 0; i < modifiers.size(); ++i) {
        auto convertedIdentifier = convertAndValidatePaymentMethodIdentifier(modifiers[i].supportedMethods);
        if (!convertedIdentifier || !handlesIdentifier(*convertedIdentifier))
            continue;

        if (serializedModifierData[i].isEmpty())
            continue;

        auto& lexicalGlobalObject = *document().execState();
        auto scope = DECLARE_THROW_SCOPE(lexicalGlobalObject.vm());
        JSC::JSValue data;
        {
            auto lock = JSC::JSLockHolder { &lexicalGlobalObject };
            data = JSONParse(&lexicalGlobalObject, serializedModifierData[i]);
            if (scope.exception())
                return Exception { ExistingExceptionError };
        }

        auto applePayModifier = convertDictionary<ApplePayModifier>(lexicalGlobalObject, WTFMove(data));
        if (scope.exception())
            return Exception { ExistingExceptionError };

        if (applePayModifier.paymentMethodType != *m_selectedPaymentMethodType)
            continue;

        if (modifiers[i].total) {
            auto totalOverride = convertAndValidate(*modifiers[i].total, currency);
            if (totalOverride.hasException())
                return totalOverride.releaseException();
            total = totalOverride.releaseReturnValue();
        }

        auto additionalDisplayItems = convertAndValidate(modifiers[i].additionalDisplayItems, currency);
        if (additionalDisplayItems.hasException())
            return additionalDisplayItems.releaseException();
        lineItems.appendVector(additionalDisplayItems.releaseReturnValue());
        break;
    }

    return ApplePaySessionPaymentRequest::TotalAndLineItems { WTFMove(total), WTFMove(lineItems) };
}

static inline void appendShippingContactInvalidError(String&& message, Optional<PaymentError::ContactField> contactField, Vector<PaymentError>& errors)
{
    if (!message.isNull())
        errors.append({ PaymentError::Code::ShippingContactInvalid, WTFMove(message), WTFMove(contactField) });
}

Vector<PaymentError> ApplePayPaymentHandler::computeErrors(String&& error, AddressErrors&& addressErrors, PayerErrorFields&& payerErrors, JSC::JSObject* paymentMethodErrors) const
{
    Vector<PaymentError> errors;

    if (m_paymentRequest->paymentDetails().shippingOptions.isEmpty())
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

void ApplePayPaymentHandler::computeAddressErrors(String&& error, AddressErrors&& addressErrors, Vector<PaymentError>& errors) const
{
    if (!m_paymentRequest->paymentOptions().requestShipping)
        return;

    using ContactField = PaymentError::ContactField;
    appendShippingContactInvalidError(WTFMove(error), WTF::nullopt, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.addressLine), ContactField::AddressLines, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.city), ContactField::Locality, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.country), ContactField::Country, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.dependentLocality), ContactField::SubLocality, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.phone), ContactField::PhoneNumber, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.postalCode), ContactField::PostalCode, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.recipient), ContactField::Name, errors);
    appendShippingContactInvalidError(WTFMove(addressErrors.region), ContactField::AdministrativeArea, errors);
}

void ApplePayPaymentHandler::computePayerErrors(PayerErrorFields&& payerErrors, Vector<PaymentError>& errors) const
{
    auto& options = m_paymentRequest->paymentOptions();
    using ContactField = PaymentError::ContactField;

    if (options.requestPayerName)
        appendShippingContactInvalidError(WTFMove(payerErrors.name), ContactField::Name, errors);

    if (options.requestPayerEmail)
        appendShippingContactInvalidError(WTFMove(payerErrors.email), ContactField::EmailAddress, errors);

    if (options.requestPayerPhone)
        appendShippingContactInvalidError(WTFMove(payerErrors.phone), ContactField::PhoneNumber, errors);
}

ExceptionOr<void> ApplePayPaymentHandler::computePaymentMethodErrors(JSC::JSObject* paymentMethodErrors, Vector<PaymentError>& errors) const
{
    if (!paymentMethodErrors)
        return { };

#if ENABLE(APPLE_PAY_SESSION_V3)
    auto& context = *scriptExecutionContext();
    auto throwScope = DECLARE_THROW_SCOPE(context.vm());
    auto applePayErrors = convert<IDLSequence<IDLInterface<ApplePayError>>>(*context.execState(), paymentMethodErrors);
    if (throwScope.exception())
        return Exception { ExistingExceptionError };

    for (auto& applePayError : applePayErrors) {
        if (applePayError)
            errors.append({ applePayError->code(), applePayError->message(), applePayError->contactField() });
    }
#else
    UNUSED_PARAM(errors);
#endif

    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::detailsUpdated(PaymentRequest::UpdateReason reason, String&& error, AddressErrors&& addressErrors, PayerErrorFields&& payerErrors, JSC::JSObject* paymentMethodErrors)
{
    using Reason = PaymentRequest::UpdateReason;
    switch (reason) {
    case Reason::ShowDetailsResolved:
        return { };
    case Reason::ShippingAddressChanged:
        return shippingAddressUpdated(computeErrors(WTFMove(error), WTFMove(addressErrors), WTFMove(payerErrors), paymentMethodErrors));
    case Reason::ShippingOptionChanged:
        return shippingOptionUpdated();
    case Reason::PaymentMethodChanged:
        return paymentMethodUpdated();
    }

    ASSERT_NOT_REACHED();
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::merchantValidationCompleted(JSC::JSValue&& merchantSessionValue)
{
    if (!paymentCoordinator().hasActiveSession())
        return Exception { InvalidStateError };

    if (!merchantSessionValue.isObject())
        return Exception { TypeError };

    String errorMessage;
    auto merchantSession = PaymentMerchantSession::fromJS(*document().execState(), asObject(merchantSessionValue), errorMessage);
    if (!merchantSession)
        return Exception { TypeError, WTFMove(errorMessage) };

    paymentCoordinator().completeMerchantValidation(*merchantSession);
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::shippingAddressUpdated(Vector<PaymentError>&& errors)
{
    ASSERT(m_isUpdating);
    m_isUpdating = false;

    ShippingContactUpdate update;
    update.errors = WTFMove(errors);

    auto newShippingMethods = computeShippingMethods();
    if (newShippingMethods.hasException())
        return newShippingMethods.releaseException();
    update.newShippingMethods = newShippingMethods.releaseReturnValue();

    auto newTotalAndLineItems = computeTotalAndLineItems();
    if (newTotalAndLineItems.hasException())
        return newTotalAndLineItems.releaseException();
    update.newTotalAndLineItems = newTotalAndLineItems.releaseReturnValue();

    paymentCoordinator().completeShippingContactSelection(WTFMove(update));
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::shippingOptionUpdated()
{
    ASSERT(m_isUpdating);
    m_isUpdating = false;

    ShippingMethodUpdate update;

    auto newTotalAndLineItems = computeTotalAndLineItems();
    if (newTotalAndLineItems.hasException())
        return newTotalAndLineItems.releaseException();
    update.newTotalAndLineItems = newTotalAndLineItems.releaseReturnValue();

    paymentCoordinator().completeShippingMethodSelection(WTFMove(update));
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::paymentMethodUpdated()
{
    ASSERT(m_isUpdating);
    m_isUpdating = false;

    auto newTotalAndLineItems = computeTotalAndLineItems();
    if (newTotalAndLineItems.hasException())
        return newTotalAndLineItems.releaseException();

    paymentCoordinator().completePaymentMethodSelection(PaymentMethodUpdate { newTotalAndLineItems.releaseReturnValue() });
    return { };
}

void ApplePayPaymentHandler::complete(Optional<PaymentComplete>&& result)
{
    if (!result) {
        ASSERT(isFinalStateResult(WTF::nullopt));
        paymentCoordinator().completePaymentSession(WTF::nullopt);
        return;
    }

    PaymentAuthorizationResult authorizationResult;
    switch (*result) {
    case PaymentComplete::Fail:
    case PaymentComplete::Unknown:
        authorizationResult.status = PaymentAuthorizationStatus::Failure;
        break;
    case PaymentComplete::Success:
        authorizationResult.status = PaymentAuthorizationStatus::Success;
        break;
    }

    ASSERT(isFinalStateResult(authorizationResult));
    paymentCoordinator().completePaymentSession(WTFMove(authorizationResult));
}

ExceptionOr<void> ApplePayPaymentHandler::retry(PaymentValidationErrors&& validationErrors)
{
    Vector<PaymentError> errors;

    computeAddressErrors(WTFMove(validationErrors.error), WTFMove(validationErrors.shippingAddress), errors);
    computePayerErrors(WTFMove(validationErrors.payer), errors);

    auto exception = computePaymentMethodErrors(validationErrors.paymentMethod.get(), errors);
    if (exception.hasException())
        return exception.releaseException();

    // Ensure there is always at least one error to avoid having a final result.
    if (errors.isEmpty())
        errors.append({ PaymentError::Code::Unknown, { }, WTF::nullopt });

    PaymentAuthorizationResult authorizationResult { PaymentAuthorizationStatus::Failure, WTFMove(errors) };
    ASSERT(!isFinalStateResult(authorizationResult));
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
        m_paymentRequest->dispatchEvent(MerchantValidationEvent::create(eventNames().merchantvalidationEvent, WTF::get<URL>(m_identifier).string(), WTFMove(validationURL)).get());
}

static Ref<PaymentAddress> convert(const ApplePayPaymentContact& contact)
{
    return PaymentAddress::create(contact.countryCode, contact.addressLines.valueOr(Vector<String>()), contact.administrativeArea, contact.locality, contact.subLocality, contact.postalCode, String(), String(), contact.localizedName, contact.phoneNumber);
}

template<typename T>
static JSC::Strong<JSC::JSObject> toJSDictionary(JSC::JSGlobalObject& lexicalGlobalObject, const T& value)
{
    JSC::JSLockHolder lock { &lexicalGlobalObject };
    return { lexicalGlobalObject.vm(), asObject(toJS<IDLDictionary<T>>(lexicalGlobalObject, *JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject), value)) };
}

void ApplePayPaymentHandler::didAuthorizePayment(const Payment& payment)
{
    ASSERT(!m_isUpdating);

    auto applePayPayment = payment.toApplePayPayment(version());
    auto shippingContact = applePayPayment.shippingContact.valueOr(ApplePayPaymentContact());
    auto detailsFunction = [applePayPayment = WTFMove(applePayPayment)](JSC::JSGlobalObject& lexicalGlobalObject) {
        return toJSDictionary(lexicalGlobalObject, applePayPayment);
    };

    m_paymentRequest->accept(WTF::get<URL>(m_identifier).string(), WTFMove(detailsFunction), convert(shippingContact), shippingContact.localizedName, shippingContact.emailAddress, shippingContact.phoneNumber);
}

void ApplePayPaymentHandler::didSelectShippingMethod(const ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    ASSERT(!m_isUpdating);
    m_isUpdating = true;

    m_paymentRequest->shippingOptionChanged(shippingMethod.identifier);
}

void ApplePayPaymentHandler::didSelectShippingContact(const PaymentContact& shippingContact)
{
    ASSERT(!m_isUpdating);
    m_isUpdating = true;

    m_paymentRequest->shippingAddressChanged(convert(shippingContact.toApplePayPaymentContact(version())));
}

void ApplePayPaymentHandler::didSelectPaymentMethod(const PaymentMethod& paymentMethod)
{
    ASSERT(!m_isUpdating);
    m_isUpdating = true;

    auto applePayPaymentMethod = paymentMethod.toApplePayPaymentMethod();
    m_selectedPaymentMethodType = applePayPaymentMethod.type;
    m_paymentRequest->paymentMethodChanged(WTF::get<URL>(m_identifier).string(), [applePayPaymentMethod = WTFMove(applePayPaymentMethod)](JSC::JSGlobalObject& lexicalGlobalObject) {
        return toJSDictionary(lexicalGlobalObject, applePayPaymentMethod);
    });
}

void ApplePayPaymentHandler::didCancelPaymentSession(PaymentSessionError&&)
{
    m_paymentRequest->cancel();
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)
