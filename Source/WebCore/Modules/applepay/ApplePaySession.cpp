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
static Optional<int64_t> parseAmount(const String& amountString)
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
                return Nullopt;
            state = State::Digit;
            break;

        case State::Sign:
            if (!parseDigit(c, isNegative, amount))
                return Nullopt;
            state = State::Digit;
            break;

        case State::Digit:
            if (c == '.') {
                state = State::Dot;
                break;
            }

            if (!parseDigit(c, isNegative, amount))
                return Nullopt;
            break;

        case State::Dot:
            if (!parseDigit(c, isNegative, amount))
                return Nullopt;

            state = State::DotDigit;
            break;

        case State::DotDigit:
            if (!parseDigit(c, isNegative, amount))
                return Nullopt;

            state = State::End;
            break;
            
        case State::End:
            return Nullopt;
        }
    }
    
    if (state != State::Digit && state != State::DotDigit && state != State::End)
        return Nullopt;

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

static Optional<PaymentRequest::ContactFields> createContactFields(DOMWindow& window, const ArrayValue& contactFieldsArray)
{
    PaymentRequest::ContactFields result;

    size_t contactFieldsCount;
    if (!contactFieldsArray.length(contactFieldsCount))
        return Nullopt;

    for (size_t i = 0; i < contactFieldsCount; ++i) {
        String contactField;
        if (!contactFieldsArray.get(i, contactField))
            return Nullopt;

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
            return Nullopt;
        }
    }

    return result;
}

static Optional<PaymentRequest::LineItem::Type> toLineItemType(const String& type)
{
    if (type == "pending")
        return PaymentRequest::LineItem::Type::Pending;
    if (type == "final")
        return PaymentRequest::LineItem::Type::Final;

    return Nullopt;
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

static Optional<PaymentRequest::LineItem> createLineItem(DOMWindow& window, const Dictionary& total)
{
    Vector<String> propertyNames;
    total.getOwnPropertyNames(propertyNames);

    for (auto& propertyName : propertyNames) {
        if (!isValidLineItemPropertyName(propertyName)) {
            auto message = makeString("\"" + propertyName, "\" is not a valid line item property name.");
            window.printErrorMessage(message);
            return Nullopt;
        }
    }

    // Line item type defaults to Final.
    PaymentRequest::LineItem result;

    if (auto typeString = total.get<String>("type")) {
        auto type = toLineItemType(*typeString);
        if (!type) {
            auto message = makeString("\"" + *typeString, "\" is not a valid line item type.");
            window.printErrorMessage(message);
            return Nullopt;
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
            return Nullopt;
        }
    }

    return result;
}

static Optional<Vector<PaymentRequest::LineItem>> createLineItems(DOMWindow& window, const ArrayValue& lineItemsArray)
{
    Vector<PaymentRequest::LineItem> result;

    size_t lineItemCount;
    if (!lineItemsArray.length(lineItemCount))
        return Nullopt;

    for (size_t i = 0; i < lineItemCount; ++i) {
        Dictionary lineItemDictionary;
        if (!lineItemsArray.get(i, lineItemDictionary))
            return Nullopt;

        if (auto lineItem = createLineItem(window, lineItemDictionary))
            result.append(*lineItem);
    }

    return result;
}

static Optional<PaymentRequest::MerchantCapabilities> createMerchantCapabilities(DOMWindow& window, const ArrayValue& merchantCapabilitiesArray)
{
    PaymentRequest::MerchantCapabilities result;

    size_t merchantCapabilitiesCount;
    if (!merchantCapabilitiesArray.length(merchantCapabilitiesCount))
        return Nullopt;

    for (size_t i = 0; i < merchantCapabilitiesCount; ++i) {
        String merchantCapability;
        if (!merchantCapabilitiesArray.get(i, merchantCapability))
            return Nullopt;

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
            return Nullopt;
        }
    }

    return result;
}

