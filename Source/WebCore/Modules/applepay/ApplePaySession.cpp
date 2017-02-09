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

#include "ApplePayLineItem.h"
#include "ApplePayPaymentAuthorizedEvent.h"
#include "ApplePayPaymentMethodSelectedEvent.h"
#include "ApplePayPaymentRequest.h"
#include "ApplePayShippingContactSelectedEvent.h"
#include "ApplePayShippingMethod.h"
#include "ApplePayShippingMethodSelectedEvent.h"
#include "ApplePayValidateMerchantEvent.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "EventNames.h"
#include "JSDOMPromise.h"
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

static ExceptionOr<PaymentRequest::LineItem> convertAndValidateTotal(ApplePayLineItem&& lineItem)
{
    auto amount = parseAmount(lineItem.amount);
    if (!amount)
        return Exception { TypeError, makeString("\"" + lineItem.amount, "\" is not a valid amount.") };

    PaymentRequest::LineItem result;
    result.amount = *amount;
    result.type = lineItem.type;
    result.label = lineItem.label;

    return WTFMove(result);
}

static ExceptionOr<PaymentRequest::LineItem> convertAndValidate(ApplePayLineItem&& lineItem)
{
    PaymentRequest::LineItem result;

    // It is OK for pending types to not have an amount.
    if (lineItem.type != PaymentRequest::LineItem::Type::Pending) {
        auto amount = parseAmount(lineItem.amount);
        if (!amount)
            return Exception { TypeError, makeString("\"" + lineItem.amount, "\" is not a valid amount.") };

        result.amount = *amount;
    }

    result.type = lineItem.type;
    result.label = lineItem.label;

    return WTFMove(result);
}

static ExceptionOr<Vector<PaymentRequest::LineItem>> convertAndValidate(std::optional<Vector<ApplePayLineItem>>&& lineItems)
{
    Vector<PaymentRequest::LineItem> result;
    if (!lineItems)
        return WTFMove(result);

    result.reserveInitialCapacity(lineItems->size());
    
    for (auto lineItem : lineItems.value()) {
        auto convertedLineItem = convertAndValidate(WTFMove(lineItem));
        if (convertedLineItem.hasException())
            return convertedLineItem.releaseException();
        result.uncheckedAppend(convertedLineItem.releaseReturnValue());
    }

    return WTFMove(result);
}

static ExceptionOr<PaymentRequest::MerchantCapabilities> convertAndValidate(Vector<ApplePayPaymentRequest::MerchantCapability>&& merchantCapabilities)
{
    if (merchantCapabilities.isEmpty())
        return Exception { TypeError, "At least one merchant capability must be provided." };

    PaymentRequest::MerchantCapabilities result;

    for (auto& merchantCapability : merchantCapabilities) {
        switch (merchantCapability) {
        case ApplePayPaymentRequest::MerchantCapability::Supports3DS:
            result.supports3DS = true;
            break;
        case ApplePayPaymentRequest::MerchantCapability::SupportsEMV:
            result.supportsEMV = true;
            break;
        case ApplePayPaymentRequest::MerchantCapability::SupportsCredit:
            result.supportsCredit = true;
            break;
        case ApplePayPaymentRequest::MerchantCapability::SupportsDebit:
            result.supportsDebit = true;
            break;
        }
    }

    return WTFMove(result);
}

static ExceptionOr<Vector<String>> convertAndValidate(unsigned version, Vector<String>&& supportedNetworks)
{
    if (supportedNetworks.isEmpty())
        return Exception { TypeError, "At least one supported network must be provided." };

    for (auto& supportedNetwork : supportedNetworks) {
        if (!PaymentRequest::isValidSupportedNetwork(version, supportedNetwork))
            return Exception { TypeError, makeString("\"" + supportedNetwork, "\" is not a valid payment network.") };
    }

    return WTFMove(supportedNetworks);
}

static ExceptionOr<PaymentRequest::ContactFields> convertAndValidate(Vector<ApplePayPaymentRequest::ContactField>&& contactFields)
{
    if (contactFields.isEmpty())
        return Exception { TypeError, "At least one contact field must be provided." };

    PaymentRequest::ContactFields result;

    for (auto& contactField : contactFields) {
        switch (contactField) {
        case ApplePayPaymentRequest::ContactField::Email:
            result.email = true;
            break;
        case ApplePayPaymentRequest::ContactField::Name:
            result.name = true;
            break;
        case ApplePayPaymentRequest::ContactField::Phone:
            result.phone = true;
            break;
        case ApplePayPaymentRequest::ContactField::PostalAddress:
            result.postalAddress = true;
            break;
        }
    }

    return WTFMove(result);
}

