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
#include "ApplePayModifier.h"
#include "ApplePayPayment.h"
#include "ApplePaySessionPaymentRequest.h"
#include "Document.h"
#include "EventNames.h"
#include "JSApplePayPayment.h"
#include "JSApplePayRequest.h"
#include "LinkIconCollector.h"
#include "MainFrame.h"
#include "MerchantValidationEvent.h"
#include "Page.h"
#include "Payment.h"
#include "PaymentAuthorizationStatus.h"
#include "PaymentContact.h"
#include "PaymentCoordinator.h"
#include "PaymentMerchantSession.h"
#include "PaymentMethod.h"
#include "PaymentRequestValidator.h"
#include "PaymentResponse.h"
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
    return { WTFMove(result) };
}

ExceptionOr<void> ApplePayPaymentHandler::convertData(JSC::JSValue&& data)
{
    auto& context = *scriptExecutionContext();
    auto throwScope = DECLARE_THROW_SCOPE(context.vm());
    auto applePayRequest = convertDictionary<ApplePayRequest>(*context.execState(), WTFMove(data));
    if (throwScope.exception())
        return Exception { ExistingExceptionError };

    m_applePayRequest = WTFMove(applePayRequest);
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::show()
{
    auto validatedRequest = convertAndValidate(m_applePayRequest->version, *m_applePayRequest, paymentCoordinator());
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
    for (auto& icon : LinkIconCollector { document() }.iconsOfTypes({ LinkIconType::TouchIcon, LinkIconType::TouchPrecomposedIcon }))
        linkIconURLs.append(icon.url);

    paymentCoordinator().beginPaymentSession(*this, document().url(), linkIconURLs, request);
    return { };
}

void ApplePayPaymentHandler::hide()
{
    paymentCoordinator().abortPaymentSession();
}

static bool shouldDiscloseApplePayCapability(Document& document)
{
    auto* page = document.page();
    if (!page || page->usesEphemeralSession())
        return false;

    return document.settings().applePayCapabilityDisclosureAllowed();
}

void ApplePayPaymentHandler::canMakePayment(Function<void(bool)>&& completionHandler)
{
    if (!shouldDiscloseApplePayCapability(document())) {
        completionHandler(paymentCoordinator().canMakePayments());
        return;
    }

    paymentCoordinator().canMakePaymentsWithActiveCard(m_applePayRequest->merchantIdentifier, document().domain(), WTFMove(completionHandler));
}

ExceptionOr<ApplePaySessionPaymentRequest::TotalAndLineItems> ApplePayPaymentHandler::computeTotalAndLineItems()
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

        auto& execState = *document().execState();
        auto scope = DECLARE_THROW_SCOPE(execState.vm());
        JSC::JSValue data;
        {
            auto lock = JSC::JSLockHolder { &execState };
            data = JSONParse(&execState, serializedModifierData[i]);
            if (scope.exception())
                return Exception { ExistingExceptionError };
        }

        auto applePayModifier = convertDictionary<ApplePayModifier>(execState, WTFMove(data));
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

ExceptionOr<void> ApplePayPaymentHandler::detailsUpdated(const AtomicString& eventType, const String& error)
{
    if (eventType == eventNames().shippingaddresschangeEvent)
        return shippingAddressUpdated(error);

    if (eventType == eventNames().shippingoptionchangeEvent)
        return shippingOptionUpdated();

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

ExceptionOr<void> ApplePayPaymentHandler::shippingAddressUpdated(const String& error)
{
    ShippingContactUpdate update;

    if (m_paymentRequest->paymentOptions().requestShipping && m_paymentRequest->paymentDetails().shippingOptions.isEmpty()) {
        PaymentError paymentError;
        paymentError.code = PaymentError::Code::ShippingContactInvalid;
        paymentError.message = error;
        update.errors.append(WTFMove(paymentError));
    }

    auto newTotalAndLineItems = computeTotalAndLineItems();
    if (newTotalAndLineItems.hasException())
        return newTotalAndLineItems.releaseException();
    update.newTotalAndLineItems = newTotalAndLineItems.releaseReturnValue();

    paymentCoordinator().completeShippingContactSelection(WTFMove(update));
    return { };
}

ExceptionOr<void> ApplePayPaymentHandler::shippingOptionUpdated()
{
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
    PaymentMethodUpdate update;

    auto newTotalAndLineItems = computeTotalAndLineItems();
    if (newTotalAndLineItems.hasException())
        return newTotalAndLineItems.releaseException();
    update.newTotalAndLineItems = newTotalAndLineItems.releaseReturnValue();

    paymentCoordinator().completePaymentMethodSelection(WTFMove(update));
    return { };
}

void ApplePayPaymentHandler::complete(std::optional<PaymentComplete>&& result)
{
    if (!result) {
        paymentCoordinator().completePaymentSession(std::nullopt);
        return;
    }

    PaymentAuthorizationResult authorizationResult;
    switch (*result) {
    case PaymentComplete::Fail:
    case PaymentComplete::Unknown:
        authorizationResult.status = PaymentAuthorizationStatus::Failure;
        authorizationResult.errors.append({ PaymentError::Code::Unknown, { }, std::nullopt });
        break;
    case PaymentComplete::Success:
        authorizationResult.status = PaymentAuthorizationStatus::Success;
        break;
    }

    paymentCoordinator().completePaymentSession(WTFMove(authorizationResult));
}

unsigned ApplePayPaymentHandler::version() const
{
    if (!m_applePayRequest)
        return 0;
    
    auto version = m_applePayRequest->version;
    ASSERT(paymentCoordinator().supportsVersion(version));
    return version;
}

void ApplePayPaymentHandler::validateMerchant(const URL& validationURL)
{
    if (validationURL.isValid())
        m_paymentRequest->dispatchEvent(MerchantValidationEvent::create(eventNames().merchantvalidationEvent, validationURL, m_paymentRequest.get()).get());
}

static Ref<PaymentAddress> convert(const ApplePayPaymentContact& contact)
{
    return PaymentAddress::create(contact.countryCode, contact.addressLines.value_or(Vector<String>()), contact.administrativeArea, contact.locality, contact.subLocality, contact.postalCode, String(), String(), String(), contact.localizedName, contact.phoneNumber);
}

void ApplePayPaymentHandler::didAuthorizePayment(const Payment& payment)
{
    auto applePayPayment = payment.toApplePayPayment(version());
    auto& execState = *document().execState();
    auto lock = JSC::JSLockHolder { &execState };
    auto details = JSC::Strong<JSC::JSObject> { execState.vm(), asObject(toJS<IDLDictionary<ApplePayPayment>>(execState, *JSC::jsCast<JSDOMGlobalObject*>(execState.lexicalGlobalObject()), applePayPayment)) };
    const auto& shippingContact = applePayPayment.shippingContact.value_or(ApplePayPaymentContact());
    m_paymentRequest->accept(WTF::get<URL>(m_identifier).string(), WTFMove(details), convert(shippingContact), shippingContact.localizedName, shippingContact.emailAddress, shippingContact.phoneNumber);
}

void ApplePayPaymentHandler::didSelectShippingMethod(const ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    m_paymentRequest->shippingOptionChanged(shippingMethod.identifier);
}

void ApplePayPaymentHandler::didSelectShippingContact(const PaymentContact& shippingContact)
{
    m_paymentRequest->shippingAddressChanged(convert(shippingContact.toApplePayPaymentContact(version())));
}

void ApplePayPaymentHandler::didSelectPaymentMethod(const PaymentMethod& paymentMethod)
{
    m_selectedPaymentMethodType = paymentMethod.toApplePayPaymentMethod().type;
    paymentMethodUpdated();
}

void ApplePayPaymentHandler::didCancelPaymentSession()
{
    m_paymentRequest->cancel();
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)