static Optional<Vector<String>> createSupportedNetworks(unsigned version, DOMWindow& window, const ArrayValue& supportedNetworksArray)
{
    Vector<String> result;

    size_t supportedNetworksCount;
    if (!supportedNetworksArray.length(supportedNetworksCount))
        return Nullopt;

    for (size_t i = 0; i < supportedNetworksCount; ++i) {
        String supportedNetwork;
        if (!supportedNetworksArray.get(i, supportedNetwork))
            return Nullopt;

        if (!PaymentRequest::isValidSupportedNetwork(version, supportedNetwork)) {
            auto message = makeString("\"" + supportedNetwork, "\" is not a valid payment network.");
            window.printErrorMessage(message);
            return Nullopt;
        }

        result.append(WTFMove(supportedNetwork));
    }

    return result;
}

static Optional<PaymentRequest::ShippingType> toShippingType(const String& shippingTypeString)
{
    if (shippingTypeString == "shipping")
        return PaymentRequest::ShippingType::Shipping;
    if (shippingTypeString == "delivery")
        return PaymentRequest::ShippingType::Delivery;
    if (shippingTypeString == "storePickup")
        return PaymentRequest::ShippingType::StorePickup;
    if (shippingTypeString == "servicePickup")
        return PaymentRequest::ShippingType::ServicePickup;

    return Nullopt;
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

static Optional<PaymentRequest::ShippingMethod> createShippingMethod(DOMWindow& window, const Dictionary& shippingMethodDictionary)
{
    Vector<String> propertyNames;
    shippingMethodDictionary.getOwnPropertyNames(propertyNames);

    for (auto& propertyName : propertyNames) {
        if (!isValidShippingMethodPropertyName(propertyName)) {
            auto message = makeString("\"" + propertyName, "\" is not a valid shipping method property name.");
            window.printErrorMessage(message);
            return Nullopt;
        }
    }

    PaymentRequest::ShippingMethod result;

    auto label = shippingMethodDictionary.get<String>("label");
    if (!label) {
        window.printErrorMessage("Missing shipping method label.");
        return Nullopt;
    }
    result.label = *label;

    auto detail = shippingMethodDictionary.get<String>("detail");
    if (!detail) {
        window.printErrorMessage("Missing shipping method detail.");
        return Nullopt;
    }
    result.detail = *detail;

    auto amountString = shippingMethodDictionary.get<String>("amount");
    if (!amountString) {
        window.printErrorMessage("Missing shipping method amount.");
        return Nullopt;
    }

    if (auto amount = parseAmount(*amountString))
        result.amount = *amount;
    else {
        auto message = makeString("\"" + *amountString, "\" is not a valid amount.");
        window.printErrorMessage(message);
        return Nullopt;
    }

    auto identifier = shippingMethodDictionary.get<String>("identifier");
    if (!identifier) {
        window.printErrorMessage("Missing shipping method identifier.");
        return Nullopt;
    }
    result.identifier = *identifier;

    return result;
}

static Optional<Vector<PaymentRequest::ShippingMethod>> createShippingMethods(DOMWindow& window, const ArrayValue& shippingMethodsArray)
{
    Vector<PaymentRequest::ShippingMethod> result;

    size_t shippingMethodCount;
    if (!shippingMethodsArray.length(shippingMethodCount))
        return Nullopt;

    for (size_t i = 0; i < shippingMethodCount; ++i) {
        Dictionary shippingMethodDictionary;
        if (!shippingMethodsArray.get(i, shippingMethodDictionary))
            return Nullopt;

        if (auto shippingMethod = createShippingMethod(window, shippingMethodDictionary))
            result.append(*shippingMethod);
        else
            return Nullopt;
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

static Optional<PaymentRequest> createPaymentRequest(unsigned version, DOMWindow& window, const Dictionary& dictionary)
{
    PaymentRequest paymentRequest;

    Vector<String> propertyNames;
    dictionary.getOwnPropertyNames(propertyNames);

    for (auto& propertyName : propertyNames) {
        if (propertyName == "requiredShippingAddressFields") {
            window.printErrorMessage("\"requiredShippingAddressFields\" has been deprecated. Please switch to \"requiredShippingContactFields\" instead.");
            return Nullopt;
        }

        if (propertyName == "requiredBillingAddressFields") {
            window.printErrorMessage("\"requiredBillingAddressFields\" has been deprecated. Please switch to \"requiredBillingContactFields\" instead.");
            return Nullopt;
        }

        if (!isValidPaymentRequestPropertyName(propertyName)) {
            auto message = makeString("\"" + propertyName, "\" is not a valid payment request property name.");
            window.printErrorMessage(message);
            return Nullopt;
        }
    }

    if (auto merchantCapabilitiesArray = dictionary.get<ArrayValue>("merchantCapabilities")) {
        auto merchantCapabilities = createMerchantCapabilities(window, *merchantCapabilitiesArray);
        if (!merchantCapabilities)
            return Nullopt;

        paymentRequest.setMerchantCapabilities(*merchantCapabilities);
    }

    if (auto supportedNetworksArray = dictionary.get<ArrayValue>("supportedNetworks")) {
        auto supportedNetworks = createSupportedNetworks(version, window, *supportedNetworksArray);
        if (!supportedNetworks)
            return Nullopt;

        paymentRequest.setSupportedNetworks(*supportedNetworks);
    }

    if (auto countryCode = dictionary.get<String>("countryCode"))
        paymentRequest.setCountryCode(*countryCode);
    if (auto currencyCode = dictionary.get<String>("currencyCode"))
        paymentRequest.setCurrencyCode(*currencyCode);

    if (auto requiredBillingContactFieldsArray = dictionary.get<ArrayValue>("requiredBillingContactFields")) {
        auto requiredBillingContactFields = createContactFields(window, *requiredBillingContactFieldsArray);
        if (!requiredBillingContactFields)
            return Nullopt;

        paymentRequest.setRequiredBillingContactFields(*requiredBillingContactFields);
    }

    if (auto billingContactValue = dictionary.get<JSC::JSValue>("billingContact")) {
        String errorMessage;
        auto billingContact = PaymentContact::fromJS(*JSMainThreadExecState::currentState(), *billingContactValue, errorMessage);
        if (!billingContact) {
            window.printErrorMessage(errorMessage);
            return Nullopt;
        }

        paymentRequest.setBillingContact(*billingContact);
    }

    if (auto requiredShippingContactFieldsArray = dictionary.get<ArrayValue>("requiredShippingContactFields")) {
        auto requiredShippingContactFields = createContactFields(window, *requiredShippingContactFieldsArray);
        if (!requiredShippingContactFields)
            return Nullopt;

        paymentRequest.setRequiredShippingContactFields(*requiredShippingContactFields);
    }

    if (auto shippingContactValue = dictionary.get<JSC::JSValue>("shippingContact")) {
        String errorMessage;
        auto shippingContact = PaymentContact::fromJS(*JSMainThreadExecState::currentState(), *shippingContactValue, errorMessage);
        if (!shippingContact) {
            window.printErrorMessage(errorMessage);
            return Nullopt;
        }

        paymentRequest.setShippingContact(*shippingContact);
    }

    if (auto shippingTypeString = dictionary.get<String>("shippingType")) {
        auto shippingType = toShippingType(*shippingTypeString);

        if (!shippingType) {
            auto message = makeString("\"" + *shippingTypeString, "\" is not a valid shipping type.");
            window.printErrorMessage(message);
            return Nullopt;
        }
        paymentRequest.setShippingType(*shippingType);
    }

    if (auto shippingMethodsArray = dictionary.get<ArrayValue>("shippingMethods")) {
        auto shippingMethods = createShippingMethods(window, *shippingMethodsArray);
        if (!shippingMethods)
            return Nullopt;

        paymentRequest.setShippingMethods(*shippingMethods);
    }

    if (auto totalDictionary = dictionary.get<Dictionary>("total")) {
        auto total = createLineItem(window, *totalDictionary);
        if (!total)
            return Nullopt;

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

RefPtr<ApplePaySession> ApplePaySession::create(Document& document, unsigned version, const Dictionary& dictionary, ExceptionCode& ec)
{
    DOMWindow& window = *document.domWindow();

    String errorMessage;
    if (!canCallApplePaySessionAPIs(document, errorMessage)) {
        window.printErrorMessage(errorMessage);
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    if (!ScriptController::processingUserGesture()) {
        window.printErrorMessage("Must create a new ApplePaySession from a user gesture handler.");
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();

    if (!version || !paymentCoordinator.supportsVersion(version)) {
        window.printErrorMessage(makeString("\"" + String::number(version), "\" is not a supported version."));
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    auto paymentRequest = createPaymentRequest(version, window, dictionary);
    if (!paymentRequest) {
        ec = TYPE_MISMATCH_ERR;
        return nullptr;
    }

    if (!PaymentRequestValidator(window).validate(*paymentRequest)) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    return adoptRef(new ApplePaySession(document, WTFMove(*paymentRequest)));
}

ApplePaySession::ApplePaySession(Document& document, PaymentRequest&& paymentRequest)
    : ActiveDOMObject(&document)
    , m_state(State::Idle)
    , m_merchantValidationState(MerchantValidationState::Idle)
    , m_paymentRequest(WTFMove(paymentRequest))
{
    suspendIfNeeded();
}

ApplePaySession::~ApplePaySession()
{
}

bool ApplePaySession::supportsVersion(ScriptExecutionContext& scriptExecutionContext, unsigned version, ExceptionCode& ec)
{
    if (!version) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    auto& document = downcast<Document>(scriptExecutionContext);
    DOMWindow& window = *document.domWindow();

    String errorMessage;
    if (!canCallApplePaySessionAPIs(document, errorMessage)) {
        window.printErrorMessage(errorMessage);
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();
    return paymentCoordinator.supportsVersion(version);
}

static bool shouldDiscloseApplePayCapability(Document& document)
{
    auto* page = document.page();
    if (!page || page->usesEphemeralSession())
        return false;

    return document.frame()->settings().applePayCapabilityDisclosureAllowed();
}

bool ApplePaySession::canMakePayments(ScriptExecutionContext& scriptExecutionContext, ExceptionCode& ec)
{
    auto& document = downcast<Document>(scriptExecutionContext);
    DOMWindow& window = *document.domWindow();

    String errorMessage;
    if (!canCallApplePaySessionAPIs(document, errorMessage)) {
        window.printErrorMessage(errorMessage);
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();
    return paymentCoordinator.canMakePayments();
}

void ApplePaySession::canMakePaymentsWithActiveCard(ScriptExecutionContext& scriptExecutionContext, const String& merchantIdentifier, DeferredWrapper&& promise, ExceptionCode& ec)
{
    auto& document = downcast<Document>(scriptExecutionContext);
    DOMWindow& window = *document.domWindow();

    String errorMessage;
    if (!canCallApplePaySessionAPIs(document, errorMessage)) {
        window.printErrorMessage(errorMessage);
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!shouldDiscloseApplePayCapability(document)) {
        auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();
        bool canMakePayments = paymentCoordinator.canMakePayments();

        RunLoop::main().dispatch([promise, canMakePayments]() mutable {
            promise.resolve(canMakePayments);
        });
        return;
    }

    auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();

    paymentCoordinator.canMakePaymentsWithActiveCard(merchantIdentifier, document.domain(), [promise](bool canMakePayments) mutable {
        promise.resolve(canMakePayments);
    });
}

void ApplePaySession::begin(ExceptionCode& ec)
{
    auto& document = *downcast<Document>(scriptExecutionContext());
    auto& window = *document.domWindow();

    if (!canBegin()) {
        window.printErrorMessage("Payment session is already active.");
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (paymentCoordinator().hasActiveSession()) {
        window.printErrorMessage("Page already has an active payment session.");

        ec = INVALID_ACCESS_ERR;
        return;
    }

    Vector<URL> linkIconURLs;
    for (auto& icon : LinkIconCollector { document }.iconsOfTypes({ LinkIconType::TouchIcon, LinkIconType::TouchPrecomposedIcon }))
        linkIconURLs.append(icon.url);

    if (!paymentCoordinator().beginPaymentSession(*this, document.url(), linkIconURLs, m_paymentRequest)) {
        window.printErrorMessage("There is already has an active payment session.");

        ec = INVALID_ACCESS_ERR;
        return;
    }

    m_state = State::Active;

    setPendingActivity(this);
}

void ApplePaySession::abort(ExceptionCode& ec)
{
    if (!canAbort()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    m_state = State::Aborted;
    paymentCoordinator().abortPaymentSession();

    didReachFinalState();
}

void ApplePaySession::completeMerchantValidation(const Dictionary& merchantSessionDictionary, ExceptionCode& ec)
{
    if (!canCompleteMerchantValidation()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto& document = *downcast<Document>(scriptExecutionContext());
    auto& window = *document.domWindow();

    String errorMessage;
    auto merchantSession = PaymentMerchantSession::fromJS(*merchantSessionDictionary.execState(), merchantSessionDictionary.initializerObject(), errorMessage);
    if (!merchantSession) {
        window.printErrorMessage(errorMessage);
        ec = INVALID_ACCESS_ERR;
        return;
    }

    m_merchantValidationState = MerchantValidationState::ValidationComplete;
    paymentCoordinator().completeMerchantValidation(*merchantSession);
}


static Optional<PaymentAuthorizationStatus> toPaymentAuthorizationStatus(unsigned short status)
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
        return Nullopt;
    }
}

void ApplePaySession::completeShippingMethodSelection(unsigned short status, const Dictionary& newTotalDictionary, const ArrayValue& newLineItemsArray, ExceptionCode& ec)
{
    if (!canCompleteShippingMethodSelection()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto authorizationStatus = toPaymentAuthorizationStatus(status);
    if (!authorizationStatus) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto& window = *downcast<Document>(scriptExecutionContext())->domWindow();
    auto newTotal = createLineItem(window, newTotalDictionary);
    if (!newTotal) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!PaymentRequestValidator(window).validateTotal(*newTotal)) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto newLineItems = createLineItems(window, newLineItemsArray);
    if (!newLineItems) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    m_state = State::Active;
    PaymentRequest::TotalAndLineItems totalAndLineItems;
    totalAndLineItems.total = *newTotal;
    totalAndLineItems.lineItems = *newLineItems;
    paymentCoordinator().completeShippingMethodSelection(*authorizationStatus, totalAndLineItems);
}

void ApplePaySession::completeShippingContactSelection(unsigned short status, const ArrayValue& newShippingMethodsArray, const Dictionary& newTotalDictionary, const ArrayValue& newLineItemsArray, ExceptionCode& ec)
{
    if (!canCompleteShippingContactSelection()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto authorizationStatus = toPaymentAuthorizationStatus(status);
    if (!authorizationStatus) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto& window = *downcast<Document>(scriptExecutionContext())->domWindow();

    auto newShippingMethods = createShippingMethods(window, newShippingMethodsArray);
    if (!newShippingMethods) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto newTotal = createLineItem(window, newTotalDictionary);
    if (!newTotal) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!PaymentRequestValidator(window).validateTotal(*newTotal)) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto newLineItems = createLineItems(window, newLineItemsArray);
    if (!newLineItems) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    m_state = State::Active;
    PaymentRequest::TotalAndLineItems totalAndLineItems;
    totalAndLineItems.total = *newTotal;
    totalAndLineItems.lineItems = *newLineItems;
    paymentCoordinator().completeShippingContactSelection(*authorizationStatus, *newShippingMethods, totalAndLineItems);
}

void ApplePaySession::completePaymentMethodSelection(const Dictionary& newTotalDictionary, const ArrayValue& newLineItemsArray, ExceptionCode& ec)
{
    if (!canCompletePaymentMethodSelection()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto& window = *downcast<Document>(scriptExecutionContext())->domWindow();
    auto newTotal = createLineItem(window, newTotalDictionary);
    if (!newTotal) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!PaymentRequestValidator(window).validateTotal(*newTotal)) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto newLineItems = createLineItems(window, newLineItemsArray);
    if (!newLineItems) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    m_state = State::Active;
    PaymentRequest::TotalAndLineItems totalAndLineItems;
    totalAndLineItems.total = *newTotal;
    totalAndLineItems.lineItems = *newLineItems;
    paymentCoordinator().completePaymentMethodSelection(totalAndLineItems);
}

void ApplePaySession::completePayment(unsigned short status, ExceptionCode& ec)
{
    if (!canCompletePayment()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto authorizationStatus = toPaymentAuthorizationStatus(status);
    if (!authorizationStatus) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    paymentCoordinator().completePaymentSession(*authorizationStatus);

    if (!isFinalStateStatus(*authorizationStatus)) {
        m_state = State::Active;
        return;
    }

    m_state = State::Completed;
    unsetPendingActivity(this);
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

    RefPtr<ApplePayValidateMerchantEvent> event = ApplePayValidateMerchantEvent::create(eventNames().validatemerchantEvent, validationURL);
    dispatchEvent(*event);
}

void ApplePaySession::didAuthorizePayment(const Payment& payment)
{
    ASSERT(m_state == State::Active);

    m_state = State::Authorized;

    RefPtr<ApplePayPaymentAuthorizedEvent> event = ApplePayPaymentAuthorizedEvent::create(eventNames().paymentauthorizedEvent, payment);
    dispatchEvent(*event);
}

void ApplePaySession::didSelectShippingMethod(const PaymentRequest::ShippingMethod& shippingMethod)
{
    ASSERT(m_state == State::Active);

    if (!hasEventListeners(eventNames().shippingmethodselectedEvent)) {
        paymentCoordinator().completeShippingMethodSelection(PaymentAuthorizationStatus::Success, { });
        return;
    }

    m_state = State::ShippingMethodSelected;
    RefPtr<ApplePayShippingMethodSelectedEvent> event = ApplePayShippingMethodSelectedEvent::create(eventNames().shippingmethodselectedEvent, shippingMethod);
    dispatchEvent(*event);
}

void ApplePaySession::didSelectShippingContact(const PaymentContact& shippingContact)
{
    ASSERT(m_state == State::Active);

    if (!hasEventListeners(eventNames().shippingcontactselectedEvent)) {
        paymentCoordinator().completeShippingContactSelection(PaymentAuthorizationStatus::Success, { }, { });
        return;
    }

    m_state = State::ShippingContactSelected;
    RefPtr<ApplePayShippingContactSelectedEvent> event = ApplePayShippingContactSelectedEvent::create(eventNames().shippingcontactselectedEvent, shippingContact);
    dispatchEvent(*event);
}

void ApplePaySession::didSelectPaymentMethod(const PaymentMethod& paymentMethod)
{
    ASSERT(m_state == State::Active);

    if (!hasEventListeners(eventNames().paymentmethodselectedEvent)) {
        paymentCoordinator().completePaymentMethodSelection({ });
        return;
    }

    m_state = State::PaymentMethodSelected;
    RefPtr<ApplePayPaymentMethodSelectedEvent> event = ApplePayPaymentMethodSelectedEvent::create(eventNames().paymentmethodselectedEvent, paymentMethod);
    dispatchEvent(*event);
}

void ApplePaySession::didCancelPayment()
{
    ASSERT(canCancel());

    m_state = State::Canceled;

    RefPtr<Event> event = Event::create(eventNames().cancelEvent, false, false);
    dispatchEvent(*event);

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
    return downcast<Document>(scriptExecutionContext())->frame()->mainFrame().paymentCoordinator();
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
