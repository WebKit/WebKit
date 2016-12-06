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
#include "ApplePaySession.h"

#if ENABLE(APPLE_PAY)

#include "ApplePayPaymentAuthorizedEvent.h"
#include "ApplePayPaymentMethodSelectedEvent.h"
#include "ApplePayShippingContactSelectedEvent.h"
#include "ApplePayShippingMethodSelectedEvent.h"
#include "ApplePayValidateMerchantEvent.h"
#include "ArrayValue.h"
#include "DOMWindow.h"
#include "Dictionary.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "EventNames.h"
#include "JSDOMPromise.h"
#include "JSMainThreadExecState.h"
#include "LinkIconCollector.h"
#include "LinkIconType.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PaymentAuthorizationStatus.h"
#include "PaymentContact.h"
#include "PaymentCoordinator.h"
#include "PaymentMerchantSession.h"
#include "PaymentMethod.h"
#include "PaymentRequestValidator.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"

namespace WebCore {

static bool parseDigit(UChar digit, bool isNegative, int64_t& amount)
{
    if (!isASCIIDigit(digit))
        return false;

    int64_t digitValue = (digit - '0');

    const int64_t maxMultiplier = std::numeric_limits<int64_t>::max() / 10;

    // Check overflow.
    if (amount > maxMultiplier || (amount == maxMultiplier && digitValue > (std::numeric_limits<int64_t>::max() % 10) + isNegative))
        return false;

    amount = amount * 10 + digitValue;
    return true;
}

// The amount follows the regular expression -?[0-9]+(\.[0-9][0-9])?.
static std::optional<int64_t> parseAmount(const String& amountString)
{
    int64_t amount = 0;

    bool isNegative = false;

    enum class State {
        Start,
        Sign,
        Digit,
        Dot,
        DotDigit,
        End,
    };

    State state = State::Start;

    for (unsigned i = 0; i < amountString.length(); ++i) {
        UChar c = amountString[i];

        switch (state) {
        case State::Start:
            if (c == '-') {
                isNegative = true;
                state = State::Sign;
                break;
            }

            if (!parseDigit(c, isNegative, amount))
                return std::nullopt;
            state = State::Digit;
            break;

        case State::Sign:
            if (!parseDigit(c, isNegative, amount))
                return std::nullopt;
            state = State::Digit;
            break;

        case State::Digit:
            if (c == '.') {
                state = State::Dot;
                break;
            }

            if (!parseDigit(c, isNegative, amount))
                return std::nullopt;
            break;

        case State::Dot:
            if (!parseDigit(c, isNegative, amount))
                return std::nullopt;

            state = State::DotDigit;
            break;

        case State::DotDigit:
            if (!parseDigit(c, isNegative, amount))
                return std::nullopt;

            state = State::End;
            break;
            
        case State::End:
            return std::nullopt;
        }
    }
    
    if (state != State::Digit && state != State::DotDigit && state != State::End)
        return std::nullopt;

    if (state == State::DotDigit) {
        // There was a single digit after the decimal point.
        // FIXME: Handle this overflowing.
        amount *= 10;
    } else if (state == State::Digit) {
        // There was no decimal point.
        // FIXME: Handle this overflowing.
        amount *= 100;
    }

    if (isNegative)
        amount = -amount;

    return amount;
}

static std::optional<PaymentRequest::ContactFields> createContactFields(DOMWindow& window, const ArrayValue& contactFieldsArray)
{
    PaymentRequest::ContactFields result;

    size_t contactFieldsCount;
    if (!contactFieldsArray.length(contactFieldsCount))
        return std::nullopt;

    for (size_t i = 0; i < contactFieldsCount; ++i) {
        String contactField;
        if (!contactFieldsArray.get(i, contactField))
            return std::nullopt;

        if (contactField == "postalAddress")
            result.postalAddress = true;
        else if (contactField == "phone")
            result.phone = true;
        else if (contactField == "email")
            result.email = true;
        else if (contactField == "name")
            result.name = true;
        else {
            auto message = makeString("\"" + contactField, "\" is not a valid contact field.");
            window.printErrorMessage(message);
            return std::nullopt;
        }
    }

    return result;
}

static std::optional<PaymentRequest::LineItem::Type> toLineItemType(const String& type)
{
    if (type == "pending")
        return PaymentRequest::LineItem::Type::Pending;
    if (type == "final")
        return PaymentRequest::LineItem::Type::Final;

    return std::nullopt;
}

static bool isValidLineItemPropertyName(const String& propertyName)
{
    const char* validPropertyNames[] = {
        "type",
        "label",
        "amount",
    };

    for (auto& validPropertyName : validPropertyNames) {
        if (propertyName == validPropertyName)
            return true;
    }

    return false;
}

static std::optional<PaymentRequest::LineItem> createLineItem(DOMWindow& window, const Dictionary& total)
{
    Vector<String> propertyNames;
    total.getOwnPropertyNames(propertyNames);

    for (auto& propertyName : propertyNames) {
        if (!isValidLineItemPropertyName(propertyName)) {
            auto message = makeString("\"" + propertyName, "\" is not a valid line item property name.");
            window.printErrorMessage(message);
            return std::nullopt;
        }
    }

    // Line item type defaults to Final.
    PaymentRequest::LineItem result;

    if (auto typeString = total.get<String>("type")) {
        auto type = toLineItemType(*typeString);
        if (!type) {
            auto message = makeString("\"" + *typeString, "\" is not a valid line item type.");
            window.printErrorMessage(message);
            return std::nullopt;
        }

        result.type = *type;
    }

    if (auto label = total.get<String>("label"))
        result.label = *label;
    if (auto amountString = total.get<String>("amount")) {
        if (auto amount = parseAmount(*amountString))
            result.amount = *amount;
        else {
            auto message = makeString("\"" + *amountString, "\" is not a valid amount.");
            window.printErrorMessage(message);
            return std::nullopt;
        }
    }

    return result;
}

static std::optional<Vector<PaymentRequest::LineItem>> createLineItems(DOMWindow& window, const ArrayValue& lineItemsArray)
{
    Vector<PaymentRequest::LineItem> result;

    size_t lineItemCount;
    if (!lineItemsArray.length(lineItemCount))
        return std::nullopt;

    for (size_t i = 0; i < lineItemCount; ++i) {
        Dictionary lineItemDictionary;
        if (!lineItemsArray.get(i, lineItemDictionary))
            return std::nullopt;

        if (auto lineItem = createLineItem(window, lineItemDictionary))
            result.append(*lineItem);
    }

    return result;
}

static std::optional<PaymentRequest::MerchantCapabilities> createMerchantCapabilities(DOMWindow& window, const ArrayValue& merchantCapabilitiesArray)
{
    PaymentRequest::MerchantCapabilities result;

    size_t merchantCapabilitiesCount;
    if (!merchantCapabilitiesArray.length(merchantCapabilitiesCount))
        return std::nullopt;

    for (size_t i = 0; i < merchantCapabilitiesCount; ++i) {
        String merchantCapability;
        if (!merchantCapabilitiesArray.get(i, merchantCapability))
            return std::nullopt;

        if (merchantCapability == "supports3DS")
            result.supports3DS = true;
        else if (merchantCapability == "supportsEMV")
            result.supportsEMV = true;
        else if (merchantCapability == "supportsCredit")
            result.supportsCredit = true;
        else if (merchantCapability == "supportsDebit")
            result.supportsDebit = true;
        else {
            auto message = makeString("\"" + merchantCapability, "\" is not a valid merchant capability.");
            window.printErrorMessage(message);
            return std::nullopt;
        }
    }

    return result;
}

static std::optional<Vector<String>> createSupportedNetworks(unsigned version, DOMWindow& window, const ArrayValue& supportedNetworksArray)
{
    Vector<String> result;

    size_t supportedNetworksCount;
    if (!supportedNetworksArray.length(supportedNetworksCount))
        return std::nullopt;

    for (size_t i = 0; i < supportedNetworksCount; ++i) {
        String supportedNetwork;
        if (!supportedNetworksArray.get(i, supportedNetwork))
            return std::nullopt;

        if (!PaymentRequest::isValidSupportedNetwork(version, supportedNetwork)) {
            auto message = makeString("\"" + supportedNetwork, "\" is not a valid payment network.");
            window.printErrorMessage(message);
            return std::nullopt;
        }

        result.append(WTFMove(supportedNetwork));
    }

    return result;
}

static std::optional<PaymentRequest::ShippingType> toShippingType(const String& shippingTypeString)
{
    if (shippingTypeString == "shipping")
        return PaymentRequest::ShippingType::Shipping;
    if (shippingTypeString == "delivery")
        return PaymentRequest::ShippingType::Delivery;
    if (shippingTypeString == "storePickup")
        return PaymentRequest::ShippingType::StorePickup;
    if (shippingTypeString == "servicePickup")
        return PaymentRequest::ShippingType::ServicePickup;

    return std::nullopt;
}

static bool isValidShippingMethodPropertyName(const String& propertyName)
{
    const char* validPropertyNames[] = {
        "label",
        "detail",
        "amount",
        "identifier",
    };

    for (auto& validPropertyName : validPropertyNames) {
        if (propertyName == validPropertyName)
            return true;
    }

    return false;
}

static std::optional<PaymentRequest::ShippingMethod> createShippingMethod(DOMWindow& window, const Dictionary& shippingMethodDictionary)
{
    Vector<String> propertyNames;
    shippingMethodDictionary.getOwnPropertyNames(propertyNames);

    for (auto& propertyName : propertyNames) {
        if (!isValidShippingMethodPropertyName(propertyName)) {
            auto message = makeString("\"" + propertyName, "\" is not a valid shipping method property name.");
            window.printErrorMessage(message);
            return std::nullopt;
        }
    }

    PaymentRequest::ShippingMethod result;

    auto label = shippingMethodDictionary.get<String>("label");
    if (!label) {
        window.printErrorMessage("Missing shipping method label.");
        return std::nullopt;
    }
    result.label = *label;

    auto detail = shippingMethodDictionary.get<String>("detail");
    if (!detail) {
        window.printErrorMessage("Missing shipping method detail.");
        return std::nullopt;
    }
    result.detail = *detail;

    auto amountString = shippingMethodDictionary.get<String>("amount");
    if (!amountString) {
        window.printErrorMessage("Missing shipping method amount.");
        return std::nullopt;
    }

    if (auto amount = parseAmount(*amountString))
        result.amount = *amount;
    else {
        auto message = makeString("\"" + *amountString, "\" is not a valid amount.");
        window.printErrorMessage(message);
        return std::nullopt;
    }

    auto identifier = shippingMethodDictionary.get<String>("identifier");
    if (!identifier) {
        window.printErrorMessage("Missing shipping method identifier.");
        return std::nullopt;
    }
    result.identifier = *identifier;

    return result;
}

static std::optional<Vector<PaymentRequest::ShippingMethod>> createShippingMethods(DOMWindow& window, const ArrayValue& shippingMethodsArray)
{
    Vector<PaymentRequest::ShippingMethod> result;

    size_t shippingMethodCount;
    if (!shippingMethodsArray.length(shippingMethodCount))
        return std::nullopt;

    for (size_t i = 0; i < shippingMethodCount; ++i) {
        Dictionary shippingMethodDictionary;
        if (!shippingMethodsArray.get(i, shippingMethodDictionary))
            return std::nullopt;

        if (auto shippingMethod = createShippingMethod(window, shippingMethodDictionary))
            result.append(*shippingMethod);
        else
            return std::nullopt;
    }

    return result;
}

static bool isValidPaymentRequestPropertyName(const String& propertyName)
{
    const char* validPropertyNames[] = {
        "merchantCapabilities",
        "supportedNetworks",
        "countryCode",
        "currencyCode",
        "requiredBillingContactFields",
        "billingContact",
        "requiredShippingContactFields",
        "shippingContact",
        "shippingType",
        "shippingMethods",
        "total",
        "lineItems",
        "applicationData",
    };

    for (auto& validPropertyName : validPropertyNames) {
        if (propertyName == validPropertyName)
            return true;
    }

    return false;
}

static std::optional<PaymentRequest> createPaymentRequest(unsigned version, DOMWindow& window, const Dictionary& dictionary)
{
    PaymentRequest paymentRequest;

    Vector<String> propertyNames;
    dictionary.getOwnPropertyNames(propertyNames);

    for (auto& propertyName : propertyNames) {
        if (propertyName == "requiredShippingAddressFields") {
            window.printErrorMessage("\"requiredShippingAddressFields\" has been deprecated. Please switch to \"requiredShippingContactFields\" instead.");
            return std::nullopt;
        }

        if (propertyName == "requiredBillingAddressFields") {
            window.printErrorMessage("\"requiredBillingAddressFields\" has been deprecated. Please switch to \"requiredBillingContactFields\" instead.");
            return std::nullopt;
        }

        if (!isValidPaymentRequestPropertyName(propertyName)) {
            auto message = makeString("\"" + propertyName, "\" is not a valid payment request property name.");
            window.printErrorMessage(message);
            return std::nullopt;
        }
    }

    if (auto merchantCapabilitiesArray = dictionary.get<ArrayValue>("merchantCapabilities")) {
        auto merchantCapabilities = createMerchantCapabilities(window, *merchantCapabilitiesArray);
        if (!merchantCapabilities)
            return std::nullopt;

        paymentRequest.setMerchantCapabilities(*merchantCapabilities);
    }

    if (auto supportedNetworksArray = dictionary.get<ArrayValue>("supportedNetworks")) {
        auto supportedNetworks = createSupportedNetworks(version, window, *supportedNetworksArray);
        if (!supportedNetworks)
            return std::nullopt;

        paymentRequest.setSupportedNetworks(*supportedNetworks);
    }

    if (auto countryCode = dictionary.get<String>("countryCode"))
        paymentRequest.setCountryCode(*countryCode);
    if (auto currencyCode = dictionary.get<String>("currencyCode"))
        paymentRequest.setCurrencyCode(*currencyCode);

    if (auto requiredBillingContactFieldsArray = dictionary.get<ArrayValue>("requiredBillingContactFields")) {
        auto requiredBillingContactFields = createContactFields(window, *requiredBillingContactFieldsArray);
        if (!requiredBillingContactFields)
            return std::nullopt;

        paymentRequest.setRequiredBillingContactFields(*requiredBillingContactFields);
    }

    if (auto billingContactValue = dictionary.get<JSC::JSValue>("billingContact")) {
        String errorMessage;
        auto billingContact = PaymentContact::fromJS(*JSMainThreadExecState::currentState(), *billingContactValue, errorMessage);
        if (!billingContact) {
            window.printErrorMessage(errorMessage);
            return std::nullopt;
        }

        paymentRequest.setBillingContact(*billingContact);
    }

    if (auto requiredShippingContactFieldsArray = dictionary.get<ArrayValue>("requiredShippingContactFields")) {
        auto requiredShippingContactFields = createContactFields(window, *requiredShippingContactFieldsArray);
        if (!requiredShippingContactFields)
            return std::nullopt;

        paymentRequest.setRequiredShippingContactFields(*requiredShippingContactFields);
    }

    if (auto shippingContactValue = dictionary.get<JSC::JSValue>("shippingContact")) {
        String errorMessage;
        auto shippingContact = PaymentContact::fromJS(*JSMainThreadExecState::currentState(), *shippingContactValue, errorMessage);
        if (!shippingContact) {
            window.printErrorMessage(errorMessage);
            return std::nullopt;
        }

        paymentRequest.setShippingContact(*shippingContact);
    }

    if (auto shippingTypeString = dictionary.get<String>("shippingType")) {
        auto shippingType = toShippingType(*shippingTypeString);

        if (!shippingType) {
            auto message = makeString("\"" + *shippingTypeString, "\" is not a valid shipping type.");
            window.printErrorMessage(message);
            return std::nullopt;
        }
        paymentRequest.setShippingType(*shippingType);
    }

    if (auto shippingMethodsArray = dictionary.get<ArrayValue>("shippingMethods")) {
        auto shippingMethods = createShippingMethods(window, *shippingMethodsArray);
        if (!shippingMethods)
            return std::nullopt;

        paymentRequest.setShippingMethods(*shippingMethods);
    }

    if (auto totalDictionary = dictionary.get<Dictionary>("total")) {
        auto total = createLineItem(window, *totalDictionary);
        if (!total)
            return std::nullopt;

        paymentRequest.setTotal(*total);
    }

    if (auto lineItemsArray = dictionary.get<ArrayValue>("lineItems")) {
        if (auto lineItems = createLineItems(window, *lineItemsArray))
            paymentRequest.setLineItems(*lineItems);
    }

    if (auto applicationData = dictionary.get<String>("applicationData"))
        paymentRequest.setApplicationData(*applicationData);

    return paymentRequest;
}

static bool isSecure(DocumentLoader& documentLoader)
{
    if (!documentLoader.response().url().protocolIs("https"))
        return false;

    if (!documentLoader.response().certificateInfo() || documentLoader.response().certificateInfo()->containsNonRootSHA1SignedCertificate())
        return false;

    return true;
}

static bool canCallApplePaySessionAPIs(Document& document, String& errorMessage)
{
    if (!isSecure(*document.loader())) {
        errorMessage = "Trying to call an ApplePaySession API from an insecure document.";
        return false;
    }

    auto& topDocument = document.topDocument();
    if (&document != &topDocument) {
        auto& topOrigin = *topDocument.topOrigin();

        if (!document.securityOrigin()->isSameSchemeHostPort(&topOrigin)) {
            errorMessage = "Trying to call an ApplePaySession API from a document with an different security origin than its top-level frame.";
            return false;
        }

        for (auto* ancestorDocument = document.parentDocument(); ancestorDocument != &topDocument; ancestorDocument = ancestorDocument->parentDocument()) {
            if (!isSecure(*ancestorDocument->loader())) {
                errorMessage = "Trying to call an ApplePaySession API from a document with an insecure parent frame.";
                return false;
            }

            if (!ancestorDocument->securityOrigin()->isSameSchemeHostPort(&topOrigin)) {
                errorMessage = "Trying to call an ApplePaySession API from a document with an different security origin than its top-level frame.";
                return false;
            }
        }
    }

    return true;
}

ExceptionOr<Ref<ApplePaySession>> ApplePaySession::create(Document& document, unsigned version, const Dictionary& dictionary)
{
    DOMWindow& window = *document.domWindow();

    String errorMessage;
    if (!canCallApplePaySessionAPIs(document, errorMessage)) {
        window.printErrorMessage(errorMessage);
        return Exception { INVALID_ACCESS_ERR };
    }

    if (!ScriptController::processingUserGesture()) {
        window.printErrorMessage("Must create a new ApplePaySession from a user gesture handler.");
        return Exception { INVALID_ACCESS_ERR };
    }

    auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();

    if (!version || !paymentCoordinator.supportsVersion(version)) {
        window.printErrorMessage(makeString("\"" + String::number(version), "\" is not a supported version."));
        return Exception { INVALID_ACCESS_ERR };
    }

    auto paymentRequest = createPaymentRequest(version, window, dictionary);
    if (!paymentRequest)
        return Exception { TYPE_MISMATCH_ERR };

    if (!PaymentRequestValidator(window).validate(*paymentRequest))
        return Exception { INVALID_ACCESS_ERR };

    return adoptRef(*new ApplePaySession(document, WTFMove(*paymentRequest)));
}

ApplePaySession::ApplePaySession(Document& document, PaymentRequest&& paymentRequest)
    : ActiveDOMObject(&document)
    , m_paymentRequest(WTFMove(paymentRequest))
{
    suspendIfNeeded();
}

ApplePaySession::~ApplePaySession()
{
}

ExceptionOr<bool> ApplePaySession::supportsVersion(ScriptExecutionContext& scriptExecutionContext, unsigned version)
{
    if (!version)
        return Exception { INVALID_ACCESS_ERR };

    auto& document = downcast<Document>(scriptExecutionContext);
    DOMWindow& window = *document.domWindow();

    String errorMessage;
    if (!canCallApplePaySessionAPIs(document, errorMessage)) {
        window.printErrorMessage(errorMessage);
        return Exception { INVALID_ACCESS_ERR };
    }

    return document.frame()->mainFrame().paymentCoordinator().supportsVersion(version);
}

static bool shouldDiscloseApplePayCapability(Document& document)
{
    auto* page = document.page();
    if (!page || page->usesEphemeralSession())
        return false;

    return document.frame()->settings().applePayCapabilityDisclosureAllowed();
}

ExceptionOr<bool> ApplePaySession::canMakePayments(ScriptExecutionContext& scriptExecutionContext)
{
    auto& document = downcast<Document>(scriptExecutionContext);
    DOMWindow& window = *document.domWindow();

    String errorMessage;
    if (!canCallApplePaySessionAPIs(document, errorMessage)) {
        window.printErrorMessage(errorMessage);
        return Exception { INVALID_ACCESS_ERR };
    }

    return document.frame()->mainFrame().paymentCoordinator().canMakePayments();
}

ExceptionOr<void> ApplePaySession::canMakePaymentsWithActiveCard(ScriptExecutionContext& scriptExecutionContext, const String& merchantIdentifier, Ref<DeferredPromise>&& passedPromise)
{
    auto& document = downcast<Document>(scriptExecutionContext);
    DOMWindow& window = *document.domWindow();

    String errorMessage;
    if (!canCallApplePaySessionAPIs(document, errorMessage)) {
        window.printErrorMessage(errorMessage);
        return Exception { INVALID_ACCESS_ERR };
    }

    RefPtr<DeferredPromise> promise(WTFMove(passedPromise));
    if (!shouldDiscloseApplePayCapability(document)) {
        auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();
        bool canMakePayments = paymentCoordinator.canMakePayments();

        RunLoop::main().dispatch([promise, canMakePayments]() mutable {
            promise->resolve<IDLBoolean>(canMakePayments);
        });
        return { };
    }

    auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();

    paymentCoordinator.canMakePaymentsWithActiveCard(merchantIdentifier, document.domain(), [promise](bool canMakePayments) mutable {
        promise->resolve<IDLBoolean>(canMakePayments);
    });
    return { };
}

ExceptionOr<void> ApplePaySession::openPaymentSetup(ScriptExecutionContext& scriptExecutionContext, const String& merchantIdentifier, Ref<DeferredPromise>&& passedPromise)
{
    auto& document = downcast<Document>(scriptExecutionContext);
    DOMWindow& window = *document.domWindow();

    String errorMessage;
    if (!canCallApplePaySessionAPIs(document, errorMessage)) {
        window.printErrorMessage(errorMessage);
        return Exception { INVALID_ACCESS_ERR };
    }

    if (!ScriptController::processingUserGesture()) {
        window.printErrorMessage("Must call ApplePaySession.openPaymentSetup from a user gesture handler.");
        return Exception { INVALID_ACCESS_ERR };
    }

    RefPtr<DeferredPromise> promise(WTFMove(passedPromise));
    auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();

    paymentCoordinator.openPaymentSetup(merchantIdentifier, document.domain(), [promise](bool result) mutable {
        promise->resolve<IDLBoolean>(result);
    });

    return { };
}

ExceptionOr<void> ApplePaySession::begin()
{
    auto& document = *downcast<Document>(scriptExecutionContext());
    auto& window = *document.domWindow();

    if (!canBegin()) {
        window.printErrorMessage("Payment session is already active.");
        return Exception { INVALID_ACCESS_ERR };
    }

    if (paymentCoordinator().hasActiveSession()) {
        window.printErrorMessage("Page already has an active payment session.");
        return Exception { INVALID_ACCESS_ERR };
    }

    Vector<URL> linkIconURLs;
    for (auto& icon : LinkIconCollector { document }.iconsOfTypes({ LinkIconType::TouchIcon, LinkIconType::TouchPrecomposedIcon }))
        linkIconURLs.append(icon.url);

    if (!paymentCoordinator().beginPaymentSession(*this, document.url(), linkIconURLs, m_paymentRequest)) {
        window.printErrorMessage("There is already has an active payment session.");
        return Exception { INVALID_ACCESS_ERR };
    }

    m_state = State::Active;

    setPendingActivity(this);

    return { };
}

ExceptionOr<void> ApplePaySession::abort()
{
    if (!canAbort())
        return Exception { INVALID_ACCESS_ERR };

    m_state = State::Aborted;
    paymentCoordinator().abortPaymentSession();

    didReachFinalState();

    return { };
}

ExceptionOr<void> ApplePaySession::completeMerchantValidation(const Dictionary& merchantSessionDictionary)
{
    if (!canCompleteMerchantValidation())
        return Exception { INVALID_ACCESS_ERR };

    if (!merchantSessionDictionary.initializerObject())
        return Exception { TypeError };

    auto& document = *downcast<Document>(scriptExecutionContext());
    auto& window = *document.domWindow();

    String errorMessage;
    auto merchantSession = PaymentMerchantSession::fromJS(*merchantSessionDictionary.execState(), merchantSessionDictionary.initializerObject(), errorMessage);
    if (!merchantSession) {
        window.printErrorMessage(errorMessage);
        return Exception { INVALID_ACCESS_ERR };
    }

    m_merchantValidationState = MerchantValidationState::ValidationComplete;
    paymentCoordinator().completeMerchantValidation(*merchantSession);

    return { };
}

static std::optional<PaymentAuthorizationStatus> toPaymentAuthorizationStatus(unsigned short status)
{
    switch (status) {
    case ApplePaySession::STATUS_SUCCESS:
        return PaymentAuthorizationStatus::Success;

    case ApplePaySession::STATUS_FAILURE:
        return PaymentAuthorizationStatus::Failure;

    case ApplePaySession::STATUS_INVALID_BILLING_POSTAL_ADDRESS:
        return PaymentAuthorizationStatus::InvalidBillingPostalAddress;

    case ApplePaySession::STATUS_INVALID_SHIPPING_POSTAL_ADDRESS:
        return PaymentAuthorizationStatus::InvalidShippingPostalAddress;

    case ApplePaySession::STATUS_INVALID_SHIPPING_CONTACT:
        return PaymentAuthorizationStatus::InvalidShippingContact;

    case ApplePaySession::STATUS_PIN_REQUIRED:
        return PaymentAuthorizationStatus::PINRequired;

    case ApplePaySession::STATUS_PIN_INCORRECT:
        return PaymentAuthorizationStatus::PINIncorrect;

    case ApplePaySession::STATUS_PIN_LOCKOUT:
        return PaymentAuthorizationStatus::PINLockout;

    default:
        return std::nullopt;
    }
}

ExceptionOr<void> ApplePaySession::completeShippingMethodSelection(unsigned short status, const Dictionary& newTotalDictionary, const ArrayValue& newLineItemsArray)
{
    if (!canCompleteShippingMethodSelection())
        return Exception { INVALID_ACCESS_ERR };

    auto authorizationStatus = toPaymentAuthorizationStatus(status);
    if (!authorizationStatus)
        return Exception { INVALID_ACCESS_ERR };

    auto& window = *downcast<Document>(scriptExecutionContext())->domWindow();
    auto newTotal = createLineItem(window, newTotalDictionary);
    if (!newTotal)
        return Exception { INVALID_ACCESS_ERR };

    if (!PaymentRequestValidator(window).validateTotal(*newTotal))
        return Exception { INVALID_ACCESS_ERR };

    auto newLineItems = createLineItems(window, newLineItemsArray);
    if (!newLineItems)
        return Exception { INVALID_ACCESS_ERR };

    m_state = State::Active;
    PaymentRequest::TotalAndLineItems totalAndLineItems;
    totalAndLineItems.total = *newTotal;
    totalAndLineItems.lineItems = *newLineItems;
    paymentCoordinator().completeShippingMethodSelection(*authorizationStatus, totalAndLineItems);

    return { };
}

ExceptionOr<void> ApplePaySession::completeShippingContactSelection(unsigned short status, const ArrayValue& newShippingMethodsArray, const Dictionary& newTotalDictionary, const ArrayValue& newLineItemsArray)
{
    if (!canCompleteShippingContactSelection())
        return Exception { INVALID_ACCESS_ERR };

    auto authorizationStatus = toPaymentAuthorizationStatus(status);
    if (!authorizationStatus)
        return Exception { INVALID_ACCESS_ERR };

    auto& window = *downcast<Document>(scriptExecutionContext())->domWindow();

    auto newShippingMethods = createShippingMethods(window, newShippingMethodsArray);
    if (!newShippingMethods)
        return Exception { INVALID_ACCESS_ERR };

    auto newTotal = createLineItem(window, newTotalDictionary);
    if (!newTotal)
        return Exception { INVALID_ACCESS_ERR };

    if (!PaymentRequestValidator(window).validateTotal(*newTotal))
        return Exception { INVALID_ACCESS_ERR };

    auto newLineItems = createLineItems(window, newLineItemsArray);
    if (!newLineItems)
        return Exception { INVALID_ACCESS_ERR };

    m_state = State::Active;
    PaymentRequest::TotalAndLineItems totalAndLineItems;
    totalAndLineItems.total = *newTotal;
    totalAndLineItems.lineItems = *newLineItems;
    paymentCoordinator().completeShippingContactSelection(*authorizationStatus, *newShippingMethods, totalAndLineItems);

    return { };
}

ExceptionOr<void> ApplePaySession::completePaymentMethodSelection(const Dictionary& newTotalDictionary, const ArrayValue& newLineItemsArray)
{
    if (!canCompletePaymentMethodSelection())
        return Exception { INVALID_ACCESS_ERR };

    auto& window = *downcast<Document>(*scriptExecutionContext()).domWindow();
    auto newTotal = createLineItem(window, newTotalDictionary);
    if (!newTotal)
        return Exception { INVALID_ACCESS_ERR };

    if (!PaymentRequestValidator(window).validateTotal(*newTotal))
        return Exception { INVALID_ACCESS_ERR };

    auto newLineItems = createLineItems(window, newLineItemsArray);
    if (!newLineItems)
        return Exception { INVALID_ACCESS_ERR };

    m_state = State::Active;
    PaymentRequest::TotalAndLineItems totalAndLineItems;
    totalAndLineItems.total = *newTotal;
    totalAndLineItems.lineItems = *newLineItems;
    paymentCoordinator().completePaymentMethodSelection(totalAndLineItems);

    return { };
}

ExceptionOr<void> ApplePaySession::completePayment(unsigned short status)
{
    if (!canCompletePayment())
        return Exception { INVALID_ACCESS_ERR };

    auto authorizationStatus = toPaymentAuthorizationStatus(status);
    if (!authorizationStatus)
        return Exception { INVALID_ACCESS_ERR };

    paymentCoordinator().completePaymentSession(*authorizationStatus);

    if (!isFinalStateStatus(*authorizationStatus)) {
        m_state = State::Active;
        return { };
    }

    m_state = State::Completed;
    unsetPendingActivity(this);
    return { };
}

void ApplePaySession::validateMerchant(const URL& validationURL)
{
    if (m_state == State::Aborted) {
        // ApplePaySession::abort has been called.
        return;
    }

    ASSERT(m_merchantValidationState == MerchantValidationState::Idle);
    ASSERT(m_state == State::Active);

    if (validationURL.isNull()) {
        // Something went wrong when getting the validation URL.
        // FIXME: Maybe we should send an error event here instead?
        return;
    }

    m_merchantValidationState = MerchantValidationState::ValidatingMerchant;

    auto event = ApplePayValidateMerchantEvent::create(eventNames().validatemerchantEvent, validationURL);
    dispatchEvent(event.get());
}

void ApplePaySession::didAuthorizePayment(const Payment& payment)
{
    ASSERT(m_state == State::Active);

    m_state = State::Authorized;

    auto event = ApplePayPaymentAuthorizedEvent::create(eventNames().paymentauthorizedEvent, payment);
    dispatchEvent(event.get());
}

void ApplePaySession::didSelectShippingMethod(const PaymentRequest::ShippingMethod& shippingMethod)
{
    ASSERT(m_state == State::Active);

    if (!hasEventListeners(eventNames().shippingmethodselectedEvent)) {
        paymentCoordinator().completeShippingMethodSelection(PaymentAuthorizationStatus::Success, { });
        return;
    }

    m_state = State::ShippingMethodSelected;
    auto event = ApplePayShippingMethodSelectedEvent::create(eventNames().shippingmethodselectedEvent, shippingMethod);
    dispatchEvent(event.get());
}

void ApplePaySession::didSelectShippingContact(const PaymentContact& shippingContact)
{
    ASSERT(m_state == State::Active);

    if (!hasEventListeners(eventNames().shippingcontactselectedEvent)) {
        paymentCoordinator().completeShippingContactSelection(PaymentAuthorizationStatus::Success, { }, { });
        return;
    }

    m_state = State::ShippingContactSelected;
    auto event = ApplePayShippingContactSelectedEvent::create(eventNames().shippingcontactselectedEvent, shippingContact);
    dispatchEvent(event.get());
}

void ApplePaySession::didSelectPaymentMethod(const PaymentMethod& paymentMethod)
{
    ASSERT(m_state == State::Active);

    if (!hasEventListeners(eventNames().paymentmethodselectedEvent)) {
        paymentCoordinator().completePaymentMethodSelection({ });
        return;
    }

    m_state = State::PaymentMethodSelected;
    auto event = ApplePayPaymentMethodSelectedEvent::create(eventNames().paymentmethodselectedEvent, paymentMethod);
    dispatchEvent(event.get());
}

void ApplePaySession::didCancelPayment()
{
    ASSERT(canCancel());

    m_state = State::Canceled;

    auto event = Event::create(eventNames().cancelEvent, false, false);
    dispatchEvent(event.get());

    didReachFinalState();
}

const char* ApplePaySession::activeDOMObjectName() const
{
    return "ApplePaySession";
}

bool ApplePaySession::canSuspendForDocumentSuspension() const
{
    switch (m_state) {
    case State::Idle:
    case State::Aborted:
    case State::Completed:
    case State::Canceled:
        return true;

    case State::Active:
    case State::Authorized:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
        return false;
    }
}

void ApplePaySession::stop()
{
    if (!canAbort())
        return;

    m_state = State::Aborted;
    paymentCoordinator().abortPaymentSession();

    didReachFinalState();
}

PaymentCoordinator& ApplePaySession::paymentCoordinator() const
{
    return downcast<Document>(*scriptExecutionContext()).frame()->mainFrame().paymentCoordinator();
}

bool ApplePaySession::canBegin() const
{
    switch (m_state) {
    case State::Idle:
        return true;

    case State::Active:
    case State::Aborted:
    case State::Authorized:
    case State::Completed:
    case State::Canceled:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
        return false;
    }
}

bool ApplePaySession::canAbort() const
{
    switch (m_state) {
    case State::Idle:
    case State::Aborted:
    case State::Completed:
    case State::Canceled:
        return false;

    case State::Active:
    case State::Authorized:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
        return true;
    }
}

bool ApplePaySession::canCancel() const
{
    switch (m_state) {
    case State::Idle:
    case State::Aborted:
    case State::Completed:
    case State::Canceled:
        return false;

    case State::Active:
    case State::Authorized:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
        return true;
    }
}

bool ApplePaySession::canCompleteMerchantValidation() const
{
    if (m_state != State::Active)
        return false;

    if (m_merchantValidationState != MerchantValidationState::ValidatingMerchant)
        return false;

    return true;
}

bool ApplePaySession::canCompleteShippingMethodSelection() const
{
    switch (m_state) {
    case State::Idle:
    case State::Aborted:
    case State::Active:
    case State::Completed:
    case State::Canceled:
    case State::Authorized:
    case State::PaymentMethodSelected:
    case State::ShippingContactSelected:
        return false;

    case State::ShippingMethodSelected:
        return true;
    }
}

bool ApplePaySession::canCompleteShippingContactSelection() const
{
    switch (m_state) {
    case State::Idle:
    case State::Aborted:
    case State::Active:
    case State::Completed:
    case State::Canceled:
    case State::Authorized:
    case State::PaymentMethodSelected:
    case State::ShippingMethodSelected:
        return false;

    case State::ShippingContactSelected:
        return true;
    }
}

bool ApplePaySession::canCompletePaymentMethodSelection() const
{
    switch (m_state) {
    case State::Idle:
    case State::Aborted:
    case State::Active:
    case State::Completed:
    case State::Canceled:
    case State::Authorized:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
        return false;

    case State::PaymentMethodSelected:
        return true;
    }
}

bool ApplePaySession::canCompletePayment() const
{
    switch (m_state) {
    case State::Idle:
    case State::Aborted:
    case State::Active:
    case State::Completed:
    case State::Canceled:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
        return false;

    case State::Authorized:
        return true;
    }
}

bool ApplePaySession::isFinalState() const
{
    switch (m_state) {
    case State::Idle:
    case State::Active:
    case State::ShippingMethodSelected:
    case State::ShippingContactSelected:
    case State::PaymentMethodSelected:
    case State::Authorized:
        return false;

    case State::Completed:
    case State::Aborted:
    case State::Canceled:
        return true;
    }
}

void ApplePaySession::didReachFinalState()
{
    ASSERT(isFinalState());
    unsetPendingActivity(this);
}

}

#endif