static ExceptionOr<PaymentRequest::ShippingMethod> convertAndValidate(ApplePayShippingMethod&& shippingMethod)
{
    auto amount = parseAmount(shippingMethod.amount);
    if (!amount)
        return Exception { TypeError, makeString("\"" + shippingMethod.amount, "\" is not a valid amount.") };

    PaymentRequest::ShippingMethod result;
    result.amount = *amount;
    result.label = shippingMethod.label;
    result.detail = shippingMethod.detail;
    result.identifier = shippingMethod.identifier;

    return WTFMove(result);
}

static ExceptionOr<Vector<PaymentRequest::ShippingMethod>> convertAndValidate(Vector<ApplePayShippingMethod>&& shippingMethods)
{
    if (shippingMethods.isEmpty())
        return Exception { TypeError, "At least one shipping method must be provided." };

    Vector<PaymentRequest::ShippingMethod> result;
    result.reserveInitialCapacity(shippingMethods.size());
    
    for (auto& shippingMethod : shippingMethods) {
        auto convertedShippingMethod = convertAndValidate(WTFMove(shippingMethod));
        if (convertedShippingMethod.hasException())
            return convertedShippingMethod.releaseException();
        result.uncheckedAppend(convertedShippingMethod.releaseReturnValue());
    }

    return WTFMove(result);
}

static ExceptionOr<PaymentRequest> convertAndValidate(unsigned version, ApplePayPaymentRequest&& paymentRequest)
{
    PaymentRequest result;

    auto total = convertAndValidateTotal(WTFMove(paymentRequest.total));
    if (total.hasException())
        return total.releaseException();
    result.setTotal(total.releaseReturnValue());

    auto lineItems = convertAndValidate(WTFMove(paymentRequest.lineItems));
    if (lineItems.hasException())
        return lineItems.releaseException();
    result.setLineItems(lineItems.releaseReturnValue());

    result.setCountryCode(paymentRequest.countryCode);
    result.setCurrencyCode(paymentRequest.currencyCode);

    auto merchantCapabilities = convertAndValidate(WTFMove(paymentRequest.merchantCapabilities));
    if (merchantCapabilities.hasException())
        return merchantCapabilities.releaseException();
    result.setMerchantCapabilities(merchantCapabilities.releaseReturnValue());

    auto supportedNetworks = convertAndValidate(version, WTFMove(paymentRequest.supportedNetworks));
    if (supportedNetworks.hasException())
        return supportedNetworks.releaseException();
    result.setSupportedNetworks(supportedNetworks.releaseReturnValue());

    if (paymentRequest.requiredBillingContactFields) {
        auto requiredBillingContactFields = convertAndValidate(WTFMove(*paymentRequest.requiredBillingContactFields));
        if (requiredBillingContactFields.hasException())
            return requiredBillingContactFields.releaseException();
        result.setRequiredBillingContactFields(requiredBillingContactFields.releaseReturnValue());
    }

    if (paymentRequest.billingContact)
        result.setBillingContact(PaymentContact::fromApplePayPaymentContact(paymentRequest.billingContact.value()));

    if (paymentRequest.requiredShippingContactFields) {
        auto requiredShippingContactFields = convertAndValidate(WTFMove(*paymentRequest.requiredShippingContactFields));
        if (requiredShippingContactFields.hasException())
            return requiredShippingContactFields.releaseException();
        result.setRequiredShippingContactFields(requiredShippingContactFields.releaseReturnValue());
    }

    if (paymentRequest.shippingContact)
        result.setShippingContact(PaymentContact::fromApplePayPaymentContact(paymentRequest.shippingContact.value()));

    result.setShippingType(paymentRequest.shippingType);

    if (paymentRequest.shippingMethods) {
        auto shippingMethods = convertAndValidate(WTFMove(*paymentRequest.shippingMethods));
        if (shippingMethods.hasException())
            return shippingMethods.releaseException();
        result.setShippingMethods(shippingMethods.releaseReturnValue());
    }

    result.setApplicationData(paymentRequest.applicationData);

    // FIXME: Merge this validation into the validation we are doing above.
    auto validatedPaymentRequest = PaymentRequestValidator::validate(result);
    if (validatedPaymentRequest.hasException())
        return validatedPaymentRequest.releaseException();

    return WTFMove(result);
}

static bool isSecure(DocumentLoader& documentLoader)
{
    if (!documentLoader.response().url().protocolIs("https"))
        return false;

    if (!documentLoader.response().certificateInfo() || documentLoader.response().certificateInfo()->containsNonRootSHA1SignedCertificate())
        return false;

    return true;
}

