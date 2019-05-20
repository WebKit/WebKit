/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "ApplePayPaymentAuthorizationResult.h"
#include "ApplePayPaymentAuthorizedEvent.h"
#include "ApplePayPaymentMethodSelectedEvent.h"
#include "ApplePayPaymentMethodUpdate.h"
#include "ApplePayPaymentRequest.h"
#include "ApplePayShippingContactSelectedEvent.h"
#include "ApplePayShippingContactUpdate.h"
#include "ApplePayShippingMethod.h"
#include "ApplePayShippingMethodSelectedEvent.h"
#include "ApplePayShippingMethodUpdate.h"
#include "ApplePayValidateMerchantEvent.h"
#include "CustomHeaderFields.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "EventNames.h"
#include "Frame.h"
#include "JSDOMPromiseDeferred.h"
#include "LinkIconCollector.h"
#include "LinkIconType.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PaymentAuthorizationStatus.h"
#include "PaymentContact.h"
#include "PaymentCoordinator.h"
#include "PaymentMerchantSession.h"
#include "PaymentMethod.h"
#include "PaymentRequestValidator.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ApplePaySession);

// The amount follows the regular expression -?[0-9]+(\.[0-9][0-9])?.
static bool validateAmount(const String& amountString)
{
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
                state = State::Sign;
                break;
            }

            if (!isASCIIDigit(c))
                return false;
            state = State::Digit;
            break;

        case State::Sign:
            if (!isASCIIDigit(c))
                return false;
            state = State::Digit;
            break;

        case State::Digit:
            if (c == '.') {
                state = State::Dot;
                break;
            }

            if (!isASCIIDigit(c))
                return false;
            break;

        case State::Dot:
            if (!isASCIIDigit(c))
                return false;

            state = State::DotDigit;
            break;

        case State::DotDigit:
            if (!isASCIIDigit(c))
                return false;

            state = State::End;
            break;

        case State::End:
            return false;
        }
    }

    return state == State::Digit || state == State::DotDigit || state == State::End;
}

static ExceptionOr<ApplePaySessionPaymentRequest::LineItem> convertAndValidateTotal(ApplePayLineItem&& lineItem)
{
    if (!validateAmount(lineItem.amount))
        return Exception { TypeError, makeString("\"" + lineItem.amount, "\" is not a valid amount.") };

    ApplePaySessionPaymentRequest::LineItem result;
    result.amount = lineItem.amount;
    result.type = lineItem.type;
    result.label = lineItem.label;

    return WTFMove(result);
}

static ExceptionOr<ApplePaySessionPaymentRequest::LineItem> convertAndValidate(ApplePayLineItem&& lineItem)
{
    ApplePaySessionPaymentRequest::LineItem result;

    // It is OK for pending types to not have an amount.
    if (lineItem.type != ApplePaySessionPaymentRequest::LineItem::Type::Pending) {
        if (!validateAmount(lineItem.amount))
            return Exception { TypeError, makeString("\"" + lineItem.amount, "\" is not a valid amount.") };

        result.amount = lineItem.amount;
    }

    result.type = lineItem.type;
    result.label = lineItem.label;

    return WTFMove(result);
}

