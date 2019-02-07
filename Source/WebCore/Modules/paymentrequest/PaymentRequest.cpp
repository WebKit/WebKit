/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "PaymentRequest.h"

#if ENABLE(PAYMENT_REQUEST)

#include "ApplePayPaymentHandler.h"
#include "Document.h"
#include "EventNames.h"
#include "JSDOMPromise.h"
#include "JSPaymentDetailsUpdate.h"
#include "JSPaymentResponse.h"
#include "PaymentAddress.h"
#include "PaymentCurrencyAmount.h"
#include "PaymentDetailsInit.h"
#include "PaymentHandler.h"
#include "PaymentMethodChangeEvent.h"
#include "PaymentMethodData.h"
#include "PaymentOptions.h"
#include "PaymentRequestUpdateEvent.h"
#include "PaymentValidationErrors.h"
#include "ScriptController.h"
#include <JavaScriptCore/JSONObject.h>
#include <JavaScriptCore/ThrowScope.h>
#include <wtf/ASCIICType.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/UUID.h>

namespace WebCore {

// Implements the IsWellFormedCurrencyCode abstract operation from ECMA 402
// https://tc39.github.io/ecma402/#sec-iswellformedcurrencycode
static bool isWellFormedCurrencyCode(const String& currency)
{
    if (currency.length() == 3)
        return currency.isAllSpecialCharacters<isASCIIAlpha>();
    return false;
}

// Implements the "valid decimal monetary value" validity checker
// https://www.w3.org/TR/payment-request/#dfn-valid-decimal-monetary-value
static bool isValidDecimalMonetaryValue(StringView value)
{
    enum class State {
        Start,
        Sign,
        Digit,
        Dot,
        DotDigit,
    };

    auto state = State::Start;
    for (auto character : value.codeUnits()) {
        switch (state) {
        case State::Start:
            if (character == '-') {
                state = State::Sign;
                break;
            }

            if (isASCIIDigit(character)) {
                state = State::Digit;
                break;
            }

            return false;

        case State::Sign:
            if (isASCIIDigit(character)) {
                state = State::Digit;
                break;
            }

            return false;

        case State::Digit:
            if (character == '.') {
                state = State::Dot;
                break;
            }

            if (isASCIIDigit(character)) {
                state = State::Digit;
                break;
            }

            return false;

        case State::Dot:
            if (isASCIIDigit(character)) {
                state = State::DotDigit;
                break;
            }

            return false;

        case State::DotDigit:
            if (isASCIIDigit(character)) {
                state = State::DotDigit;
                break;
            }

            return false;
        }
    }

    if (state == State::Digit || state == State::DotDigit)
        return true;

    return false;
}

// Implements the "check and canonicalize amount" validity checker
// https://www.w3.org/TR/payment-request/#dfn-check-and-canonicalize-amount
static ExceptionOr<void> checkAndCanonicalizeAmount(PaymentCurrencyAmount& amount)
{
    if (!isWellFormedCurrencyCode(amount.currency))
        return Exception { RangeError, makeString("\"", amount.currency, "\" is not a valid currency code.") };

    if (!isValidDecimalMonetaryValue(amount.value))
        return Exception { TypeError, makeString("\"", amount.value, "\" is not a valid decimal monetary value.") };

    amount.currency = amount.currency.convertToASCIIUppercase();
    return { };
}

// Implements the "check and canonicalize total" validity checker
// https://www.w3.org/TR/payment-request/#dfn-check-and-canonicalize-total
static ExceptionOr<void> checkAndCanonicalizeTotal(PaymentCurrencyAmount& total)
{
    auto exception = checkAndCanonicalizeAmount(total);
    if (exception.hasException())
        return exception;

    if (total.value[0] == '-')
        return Exception { TypeError, "Total currency values cannot be negative."_s };

    return { };
}

// Implements "validate a standardized payment method identifier"
// https://www.w3.org/TR/payment-method-id/#validity-0
static bool isValidStandardizedPaymentMethodIdentifier(StringView identifier)
{
    enum class State {
        Start,
        Hyphen,
        LowerAlpha,
        Digit,
    };

    auto state = State::Start;
    for (auto character : identifier.codeUnits()) {
        switch (state) {
        case State::Start:
        case State::Hyphen:
            if (isASCIILower(character)) {
                state = State::LowerAlpha;
                break;
            }

            return false;

        case State::LowerAlpha:
        case State::Digit:
            if (isASCIILower(character)) {
                state = State::LowerAlpha;
                break;
            }

            if (isASCIIDigit(character)) {
                state = State::Digit;
                break;
            }

            if (character == '-') {
                state = State::Hyphen;
                break;
            }

            return false;
        }
    }

    return state == State::LowerAlpha || state == State::Digit;
}

// Implements "validate a URL-based payment method identifier"
// https://www.w3.org/TR/payment-method-id/#validation
static bool isValidURLBasedPaymentMethodIdentifier(const URL& url)
{
    if (!url.protocolIs("https"))
        return false;

    if (!url.user().isEmpty() || !url.pass().isEmpty())
        return false;

    return true;
}

// Implements "validate a payment method identifier"
// https://www.w3.org/TR/payment-method-id/#validity
Optional<PaymentRequest::MethodIdentifier> convertAndValidatePaymentMethodIdentifier(const String& identifier)
{
    URL url { URL(), identifier };
    if (!url.isValid()) {
        if (isValidStandardizedPaymentMethodIdentifier(identifier))
            return { identifier };
        return WTF::nullopt;
    }

    if (isValidURLBasedPaymentMethodIdentifier(url))
        return { WTFMove(url) };

    return WTF::nullopt;
}

enum class ShouldValidatePaymentMethodIdentifier {
    No,
    Yes,
};

static ExceptionOr<std::tuple<String, Vector<String>>> checkAndCanonicalizeDetails(JSC::ExecState& execState, PaymentDetailsBase& details, bool requestShipping, ShouldValidatePaymentMethodIdentifier shouldValidatePaymentMethodIdentifier)
{
    for (auto& item : details.displayItems) {
        auto exception = checkAndCanonicalizeAmount(item.amount);
        if (exception.hasException())
            return exception.releaseException();
    }

    String selectedShippingOption;
    if (requestShipping) {
        HashSet<String> seenShippingOptionIDs;
        for (auto& shippingOption : details.shippingOptions) {
            auto exception = checkAndCanonicalizeAmount(shippingOption.amount);
            if (exception.hasException())
                return exception.releaseException();

            auto addResult = seenShippingOptionIDs.add(shippingOption.id);
            if (!addResult.isNewEntry)
                return Exception { TypeError, "Shipping option IDs must be unique." };

            if (shippingOption.selected)
                selectedShippingOption = shippingOption.id;
        }
    }

    Vector<String> serializedModifierData;
    serializedModifierData.reserveInitialCapacity(details.modifiers.size());
    for (auto& modifier : details.modifiers) {
        if (shouldValidatePaymentMethodIdentifier == ShouldValidatePaymentMethodIdentifier::Yes) {
            auto paymentMethodIdentifier = convertAndValidatePaymentMethodIdentifier(modifier.supportedMethods);
            if (!paymentMethodIdentifier)
                return Exception { RangeError, makeString('"', modifier.supportedMethods, "\" is an invalid payment method identifier.") };
        }

        if (modifier.total) {
            auto exception = checkAndCanonicalizeTotal(modifier.total->amount);
            if (exception.hasException())
                return exception.releaseException();
        }

        for (auto& item : modifier.additionalDisplayItems) {
            auto exception = checkAndCanonicalizeAmount(item.amount);
            if (exception.hasException())
                return exception.releaseException();
        }

        String serializedData;
        if (modifier.data) {
            auto scope = DECLARE_THROW_SCOPE(execState.vm());
            serializedData = JSONStringify(&execState, modifier.data.get(), 0);
            if (scope.exception())
                return Exception { ExistingExceptionError };
            modifier.data.clear();
        }
        serializedModifierData.uncheckedAppend(WTFMove(serializedData));
    }

    return std::make_tuple(WTFMove(selectedShippingOption), WTFMove(serializedModifierData));
}

// Implements the PaymentRequest Constructor
// https://www.w3.org/TR/payment-request/#constructor
ExceptionOr<Ref<PaymentRequest>> PaymentRequest::create(Document& document, Vector<PaymentMethodData>&& methodData, PaymentDetailsInit&& details, PaymentOptions&& options)
{
    auto canCreateSession = PaymentHandler::canCreateSession(document);
    if (canCreateSession.hasException())
        return canCreateSession.releaseException();

    if (details.id.isNull())
        details.id = createCanonicalUUIDString();

    if (methodData.isEmpty())
        return Exception { TypeError, "At least one payment method is required."_s };

    Vector<Method> serializedMethodData;
    serializedMethodData.reserveInitialCapacity(methodData.size());
    for (auto& paymentMethod : methodData) {
        auto identifier = convertAndValidatePaymentMethodIdentifier(paymentMethod.supportedMethods);
        if (!identifier)
            return Exception { RangeError, makeString('"', paymentMethod.supportedMethods, "\" is an invalid payment method identifier.") };

        String serializedData;
        if (paymentMethod.data) {
            auto scope = DECLARE_THROW_SCOPE(document.execState()->vm());
            serializedData = JSONStringify(document.execState(), paymentMethod.data.get(), 0);
            if (scope.exception())
                return Exception { ExistingExceptionError };
        }
        serializedMethodData.uncheckedAppend({ WTFMove(*identifier), WTFMove(serializedData) });
    }

    auto totalResult = checkAndCanonicalizeTotal(details.total.amount);
    if (totalResult.hasException())
        return totalResult.releaseException();

    auto detailsResult = checkAndCanonicalizeDetails(*document.execState(), details, options.requestShipping, ShouldValidatePaymentMethodIdentifier::No);
    if (detailsResult.hasException())
        return detailsResult.releaseException();

    auto shippingOptionAndModifierData = detailsResult.releaseReturnValue();
    return adoptRef(*new PaymentRequest(document, WTFMove(options), WTFMove(details), WTFMove(std::get<1>(shippingOptionAndModifierData)), WTFMove(serializedMethodData), WTFMove(std::get<0>(shippingOptionAndModifierData))));
}

PaymentRequest::PaymentRequest(Document& document, PaymentOptions&& options, PaymentDetailsInit&& details, Vector<String>&& serializedModifierData, Vector<Method>&& serializedMethodData, String&& selectedShippingOption)
    : ActiveDOMObject { document }
    , m_options { WTFMove(options) }
    , m_details { WTFMove(details) }
    , m_serializedModifierData { WTFMove(serializedModifierData) }
    , m_serializedMethodData { WTFMove(serializedMethodData) }
    , m_shippingOption { WTFMove(selectedShippingOption) }
{
    suspendIfNeeded();
}

PaymentRequest::~PaymentRequest()
{
    ASSERT(!hasPendingActivity());
    ASSERT(!m_activePaymentHandler);
}

static ExceptionOr<JSC::JSValue> parse(ScriptExecutionContext& context, const String& string)
{
    auto scope = DECLARE_THROW_SCOPE(context.vm());
    JSC::JSValue data = JSONParse(context.execState(), string);
    if (scope.exception())
        return Exception { ExistingExceptionError };
    return WTFMove(data);
}

// https://www.w3.org/TR/payment-request/#show()-method
void PaymentRequest::show(Document& document, RefPtr<DOMPromise>&& detailsPromise, ShowPromise&& promise)
{
    if (!document.frame()) {
        promise.reject(Exception { AbortError });
        return;
    }

    if (!UserGestureIndicator::processingUserGesture()) {
        promise.reject(Exception { SecurityError, "show() must be triggered by user activation." });
        return;
    }

    if (m_state != State::Created) {
        promise.reject(Exception { InvalidStateError });
        return;
    }

    if (PaymentHandler::hasActiveSession(document)) {
        promise.reject(Exception { AbortError });
        return;
    }

    m_state = State::Interactive;
    ASSERT(!m_showPromise);
    m_showPromise = WTFMove(promise);

    RefPtr<PaymentHandler> selectedPaymentHandler;
    for (auto& paymentMethod : m_serializedMethodData) {
        auto data = parse(document, paymentMethod.serializedData);
        if (data.hasException()) {
            settleShowPromise(data.releaseException());
            return;
        }

        auto handler = PaymentHandler::create(document, *this, paymentMethod.identifier);
        if (!handler)
            continue;

        auto result = handler->convertData(data.releaseReturnValue());
        if (result.hasException()) {
            settleShowPromise(result.releaseException());
            return;
        }

        if (!selectedPaymentHandler)
            selectedPaymentHandler = WTFMove(handler);
    }

    if (!selectedPaymentHandler) {
        settleShowPromise(Exception { NotSupportedError });
        return;
    }

    auto exception = selectedPaymentHandler->show();
    if (exception.hasException()) {
        settleShowPromise(exception.releaseException());
        return;
    }

    ASSERT(!m_activePaymentHandler);
    m_activePaymentHandler = PaymentHandlerWithPendingActivity { selectedPaymentHandler.releaseNonNull(), makePendingActivity(*this) };

    if (!detailsPromise)
        return;

    exception = updateWith(UpdateReason::ShowDetailsResolved, detailsPromise.releaseNonNull());
    ASSERT(!exception.hasException());
}

void PaymentRequest::abortWithException(Exception&& exception)
{
    ASSERT(m_state == State::Interactive);
    closeActivePaymentHandler();

    if (m_response)
        m_response->abortWithException(WTFMove(exception));
    else
        settleShowPromise(WTFMove(exception));
}

void PaymentRequest::settleShowPromise(ExceptionOr<PaymentResponse&>&& result)
{
    if (auto showPromise = std::exchange(m_showPromise, WTF::nullopt))
        showPromise->settle(WTFMove(result));
}

void PaymentRequest::closeActivePaymentHandler()
{
    if (auto activePaymentHandler = std::exchange(m_activePaymentHandler, WTF::nullopt))
        activePaymentHandler->paymentHandler->hide();

    m_isUpdating = false;
    m_state = State::Closed;
}

void PaymentRequest::stop()
{
    closeActivePaymentHandler();
    settleShowPromise(Exception { AbortError });
}

// https://www.w3.org/TR/payment-request/#abort()-method
void PaymentRequest::abort(AbortPromise&& promise)
{
    if (m_response && m_response->hasRetryPromise()) {
        promise.reject(Exception { InvalidStateError });
        return;
    }

    if (m_state != State::Interactive) {
        promise.reject(Exception { InvalidStateError });
        return;
    }

    abortWithException(Exception { AbortError });
    promise.resolve();
}

// https://www.w3.org/TR/payment-request/#canmakepayment()-method
void PaymentRequest::canMakePayment(Document& document, CanMakePaymentPromise&& promise)
{
    if (m_state != State::Created) {
        promise.reject(Exception { InvalidStateError });
        return;
    }

    for (auto& paymentMethod : m_serializedMethodData) {
        auto handler = PaymentHandler::create(document, *this, paymentMethod.identifier);
        if (!handler)
            continue;

        handler->canMakePayment([promise = WTFMove(promise)](bool canMakePayment) mutable {
            promise.resolve(canMakePayment);
        });
        return;
    }

    promise.resolve(false);
}

const String& PaymentRequest::id() const
{
    return m_details.id;
}

Optional<PaymentShippingType> PaymentRequest::shippingType() const
{
    if (m_options.requestShipping)
        return m_options.shippingType;
    return WTF::nullopt;
}

bool PaymentRequest::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

void PaymentRequest::shippingAddressChanged(Ref<PaymentAddress>&& shippingAddress)
{
    whenDetailsSettled([this, protectedThis = makeRefPtr(this), shippingAddress = makeRefPtr(shippingAddress.get())]() mutable {
        m_shippingAddress = WTFMove(shippingAddress);
        dispatchEvent(PaymentRequestUpdateEvent::create(eventNames().shippingaddresschangeEvent));
    });
}

void PaymentRequest::shippingOptionChanged(const String& shippingOption)
{
    whenDetailsSettled([this, protectedThis = makeRefPtr(this), shippingOption]() mutable {
        m_shippingOption = shippingOption;
        dispatchEvent(PaymentRequestUpdateEvent::create(eventNames().shippingoptionchangeEvent));
    });
}

void PaymentRequest::paymentMethodChanged(const String& methodName, PaymentMethodChangeEvent::MethodDetailsFunction&& methodDetailsFunction)
{
    whenDetailsSettled([this, protectedThis = makeRefPtr(this), methodName, methodDetailsFunction = WTFMove(methodDetailsFunction)]() mutable {
        auto& eventName = eventNames().paymentmethodchangeEvent;
        if (hasEventListeners(eventName))
            dispatchEvent(PaymentMethodChangeEvent::create(eventName, methodName, WTFMove(methodDetailsFunction)));
        else
            activePaymentHandler()->detailsUpdated(UpdateReason::PaymentMethodChanged, { }, { }, { }, { });
    });
}

ExceptionOr<void> PaymentRequest::updateWith(UpdateReason reason, Ref<DOMPromise>&& promise)
{
    if (m_state != State::Interactive)
        return Exception { InvalidStateError };

    if (m_isUpdating)
        return Exception { InvalidStateError };

    m_isUpdating = true;

    ASSERT(!m_detailsPromise);
    m_detailsPromise = WTFMove(promise);
    m_detailsPromise->whenSettled([this, protectedThis = makeRefPtr(this), reason]() {
        settleDetailsPromise(reason);
    });

    return { };
}

ExceptionOr<void> PaymentRequest::completeMerchantValidation(Event& event, Ref<DOMPromise>&& merchantSessionPromise)
{
    if (m_state != State::Interactive)
        return Exception { InvalidStateError };

    event.stopPropagation();
    event.stopImmediatePropagation();

    m_merchantSessionPromise = WTFMove(merchantSessionPromise);
    m_merchantSessionPromise->whenSettled([this, protectedThis = makeRefPtr(this)]() {
        if (m_state != State::Interactive)
            return;

        if (m_merchantSessionPromise->status() == DOMPromise::Status::Rejected) {
            abortWithException(Exception { AbortError });
            return;
        }

        auto exception = activePaymentHandler()->merchantValidationCompleted(m_merchantSessionPromise->result());
        if (exception.hasException()) {
            abortWithException(exception.releaseException());
            return;
        }
    });

    return { };
}

void PaymentRequest::settleDetailsPromise(UpdateReason reason)
{
    auto scopeExit = makeScopeExit([&] {
        m_isUpdating = false;
        m_isCancelPending = false;
        m_detailsPromise = nullptr;
    });

    if (m_state != State::Interactive)
        return;

    if (m_isCancelPending || m_detailsPromise->status() == DOMPromise::Status::Rejected) {
        abortWithException(Exception { AbortError });
        return;
    }

    auto& context = *m_detailsPromise->scriptExecutionContext();
    auto throwScope = DECLARE_THROW_SCOPE(context.vm());
    auto detailsUpdate = convertDictionary<PaymentDetailsUpdate>(*context.execState(), m_detailsPromise->result());
    if (throwScope.exception()) {
        abortWithException(Exception { ExistingExceptionError });
        return;
    }

    auto totalResult = checkAndCanonicalizeTotal(detailsUpdate.total.amount);
    if (totalResult.hasException()) {
        abortWithException(totalResult.releaseException());
        return;
    }

    auto detailsResult = checkAndCanonicalizeDetails(*context.execState(), detailsUpdate, m_options.requestShipping, ShouldValidatePaymentMethodIdentifier::Yes);
    if (detailsResult.hasException()) {
        abortWithException(detailsResult.releaseException());
        return;
    }

    auto shippingOptionAndModifierData = detailsResult.releaseReturnValue();

    m_details.total = WTFMove(detailsUpdate.total);
    m_details.displayItems = WTFMove(detailsUpdate.displayItems);
    if (m_options.requestShipping) {
        m_details.shippingOptions = WTFMove(detailsUpdate.shippingOptions);
        m_shippingOption = WTFMove(std::get<0>(shippingOptionAndModifierData));
    }

    m_details.modifiers = WTFMove(detailsUpdate.modifiers);
    m_serializedModifierData = WTFMove(std::get<1>(shippingOptionAndModifierData));

    auto result = activePaymentHandler()->detailsUpdated(reason, WTFMove(detailsUpdate.error), WTFMove(detailsUpdate.shippingAddressErrors), WTFMove(detailsUpdate.payerErrors), detailsUpdate.paymentMethodErrors.get());
    if (result.hasException()) {
        abortWithException(result.releaseException());
        return;
    }
}

void PaymentRequest::whenDetailsSettled(std::function<void()>&& callback)
{
    auto whenSettledFunction = [this, callback = WTFMove(callback)] {
        ASSERT(m_state == State::Interactive);
        ASSERT(!m_isUpdating);
        ASSERT(!m_isCancelPending);
        ASSERT_UNUSED(this, this);
        callback();
    };

    if (!m_detailsPromise) {
        whenSettledFunction();
        return;
    }

    m_detailsPromise->whenSettled([this, protectedThis = makeRefPtr(this), whenSettledFunction = WTFMove(whenSettledFunction)] {
        if (m_state == State::Interactive)
            whenSettledFunction();
    });
}

void PaymentRequest::accept(const String& methodName, PaymentResponse::DetailsFunction&& detailsFunction, Ref<PaymentAddress>&& shippingAddress, const String& payerName, const String& payerEmail, const String& payerPhone)
{
    ASSERT(!m_isUpdating);
    ASSERT(m_state == State::Interactive);

    bool isRetry = m_response;
    if (!isRetry) {
        m_response = PaymentResponse::create(scriptExecutionContext(), *this);
        m_response->setRequestId(m_details.id);
    }

    m_response->setMethodName(methodName);
    m_response->setDetailsFunction(WTFMove(detailsFunction));
    m_response->setShippingAddress(m_options.requestShipping ? shippingAddress.ptr() : nullptr);
    m_response->setShippingOption(m_options.requestShipping ? m_shippingOption : String { });
    m_response->setPayerName(m_options.requestPayerName ? payerName : String { });
    m_response->setPayerEmail(m_options.requestPayerEmail ? payerEmail : String { });
    m_response->setPayerPhone(m_options.requestPayerPhone ? payerPhone : String { });

    if (!isRetry)
        settleShowPromise(*m_response);
    else {
        ASSERT(m_response->hasRetryPromise());
        m_response->settleRetryPromise();
    }

    m_state = State::Closed;
}

ExceptionOr<void> PaymentRequest::complete(Optional<PaymentComplete>&& result)
{
    ASSERT(m_state == State::Closed);
    if (!m_activePaymentHandler)
        return Exception { AbortError };

    activePaymentHandler()->complete(WTFMove(result));
    m_activePaymentHandler = WTF::nullopt;
    return { };
}

ExceptionOr<void> PaymentRequest::retry(PaymentValidationErrors&& errors)
{
    ASSERT(m_state == State::Closed);
    if (!m_activePaymentHandler)
        return Exception { AbortError };

    m_state = State::Interactive;
    return activePaymentHandler()->retry(WTFMove(errors));
}

void PaymentRequest::cancel()
{
    m_activePaymentHandler = WTF::nullopt;

    if (m_isUpdating) {
        m_isCancelPending = true;
        return;
    }

    abortWithException(Exception { AbortError });
}

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