static ExceptionOr<void> canCallApplePaySessionAPIs(Document& document)
{
    if (!isSecure(*document.loader()))
        return Exception { INVALID_ACCESS_ERR, "Trying to call an ApplePaySession API from an insecure document." };

    auto& topDocument = document.topDocument();
    if (&document != &topDocument) {
        auto& topOrigin = topDocument.topOrigin();

        if (!document.securityOrigin().isSameSchemeHostPort(topOrigin))
            return Exception { INVALID_ACCESS_ERR, "Trying to call an ApplePaySession API from a document with an different security origin than its top-level frame." };

        for (auto* ancestorDocument = document.parentDocument(); ancestorDocument != &topDocument; ancestorDocument = ancestorDocument->parentDocument()) {
            if (!isSecure(*ancestorDocument->loader()))
                return Exception { INVALID_ACCESS_ERR, "Trying to call an ApplePaySession API from a document with an insecure parent frame." };

            if (!ancestorDocument->securityOrigin().isSameSchemeHostPort(topOrigin))
                return Exception { INVALID_ACCESS_ERR, "Trying to call an ApplePaySession API from a document with an different security origin than its top-level frame." };
        }
    }

    return { };
}

ExceptionOr<Ref<ApplePaySession>> ApplePaySession::create(Document& document, unsigned version, ApplePayPaymentRequest&& paymentRequest)
{
    auto canCall = canCallApplePaySessionAPIs(document);
    if (canCall.hasException())
        return canCall.releaseException();

    if (!ScriptController::processingUserGesture())
        return Exception { INVALID_ACCESS_ERR, "Must create a new ApplePaySession from a user gesture handler." };

    auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();

    if (!version || !paymentCoordinator.supportsVersion(version))
        return Exception { INVALID_ACCESS_ERR, makeString("\"" + String::number(version), "\" is not a supported version.") };

    auto convertedPaymentRequest = convertAndValidate(version, WTFMove(paymentRequest));
    if (convertedPaymentRequest.hasException())
        return convertedPaymentRequest.releaseException();

    return adoptRef(*new ApplePaySession(document, convertedPaymentRequest.releaseReturnValue()));
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

    auto canCall = canCallApplePaySessionAPIs(document);
    if (canCall.hasException())
        return canCall.releaseException();

    return document.frame()->mainFrame().paymentCoordinator().supportsVersion(version);
}

static bool shouldDiscloseApplePayCapability(Document& document)
{
    auto* page = document.page();
    if (!page || page->usesEphemeralSession())
        return false;

    return document.settings().applePayCapabilityDisclosureAllowed();
}

ExceptionOr<bool> ApplePaySession::canMakePayments(ScriptExecutionContext& scriptExecutionContext)
{
    auto& document = downcast<Document>(scriptExecutionContext);

    auto canCall = canCallApplePaySessionAPIs(document);
    if (canCall.hasException())
        return canCall.releaseException();

    return document.frame()->mainFrame().paymentCoordinator().canMakePayments();
}

ExceptionOr<void> ApplePaySession::canMakePaymentsWithActiveCard(ScriptExecutionContext& scriptExecutionContext, const String& merchantIdentifier, Ref<DeferredPromise>&& passedPromise)
{
    auto& document = downcast<Document>(scriptExecutionContext);

    auto canCall = canCallApplePaySessionAPIs(document);
    if (canCall.hasException())
        return canCall.releaseException();

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

    auto canCall = canCallApplePaySessionAPIs(document);
    if (canCall.hasException())
        return canCall.releaseException();

    if (!ScriptController::processingUserGesture())
        return Exception { INVALID_ACCESS_ERR, "Must call ApplePaySession.openPaymentSetup from a user gesture handler." };

    RefPtr<DeferredPromise> promise(WTFMove(passedPromise));
    auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();

    paymentCoordinator.openPaymentSetup(merchantIdentifier, document.domain(), [promise](bool result) mutable {
        promise->resolve<IDLBoolean>(result);
    });

    return { };
}