static ExceptionOr<Vector<ApplePaySessionPaymentRequest::LineItem>> convertAndValidate(Optional<Vector<ApplePayLineItem>>&& lineItems)
{
    Vector<ApplePaySessionPaymentRequest::LineItem> result;
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

static ExceptionOr<ApplePaySessionPaymentRequest::ShippingMethod> convertAndValidate(ApplePayShippingMethod&& shippingMethod)
{
    if (!validateAmount(shippingMethod.amount))
        return Exception { TypeError, makeString("\"" + shippingMethod.amount, "\" is not a valid amount.") };

    ApplePaySessionPaymentRequest::ShippingMethod result;
    result.amount = shippingMethod.amount;
    result.label = shippingMethod.label;
    result.detail = shippingMethod.detail;
    result.identifier = shippingMethod.identifier;

    return WTFMove(result);
}

static ExceptionOr<Vector<ApplePaySessionPaymentRequest::ShippingMethod>> convertAndValidate(Vector<ApplePayShippingMethod>&& shippingMethods)
{
    Vector<ApplePaySessionPaymentRequest::ShippingMethod> result;
    result.reserveInitialCapacity(shippingMethods.size());
    
    for (auto& shippingMethod : shippingMethods) {
        auto convertedShippingMethod = convertAndValidate(WTFMove(shippingMethod));
        if (convertedShippingMethod.hasException())
            return convertedShippingMethod.releaseException();
        result.uncheckedAppend(convertedShippingMethod.releaseReturnValue());
    }

    return WTFMove(result);
}

static ExceptionOr<ApplePaySessionPaymentRequest> convertAndValidate(Document& document, unsigned version, ApplePayPaymentRequest&& paymentRequest, const PaymentCoordinator& paymentCoordinator)
{
    auto convertedRequest = convertAndValidate(document, version, paymentRequest, paymentCoordinator);
    if (convertedRequest.hasException())
        return convertedRequest.releaseException();

    auto result = convertedRequest.releaseReturnValue();
    result.setRequester(ApplePaySessionPaymentRequest::Requester::ApplePayJS);
    result.setCurrencyCode(paymentRequest.currencyCode);

    auto total = convertAndValidateTotal(WTFMove(paymentRequest.total));
    if (total.hasException())
        return total.releaseException();
    result.setTotal(total.releaseReturnValue());

    auto lineItems = convertAndValidate(WTFMove(paymentRequest.lineItems));
    if (lineItems.hasException())
        return lineItems.releaseException();
    result.setLineItems(lineItems.releaseReturnValue());

    result.setShippingType(paymentRequest.shippingType);

    if (paymentRequest.shippingMethods) {
        auto shippingMethods = convertAndValidate(WTFMove(*paymentRequest.shippingMethods));
        if (shippingMethods.hasException())
            return shippingMethods.releaseException();
        result.setShippingMethods(shippingMethods.releaseReturnValue());
    }

    // FIXME: Merge this validation into the validation we are doing above.
    auto validatedPaymentRequest = PaymentRequestValidator::validate(result);
    if (validatedPaymentRequest.hasException())
        return validatedPaymentRequest.releaseException();

    return WTFMove(result);
}


static Vector<PaymentError> convert(const Vector<RefPtr<ApplePayError>>& errors)
{
    Vector<PaymentError> convertedErrors;

    for (auto& error : errors) {
        PaymentError convertedError;

        convertedError.code = error->code();
        convertedError.message = error->message();
        convertedError.contactField = error->contactField();

        convertedErrors.append(convertedError);
    }

    return convertedErrors;
}

static ExceptionOr<PaymentAuthorizationResult> convertAndValidate(ApplePayPaymentAuthorizationResult&& result)
{
    PaymentAuthorizationResult convertedResult;

    switch (result.status) {
    case ApplePaySession::STATUS_SUCCESS:
        convertedResult.status = PaymentAuthorizationStatus::Success;
        break;

    case ApplePaySession::STATUS_FAILURE:
        convertedResult.status = PaymentAuthorizationStatus::Failure;
        break;

    case ApplePaySession::STATUS_INVALID_BILLING_POSTAL_ADDRESS:
        convertedResult.status = PaymentAuthorizationStatus::Failure;
        convertedResult.errors.append({ PaymentError::Code::BillingContactInvalid, { }, WTF::nullopt });
        break;

    case ApplePaySession::STATUS_INVALID_SHIPPING_POSTAL_ADDRESS:
        convertedResult.status = PaymentAuthorizationStatus::Failure;
        convertedResult.errors.append({ PaymentError::Code::ShippingContactInvalid, { }, PaymentError::ContactField::PostalAddress });
        break;

    case ApplePaySession::STATUS_INVALID_SHIPPING_CONTACT:
        convertedResult.status = PaymentAuthorizationStatus::Failure;
        convertedResult.errors.append({ PaymentError::Code::ShippingContactInvalid, { }, WTF::nullopt });
        break;

    case ApplePaySession::STATUS_PIN_REQUIRED:
        convertedResult.status = PaymentAuthorizationStatus::PINRequired;
        break;

    case ApplePaySession::STATUS_PIN_INCORRECT:
        convertedResult.status = PaymentAuthorizationStatus::PINIncorrect;
        break;

    case ApplePaySession::STATUS_PIN_LOCKOUT:
        convertedResult.status = PaymentAuthorizationStatus::PINLockout;
        break;

    default:
        return Exception { InvalidAccessError };
    }

    convertedResult.errors.appendVector(convert(result.errors));

    return WTFMove(convertedResult);
}

static ExceptionOr<PaymentMethodUpdate> convertAndValidate(ApplePayPaymentMethodUpdate&& update)
{
    PaymentMethodUpdate convertedUpdate;

    auto convertedNewTotal = convertAndValidateTotal(WTFMove(update.newTotal));
    if (convertedNewTotal.hasException())
        return convertedNewTotal.releaseException();
    convertedUpdate.newTotalAndLineItems.total = convertedNewTotal.releaseReturnValue();

    // FIXME: Merge this validation into the validation we are doing above.
    auto validatedTotal = PaymentRequestValidator::validateTotal(convertedUpdate.newTotalAndLineItems.total);
    if (validatedTotal.hasException())
        return validatedTotal.releaseException();

    auto convertedNewLineItems = convertAndValidate(WTFMove(update.newLineItems));
    if (convertedNewLineItems.hasException())
        return convertedNewLineItems.releaseException();

    convertedUpdate.newTotalAndLineItems.lineItems = convertedNewLineItems.releaseReturnValue();

    return WTFMove(convertedUpdate);
}

static ExceptionOr<ShippingContactUpdate> convertAndValidate(ApplePayShippingContactUpdate&& update)
{
    ShippingContactUpdate convertedUpdate;

    convertedUpdate.errors = convert(update.errors);

    auto convertedNewShippingMethods = convertAndValidate(WTFMove(update.newShippingMethods));
    if (convertedNewShippingMethods.hasException())
        return convertedNewShippingMethods.releaseException();
    convertedUpdate.newShippingMethods = convertedNewShippingMethods.releaseReturnValue();

    auto convertedNewTotal = convertAndValidateTotal(WTFMove(update.newTotal));
    if (convertedNewTotal.hasException())
        return convertedNewTotal.releaseException();
    convertedUpdate.newTotalAndLineItems.total = convertedNewTotal.releaseReturnValue();

    // FIXME: Merge this validation into the validation we are doing above.
    auto validatedTotal = PaymentRequestValidator::validateTotal(convertedUpdate.newTotalAndLineItems.total);
    if (validatedTotal.hasException())
        return validatedTotal.releaseException();

    auto convertedNewLineItems = convertAndValidate(WTFMove(update.newLineItems));
    if (convertedNewLineItems.hasException())
        return convertedNewLineItems.releaseException();
    convertedUpdate.newTotalAndLineItems.lineItems = convertedNewLineItems.releaseReturnValue();

    return WTFMove(convertedUpdate);
}

static ExceptionOr<ShippingMethodUpdate> convertAndValidate(ApplePayShippingMethodUpdate&& update)
{
    ShippingMethodUpdate convertedUpdate;

    auto convertedNewTotal = convertAndValidateTotal(WTFMove(update.newTotal));
    if (convertedNewTotal.hasException())
        return convertedNewTotal.releaseException();

    convertedUpdate.newTotalAndLineItems.total = convertedNewTotal.releaseReturnValue();

    // FIXME: Merge this validation into the validation we are doing above.
    auto validatedTotal = PaymentRequestValidator::validateTotal(convertedUpdate.newTotalAndLineItems.total);
    if (validatedTotal.hasException())
        return validatedTotal.releaseException();

    auto convertedNewLineItems = convertAndValidate(WTFMove(update.newLineItems));
    if (convertedNewLineItems.hasException())
        return convertedNewLineItems.releaseException();

    convertedUpdate.newTotalAndLineItems.lineItems = convertedNewLineItems.releaseReturnValue();

    return WTFMove(convertedUpdate);
}

ExceptionOr<Ref<ApplePaySession>> ApplePaySession::create(Document& document, unsigned version, ApplePayPaymentRequest&& paymentRequest)
{
    auto canCall = canCreateSession(document);
    if (canCall.hasException())
        return canCall.releaseException();

    if (!UserGestureIndicator::processingUserGesture())
        return Exception { InvalidAccessError, "Must create a new ApplePaySession from a user gesture handler." };

    if (!document.page())
        return Exception { InvalidAccessError, "Frame is detached" };

    auto convertedPaymentRequest = convertAndValidate(document, version, WTFMove(paymentRequest), document.page()->paymentCoordinator());
    if (convertedPaymentRequest.hasException())
        return convertedPaymentRequest.releaseException();

    return adoptRef(*new ApplePaySession(document, version, convertedPaymentRequest.releaseReturnValue()));
}

ApplePaySession::ApplePaySession(Document& document, unsigned version, ApplePaySessionPaymentRequest&& paymentRequest)
    : ActiveDOMObject { document }
    , m_paymentRequest { WTFMove(paymentRequest) }
    , m_version { version }
{
    ASSERT(document.page()->paymentCoordinator().supportsVersion(document, version));
    suspendIfNeeded();
}

ApplePaySession::~ApplePaySession() = default;

ExceptionOr<bool> ApplePaySession::supportsVersion(Document& document, unsigned version)
{
    if (!version)
        return Exception { InvalidAccessError };

    auto canCall = canCreateSession(document);
    if (canCall.hasException())
        return canCall.releaseException();

    auto* page = document.page();
    if (!page)
        return Exception { InvalidAccessError };

    return page->paymentCoordinator().supportsVersion(document, version);
}

static bool shouldDiscloseApplePayCapability(Document& document)
{
    auto* page = document.page();
    if (!page || page->usesEphemeralSession())
        return false;

    return document.settings().applePayCapabilityDisclosureAllowed();
}

ExceptionOr<bool> ApplePaySession::canMakePayments(Document& document)
{
    auto canCall = canCreateSession(document);
    if (canCall.hasException())
        return canCall.releaseException();

    auto* page = document.page();
    if (!page)
        return Exception { InvalidAccessError };

    return page->paymentCoordinator().canMakePayments(document);
}

ExceptionOr<void> ApplePaySession::canMakePaymentsWithActiveCard(Document& document, const String& merchantIdentifier, Ref<DeferredPromise>&& passedPromise)
{
    auto canCall = canCreateSession(document);
    if (canCall.hasException())
        return canCall.releaseException();

    RefPtr<DeferredPromise> promise(WTFMove(passedPromise));
    if (!shouldDiscloseApplePayCapability(document)) {
        auto* page = document.page();
        if (!page)
            return Exception { InvalidAccessError };

        auto& paymentCoordinator = page->paymentCoordinator();
        bool canMakePayments = paymentCoordinator.canMakePayments(document);

        RunLoop::main().dispatch([promise, canMakePayments]() mutable {
            promise->resolve<IDLBoolean>(canMakePayments);
        });
        return { };
    }

    auto* page = document.page();
    if (!page)
        return Exception { InvalidAccessError };

    auto& paymentCoordinator = page->paymentCoordinator();

    paymentCoordinator.canMakePaymentsWithActiveCard(document, merchantIdentifier, [promise](bool canMakePayments) mutable {
        promise->resolve<IDLBoolean>(canMakePayments);
    });
    return { };
}

ExceptionOr<void> ApplePaySession::openPaymentSetup(Document& document, const String& merchantIdentifier, Ref<DeferredPromise>&& passedPromise)
{
    auto canCall = canCreateSession(document);
    if (canCall.hasException())
        return canCall.releaseException();

    if (!UserGestureIndicator::processingUserGesture())
        return Exception { InvalidAccessError, "Must call ApplePaySession.openPaymentSetup from a user gesture handler." };

    auto* page = document.page();
    if (!page)
        return Exception { InvalidAccessError };

    auto& paymentCoordinator = page->paymentCoordinator();

    RefPtr<DeferredPromise> promise(WTFMove(passedPromise));
    paymentCoordinator.openPaymentSetup(document, merchantIdentifier, [promise](bool result) mutable {
        promise->resolve<IDLBoolean>(result);
    });

    return { };
}

ExceptionOr<void> ApplePaySession::begin(Document& document)
{
    if (!canBegin())
        return Exception { InvalidAccessError, "Payment session is already active." };

    if (paymentCoordinator().hasActiveSession())
        return Exception { InvalidAccessError, "Page already has an active payment session." };

    if (!paymentCoordinator().beginPaymentSession(document, *this, m_paymentRequest))
        return Exception { InvalidAccessError, "There is already has an active payment session." };

    m_state = State::Active;

    setPendingActivity(*this);

    return { };
}

ExceptionOr<void> ApplePaySession::abort()
{
    if (!canAbort())
        return Exception { InvalidAccessError };

    m_state = State::Aborted;
    paymentCoordinator().abortPaymentSession();

    didReachFinalState();

    return { };
}

ExceptionOr<void> ApplePaySession::completeMerchantValidation(JSC::ExecState& state, JSC::JSValue merchantSessionValue)
{
    if (!canCompleteMerchantValidation())
        return Exception { InvalidAccessError };

    if (!merchantSessionValue.isObject())
        return Exception { TypeError };

    auto& document = *downcast<Document>(scriptExecutionContext());
    auto& window = *document.domWindow();

    String errorMessage;
    auto merchantSession = PaymentMerchantSession::fromJS(state, asObject(merchantSessionValue), errorMessage);
    if (!merchantSession) {
        window.printErrorMessage(errorMessage);
        return Exception { InvalidAccessError };
    }

    m_merchantValidationState = MerchantValidationState::ValidationComplete;
    paymentCoordinator().completeMerchantValidation(*merchantSession);

    return { };
}

ExceptionOr<void> ApplePaySession::completeShippingMethodSelection(ApplePayShippingMethodUpdate&& update)
{
    if (!canCompleteShippingMethodSelection())
        return Exception { InvalidAccessError };

    auto convertedUpdate = convertAndValidate(WTFMove(update));
    if (convertedUpdate.hasException())
        return convertedUpdate.releaseException();

    m_state = State::Active;
    paymentCoordinator().completeShippingMethodSelection(convertedUpdate.releaseReturnValue());

    return { };
}

ExceptionOr<void> ApplePaySession::completeShippingContactSelection(ApplePayShippingContactUpdate&& update)
{
    if (!canCompleteShippingContactSelection())
        return Exception { InvalidAccessError };

    auto convertedUpdate = convertAndValidate(WTFMove(update));
    if (convertedUpdate.hasException())
        return convertedUpdate.releaseException();

    m_state = State::Active;
    paymentCoordinator().completeShippingContactSelection(convertedUpdate.releaseReturnValue());

    return { };
}

ExceptionOr<void> ApplePaySession::completePaymentMethodSelection(ApplePayPaymentMethodUpdate&& update)
{
    if (!canCompletePaymentMethodSelection())
        return Exception { InvalidAccessError };

    auto convertedUpdate = convertAndValidate(WTFMove(update));
    if (convertedUpdate.hasException())
        return convertedUpdate.releaseException();

    m_state = State::Active;
    paymentCoordinator().completePaymentMethodSelection(convertedUpdate.releaseReturnValue());

    return { };
}

ExceptionOr<void> ApplePaySession::completePayment(ApplePayPaymentAuthorizationResult&& result)
{
    if (!canCompletePayment())
        return Exception { InvalidAccessError };

    auto convertedResultOrException = convertAndValidate(WTFMove(result));
    if (convertedResultOrException.hasException())
        return convertedResultOrException.releaseException();

    auto&& convertedResult = convertedResultOrException.releaseReturnValue();
    bool isFinalState = isFinalStateResult(convertedResult);

    paymentCoordinator().completePaymentSession(WTFMove(convertedResult));

    if (!isFinalState) {
        m_state = State::Active;
        return { };
    }

    m_state = State::Completed;
    unsetPendingActivity(*this);

    return { };
}

ExceptionOr<void> ApplePaySession::completeShippingMethodSelection(unsigned short status, ApplePayLineItem&& newTotal, Vector<ApplePayLineItem>&& newLineItems)
{
    ApplePayShippingMethodUpdate update;

    switch (status) {
    case ApplePaySession::STATUS_SUCCESS:
        break;

    case ApplePaySession::STATUS_FAILURE:
    case ApplePaySession::STATUS_INVALID_BILLING_POSTAL_ADDRESS:
    case ApplePaySession::STATUS_INVALID_SHIPPING_POSTAL_ADDRESS:
    case ApplePaySession::STATUS_INVALID_SHIPPING_CONTACT:
    case ApplePaySession::STATUS_PIN_REQUIRED:
    case ApplePaySession::STATUS_PIN_INCORRECT:
    case ApplePaySession::STATUS_PIN_LOCKOUT:
        // This is a fatal error. Cancel the request.
        m_state = State::CancelRequested;
        paymentCoordinator().cancelPaymentSession();
        return { };

    default:
        return Exception { InvalidAccessError };
    }

    update.newTotal = WTFMove(newTotal);
    update.newLineItems = WTFMove(newLineItems);

    return completeShippingMethodSelection(WTFMove(update));
}

ExceptionOr<void> ApplePaySession::completeShippingContactSelection(unsigned short status, Vector<ApplePayShippingMethod>&& newShippingMethods, ApplePayLineItem&& newTotal, Vector<ApplePayLineItem>&& newLineItems)
{
    ApplePayShippingContactUpdate update;

    Optional<ApplePayError::Code> errorCode;
    Optional<ApplePayError::ContactField> contactField;

    switch (status) {
    case ApplePaySession::STATUS_SUCCESS:
        break;

    case ApplePaySession::STATUS_FAILURE:
    case ApplePaySession::STATUS_PIN_REQUIRED:
    case ApplePaySession::STATUS_PIN_INCORRECT:
    case ApplePaySession::STATUS_PIN_LOCKOUT:
        errorCode = ApplePayError::Code::Unknown;
        break;

    case ApplePaySession::STATUS_INVALID_BILLING_POSTAL_ADDRESS:
        errorCode = ApplePayError::Code::BillingContactInvalid;
        break;

    case ApplePaySession::STATUS_INVALID_SHIPPING_POSTAL_ADDRESS:
        errorCode = ApplePayError::Code::ShippingContactInvalid;
        contactField = ApplePayError::ContactField::PostalAddress;
        break;

    case ApplePaySession::STATUS_INVALID_SHIPPING_CONTACT:
        errorCode = ApplePayError::Code::ShippingContactInvalid;
        break;

    default:
        return Exception { InvalidAccessError };
    }

    if (errorCode)
        update.errors = { ApplePayError::create(*errorCode, contactField, { }) };

    update.newShippingMethods = WTFMove(newShippingMethods);
    update.newTotal = WTFMove(newTotal);
    update.newLineItems = WTFMove(newLineItems);

    return completeShippingContactSelection(WTFMove(update));
}

ExceptionOr<void> ApplePaySession::completePaymentMethodSelection(ApplePayLineItem&& newTotal, Vector<ApplePayLineItem>&& newLineItems)
{
    ApplePayPaymentMethodUpdate update;

    update.newTotal = WTFMove(newTotal);
    update.newLineItems = WTFMove(newLineItems);

    return completePaymentMethodSelection(WTFMove(update));
}

ExceptionOr<void> ApplePaySession::completePayment(unsigned short status)
{
    ApplePayPaymentAuthorizationResult result;
    result.status = status;

    return completePayment(WTFMove(result));
}

unsigned ApplePaySession::version() const
{
    return m_version;
}

void ApplePaySession::validateMerchant(URL&& validationURL)
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

    auto event = ApplePayValidateMerchantEvent::create(eventNames().validatemerchantEvent, WTFMove(validationURL));
    dispatchEvent(event.get());
}

void ApplePaySession::didAuthorizePayment(const Payment& payment)
{
    ASSERT(m_state == State::Active);

    m_state = State::Authorized;

    auto event = ApplePayPaymentAuthorizedEvent::create(eventNames().paymentauthorizedEvent, version(), payment);
    dispatchEvent(event.get());
}

void ApplePaySession::didSelectShippingMethod(const ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    ASSERT(m_state == State::Active);

    if (!hasEventListeners(eventNames().shippingmethodselectedEvent)) {
        paymentCoordinator().completeShippingMethodSelection({ });
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
        paymentCoordinator().completeShippingContactSelection({ });
        return;
    }

    m_state = State::ShippingContactSelected;
    auto event = ApplePayShippingContactSelectedEvent::create(eventNames().shippingcontactselectedEvent, version(), shippingContact);
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

void ApplePaySession::didCancelPaymentSession()
{
    ASSERT(canCancel());

    m_state = State::Canceled;

    auto event = Event::create(eventNames().cancelEvent, Event::CanBubble::No, Event::IsCancelable::No);
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
    case State::CancelRequested:
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
    return downcast<Document>(*scriptExecutionContext()).page()->paymentCoordinator();
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
    case State::CancelRequested:
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
    case State::CancelRequested:
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
    case State::CancelRequested:
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
    case State::CancelRequested:
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
    case State::CancelRequested:
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
    case State::CancelRequested:
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
    case State::CancelRequested:
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
    case State::CancelRequested:
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
    unsetPendingActivity(*this);
}

}

#endif