ExceptionOr<void> ApplePaySession::begin()
{
    if (!canBegin())
        return Exception { INVALID_ACCESS_ERR, "Payment session is already active." };

    if (paymentCoordinator().hasActiveSession())
        return Exception { INVALID_ACCESS_ERR, "Page already has an active payment session." };

    auto& document = *downcast<Document>(scriptExecutionContext());

    Vector<URL> linkIconURLs;
    for (auto& icon : LinkIconCollector { document }.iconsOfTypes({ LinkIconType::TouchIcon, LinkIconType::TouchPrecomposedIcon }))
        linkIconURLs.append(icon.url);

    if (!paymentCoordinator().beginPaymentSession(*this, document.url(), linkIconURLs, m_paymentRequest))
        return Exception { INVALID_ACCESS_ERR, "There is already has an active payment session." };

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

ExceptionOr<void> ApplePaySession::completeMerchantValidation(JSC::ExecState& state, JSC::JSValue merchantSessionValue)
{
    if (!canCompleteMerchantValidation())
        return Exception { INVALID_ACCESS_ERR };

    if (!merchantSessionValue.isObject())
        return Exception { TypeError };

    auto& document = *downcast<Document>(scriptExecutionContext());
    auto& window = *document.domWindow();

    String errorMessage;
    auto merchantSession = PaymentMerchantSession::fromJS(state, asObject(merchantSessionValue), errorMessage);
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

ExceptionOr<void> ApplePaySession::completeShippingMethodSelection(unsigned short status, ApplePayLineItem&& newTotal, Vector<ApplePayLineItem>&& newLineItems)
{
    if (!canCompleteShippingMethodSelection())
        return Exception { INVALID_ACCESS_ERR };

    auto authorizationStatus = toPaymentAuthorizationStatus(status);
    if (!authorizationStatus)
        return Exception { INVALID_ACCESS_ERR };

    auto convertedNewTotal = convertAndValidateTotal(WTFMove(newTotal));
    if (convertedNewTotal.hasException())
        return convertedNewTotal.releaseException();

    PaymentRequest::TotalAndLineItems totalAndLineItems;
    totalAndLineItems.total = convertedNewTotal.releaseReturnValue();

    // FIXME: Merge this validation into the validation we are doing above.
    auto validatedTotal = PaymentRequestValidator::validateTotal(totalAndLineItems.total);
    if (validatedTotal.hasException())
        return validatedTotal.releaseException();

    auto convertedNewLineItems = convertAndValidate(WTFMove(newLineItems));
    if (convertedNewLineItems.hasException())
        return convertedNewLineItems.releaseException();

    totalAndLineItems.lineItems = convertedNewLineItems.releaseReturnValue();

    m_state = State::Active;
    paymentCoordinator().completeShippingMethodSelection(*authorizationStatus, totalAndLineItems);

    return { };
}

ExceptionOr<void> ApplePaySession::completeShippingContactSelection(unsigned short status, Vector<ApplePayShippingMethod>&& newShippingMethods, ApplePayLineItem&& newTotal, Vector<ApplePayLineItem>&& newLineItems)
{
    if (!canCompleteShippingContactSelection())
        return Exception { INVALID_ACCESS_ERR };

    auto authorizationStatus = toPaymentAuthorizationStatus(status);
    if (!authorizationStatus)
        return Exception { INVALID_ACCESS_ERR };

    auto convertedNewShippingMethods = convertAndValidate(WTFMove(newShippingMethods));
    if (convertedNewShippingMethods.hasException())
        return convertedNewShippingMethods.releaseException();

    auto convertedNewTotal = convertAndValidateTotal(WTFMove(newTotal));
    if (convertedNewTotal.hasException())
        return convertedNewTotal.releaseException();

    PaymentRequest::TotalAndLineItems totalAndLineItems;
    totalAndLineItems.total = convertedNewTotal.releaseReturnValue();

    // FIXME: Merge this validation into the validation we are doing above.
    auto validatedTotal = PaymentRequestValidator::validateTotal(totalAndLineItems.total);
    if (validatedTotal.hasException())
        return validatedTotal.releaseException();

    auto convertedNewLineItems = convertAndValidate(WTFMove(newLineItems));
    if (convertedNewLineItems.hasException())
        return convertedNewLineItems.releaseException();

    totalAndLineItems.lineItems = convertedNewLineItems.releaseReturnValue();

    m_state = State::Active;
    paymentCoordinator().completeShippingContactSelection(*authorizationStatus, convertedNewShippingMethods.releaseReturnValue(), totalAndLineItems);

    return { };
}

ExceptionOr<void> ApplePaySession::completePaymentMethodSelection(ApplePayLineItem&& newTotal, Vector<ApplePayLineItem>&& newLineItems)
{
    if (!canCompletePaymentMethodSelection())
        return Exception { INVALID_ACCESS_ERR };

    auto convertedNewTotal = convertAndValidateTotal(WTFMove(newTotal));
    if (convertedNewTotal.hasException())
        return convertedNewTotal.releaseException();

    PaymentRequest::TotalAndLineItems totalAndLineItems;
    totalAndLineItems.total = convertedNewTotal.releaseReturnValue();

    // FIXME: Merge this validation into the validation we are doing above.
    auto validatedTotal = PaymentRequestValidator::validateTotal(totalAndLineItems.total);
    if (validatedTotal.hasException())
        return validatedTotal.releaseException();

    auto convertedNewLineItems = convertAndValidate(WTFMove(newLineItems));
    if (convertedNewLineItems.hasException())
        return convertedNewLineItems.releaseException();

    totalAndLineItems.lineItems = convertedNewLineItems.releaseReturnValue();

    m_state = State::Active;
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
