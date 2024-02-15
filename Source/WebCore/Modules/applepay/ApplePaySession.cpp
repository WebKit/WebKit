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

#include "ApplePayCancelEvent.h"
#include "ApplePayCouponCodeChangedEvent.h"
#include "ApplePayCouponCodeUpdate.h"
#include "ApplePayErrorCode.h"
#include "ApplePayErrorContactField.h"
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
#include "Document.h"
#include "DocumentLoader.h"
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "LinkIconCollector.h"
#include "LinkIconType.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PaymentContact.h"
#include "PaymentCoordinator.h"
#include "PaymentMerchantSession.h"
#include "PaymentMethod.h"
#include "PaymentRequestUtilities.h"
#include "PaymentRequestValidator.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RunLoop.h>

#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
#include <wtf/DateMath.h>
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ApplePaySession);

static ExceptionOr<ApplePayLineItem> convertAndValidateTotal(ApplePayLineItem&& lineItem)
{
    if (!isValidDecimalMonetaryValue(lineItem.amount))
        return Exception { ExceptionCode::TypeError, makeString("\"" + lineItem.amount, "\" is not a valid amount.") };

    auto validatedTotal = PaymentRequestValidator::validateTotal(lineItem);
    if (validatedTotal.hasException())
        return validatedTotal.releaseException();

    return WTFMove(lineItem);
}

static ExceptionOr<ApplePayLineItem> convertAndValidate(ApplePayLineItem&& lineItem)
{
    if (lineItem.type == ApplePayLineItem::Type::Pending) {
        // It is OK for pending types to not have an amount.
        lineItem.amount = nullString();
    } else {
        if (!isValidDecimalMonetaryValue(lineItem.amount))
            return Exception { ExceptionCode::TypeError, makeString("\"" + lineItem.amount, "\" is not a valid amount.") };
    }

    return WTFMove(lineItem);
}

static ExceptionOr<Vector<ApplePayLineItem>> convertAndValidate(std::optional<Vector<ApplePayLineItem>>&& lineItems)
{
    Vector<ApplePayLineItem> result;
    if (!lineItems)
        return WTFMove(result);

    result.reserveInitialCapacity(lineItems->size());
    
    for (auto lineItem : lineItems.value()) {
        auto convertedLineItem = convertAndValidate(WTFMove(lineItem));
        if (convertedLineItem.hasException())
            return convertedLineItem.releaseException();
        result.append(convertedLineItem.releaseReturnValue());
    }

    return WTFMove(result);
}

static ExceptionOr<ApplePayShippingMethod> convertAndValidate(ApplePayShippingMethod&& shippingMethod)
{
    if (!isValidDecimalMonetaryValue(shippingMethod.amount))
        return Exception { ExceptionCode::TypeError, makeString("\"" + shippingMethod.amount, "\" is not a valid amount.") };

    return WTFMove(shippingMethod);
}

static ExceptionOr<Vector<ApplePayShippingMethod>> convertAndValidate(Vector<ApplePayShippingMethod>&& shippingMethods)
{
    Vector<ApplePayShippingMethod> result;
    result.reserveInitialCapacity(shippingMethods.size());

    for (auto& shippingMethod : shippingMethods) {
        auto convertedShippingMethod = convertAndValidate(WTFMove(shippingMethod));
        if (convertedShippingMethod.hasException())
            return convertedShippingMethod.releaseException();
        result.append(convertedShippingMethod.releaseReturnValue());
    }

    return WTFMove(result);
}

#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)

static ExceptionOr<ApplePayRecurringPaymentRequest> convertAndValidate(ApplePayRecurringPaymentRequest&& recurringPaymentRequest)
{
    auto& regularBilling = recurringPaymentRequest.regularBilling;
    if (regularBilling.paymentTiming != ApplePayPaymentTiming::Recurring)
        return Exception(ExceptionCode::TypeError, "'regularBilling' must be a 'recurring' line item."_s);
    if (!regularBilling.label)
        return Exception(ExceptionCode::TypeError, "Missing label for 'regularBilling'."_s);
    if (!isValidDecimalMonetaryValue(regularBilling.amount) && regularBilling.type != ApplePayLineItem::Type::Pending)
        return Exception(ExceptionCode::TypeError, makeString('"', regularBilling.amount, "\" is not a valid amount."));

    if (auto& trialBilling = recurringPaymentRequest.trialBilling) {
        if (trialBilling->paymentTiming != ApplePayPaymentTiming::Recurring)
            return Exception(ExceptionCode::TypeError, "'trialBilling' must be a 'recurring' line item."_s);
        if (!trialBilling->label)
            return Exception(ExceptionCode::TypeError, "Missing label for 'trialBilling'."_s);
        if (!isValidDecimalMonetaryValue(trialBilling->amount) && trialBilling->type != ApplePayLineItem::Type::Pending)
            return Exception(ExceptionCode::TypeError, makeString('"', trialBilling->amount, "\" is not a valid amount."));
    }

    if (auto& managementURL = recurringPaymentRequest.managementURL; !URL { managementURL }.isValid())
        return Exception(ExceptionCode::TypeError, makeString('"', managementURL, "\" is not a valid URL."));

    if (auto& tokenNotificationURL = recurringPaymentRequest.tokenNotificationURL; !tokenNotificationURL.isNull() && !URL { tokenNotificationURL }.isValid())
        return Exception(ExceptionCode::TypeError, makeString('"', tokenNotificationURL, "\" is not a valid URL."));

    return WTFMove(recurringPaymentRequest);
}

#endif // ENABLE(APPLE_PAY_RECURRING_PAYMENTS)

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)

static ExceptionOr<ApplePayAutomaticReloadPaymentRequest> convertAndValidate(ApplePayAutomaticReloadPaymentRequest&& automaticReloadPaymentRequest)
{
    auto& automaticReloadBilling = automaticReloadPaymentRequest.automaticReloadBilling;
    if (automaticReloadBilling.paymentTiming != ApplePayPaymentTiming::AutomaticReload)
        return Exception(ExceptionCode::TypeError, "'automaticReloadBilling' must be an 'automaticReload' line item."_s);
    if (!automaticReloadBilling.label)
        return Exception(ExceptionCode::TypeError, "Missing label for 'automaticReloadBilling'."_s);
    if (!isValidDecimalMonetaryValue(automaticReloadBilling.amount) && automaticReloadBilling.type != ApplePayLineItem::Type::Pending)
        return Exception(ExceptionCode::TypeError, makeString('"', automaticReloadBilling.amount, "\" is not a valid amount."));
    if (!isValidDecimalMonetaryValue(automaticReloadBilling.automaticReloadPaymentThresholdAmount))
        return Exception(ExceptionCode::TypeError, makeString('"', automaticReloadBilling.automaticReloadPaymentThresholdAmount, "\" is not a valid automaticReloadPaymentThresholdAmount."));

    if (auto& managementURL = automaticReloadPaymentRequest.managementURL; !URL { managementURL }.isValid())
        return Exception(ExceptionCode::TypeError, makeString('"', managementURL, "\" is not a valid URL."));

    if (auto& tokenNotificationURL = automaticReloadPaymentRequest.tokenNotificationURL; !tokenNotificationURL.isNull() && !URL { tokenNotificationURL }.isValid())
        return Exception(ExceptionCode::TypeError, makeString('"', tokenNotificationURL, "\" is not a valid URL."));

    return WTFMove(automaticReloadPaymentRequest);
}

#endif // ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)

#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)

static ExceptionOr<ApplePayPaymentTokenContext> convertAndValidate(ApplePayPaymentTokenContext&& tokenContext)
{
    if (!isValidDecimalMonetaryValue(tokenContext.amount))
        return Exception { ExceptionCode::TypeError, makeString("\"" + tokenContext.amount, "\" is not a valid amount.") };

    return WTFMove(tokenContext);
}

static ExceptionOr<Vector<ApplePayPaymentTokenContext>> convertAndValidate(Vector<ApplePayPaymentTokenContext>&& tokenContexts)
{
    Vector<ApplePayPaymentTokenContext> result;
    result.reserveInitialCapacity(tokenContexts.size());

    for (auto& tokenContext : tokenContexts) {
        auto convertedTokenContext = convertAndValidate(WTFMove(tokenContext));
        if (convertedTokenContext.hasException())
            return convertedTokenContext.releaseException();
        result.append(convertedTokenContext.releaseReturnValue());
    }

    return WTFMove(result);
}

#endif // ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)

#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)

ExceptionOr<void> ApplePayDeferredPaymentRequest::validate() const
{
    if (deferredBilling.paymentTiming != ApplePayPaymentTiming::Deferred)
        return Exception(ExceptionCode::TypeError, "'deferredBilling' must be a 'deferred' line item."_s);
    if (!deferredBilling.label)
        return Exception(ExceptionCode::TypeError, "Missing label for 'deferredBilling'."_s);
    if (!isValidDecimalMonetaryValue(deferredBilling.amount) && deferredBilling.type != ApplePayLineItem::Type::Pending)
        return Exception(ExceptionCode::TypeError, makeString('"', deferredBilling.amount, "\" is not a valid amount."));

    if (!URL { managementURL }.isValid())
        return Exception(ExceptionCode::TypeError, makeString('"', managementURL, "\" is not a valid URL."));

    if (freeCancellationDate.isNaN()) {
        if (!freeCancellationDateTimeZone.isEmpty())
            return Exception(ExceptionCode::TypeError, "Unexpected 'freeCancellationDateTimeZone' when 'freeCancellationDate' is not set."_s);
    } else if (freeCancellationDateTimeZone.isEmpty())
        return Exception(ExceptionCode::TypeError, "Missing 'freeCancellationDateTimeZone' when 'freeCancellationDate' is set."_s);
    else if (!isTimeZoneValid(freeCancellationDateTimeZone))
        return Exception(ExceptionCode::TypeError, makeString('"', freeCancellationDateTimeZone, "\" is not a valid time zone."));

    if (paymentDescription.isEmpty())
        return Exception(ExceptionCode::TypeError, "Missing 'paymentDescription'."_s);

    if (!tokenNotificationURL.isNull() && !URL { tokenNotificationURL }.isValid())
        return Exception(ExceptionCode::TypeError, makeString('"', tokenNotificationURL, "\" is not a valid URL."));

    return { };
}

ExceptionOr<ApplePayDeferredPaymentRequest> ApplePayDeferredPaymentRequest::convertAndValidate() &&
{
    auto validity = validate();
    if (validity.hasException())
        return validity.releaseException();

    return WTFMove(*this);
}

#endif // ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)

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

#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    if (paymentRequest.recurringPaymentRequest) {
        auto recurringPaymentRequest = convertAndValidate(WTFMove(*paymentRequest.recurringPaymentRequest));
        if (recurringPaymentRequest.hasException())
            return recurringPaymentRequest.releaseException();
        result.setRecurringPaymentRequest(recurringPaymentRequest.releaseReturnValue());
    }
#endif

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    if (paymentRequest.automaticReloadPaymentRequest) {
        auto automaticReloadPaymentRequest = convertAndValidate(WTFMove(*paymentRequest.automaticReloadPaymentRequest));
        if (automaticReloadPaymentRequest.hasException())
            return automaticReloadPaymentRequest.releaseException();
        result.setAutomaticReloadPaymentRequest(automaticReloadPaymentRequest.releaseReturnValue());
    }
#endif

#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    if (paymentRequest.multiTokenContexts) {
        auto multiTokenContexts = convertAndValidate(WTFMove(*paymentRequest.multiTokenContexts));
        if (multiTokenContexts.hasException())
            return multiTokenContexts.releaseException();
        result.setMultiTokenContexts(multiTokenContexts.releaseReturnValue());
    }
#endif

#if ENABLE(APPLE_PAY_DEFERRED_PAYMENTS)
    if (paymentRequest.deferredPaymentRequest) {
        auto deferredPaymentRequest = WTFMove(*paymentRequest.deferredPaymentRequest).convertAndValidate();
        if (deferredPaymentRequest.hasException())
            return deferredPaymentRequest.releaseException();
        result.setDeferredPaymentRequest(deferredPaymentRequest.releaseReturnValue());
    }
#endif

    // FIXME: Merge this validation into the validation we are doing above.
    constexpr OptionSet fieldsToValidate = {
        PaymentRequestValidator::Field::MerchantCapabilities,
        PaymentRequestValidator::Field::SupportedNetworks,
        PaymentRequestValidator::Field::CountryCode,
        PaymentRequestValidator::Field::CurrencyCode,
        PaymentRequestValidator::Field::Total,
        PaymentRequestValidator::Field::ShippingMethods,
    };
    auto validatedPaymentRequest = PaymentRequestValidator::validate(result, fieldsToValidate);
    if (validatedPaymentRequest.hasException())
        return validatedPaymentRequest.releaseException();

    return WTFMove(result);
}

#if HAVE(APPLE_PAY_PAYMENT_ORDER_DETAILS)

static ExceptionOr<ApplePayPaymentOrderDetails> convertAndValidate(ApplePayPaymentOrderDetails&& orderDetails)
{
    if (auto& webServiceURL = orderDetails.webServiceURL; !URL { webServiceURL }.isValid())
        return Exception(ExceptionCode::TypeError, makeString('"', webServiceURL, "\" is not a valid URL."));

    return WTFMove(orderDetails);
}

#endif // HAVE(APPLE_PAY_PAYMENT_ORDER_DETAILS)

static ExceptionOr<ApplePayPaymentAuthorizationResult> convertAndValidate(ApplePayPaymentAuthorizationResult&& result)
{
    switch (result.status) {
    case ApplePayPaymentAuthorizationResult::Success:
    case ApplePayPaymentAuthorizationResult::Failure:
    case ApplePayPaymentAuthorizationResult::PINRequired:
    case ApplePayPaymentAuthorizationResult::PINIncorrect:
    case ApplePayPaymentAuthorizationResult::PINLockout:
        break;

    case ApplePayPaymentAuthorizationResult::InvalidBillingPostalAddress:
        result.status = ApplePayPaymentAuthorizationResult::Failure;
        result.errors.insert(0, ApplePayError::create(ApplePayErrorCode::BillingContactInvalid, std::nullopt, nullString()));
        break;

    case ApplePayPaymentAuthorizationResult::InvalidShippingPostalAddress:
        result.status = ApplePayPaymentAuthorizationResult::Failure;
        result.errors.insert(0, ApplePayError::create(ApplePayErrorCode::ShippingContactInvalid, ApplePayErrorContactField::PostalAddress, nullString()));
        break;

    case ApplePayPaymentAuthorizationResult::InvalidShippingContact:
        result.status = ApplePayPaymentAuthorizationResult::Failure;
        result.errors.insert(0, ApplePayError::create(ApplePayErrorCode::ShippingContactInvalid, std::nullopt, nullString()));
        break;

    default:
        return Exception { ExceptionCode::InvalidAccessError };
    }

#if HAVE(APPLE_PAY_PAYMENT_ORDER_DETAILS)
    if (auto orderDetails = WTFMove(result.orderDetails)) {
        auto convertedOrderDetails = convertAndValidate(WTFMove(*orderDetails));
        if (convertedOrderDetails.hasException())
            return convertedOrderDetails.releaseException();
        result.orderDetails = convertedOrderDetails.releaseReturnValue();
    }
#endif

    return WTFMove(result);
}

static ExceptionOr<ApplePayPaymentMethodUpdate> convertAndValidate(ApplePayPaymentMethodUpdate&& update)
{
    auto convertedNewTotal = convertAndValidateTotal(WTFMove(update.newTotal));
    if (convertedNewTotal.hasException())
        return convertedNewTotal.releaseException();
    update.newTotal = convertedNewTotal.releaseReturnValue();

    auto convertedNewLineItems = convertAndValidate(WTFMove(update.newLineItems));
    if (convertedNewLineItems.hasException())
        return convertedNewLineItems.releaseException();
    update.newLineItems = convertedNewLineItems.releaseReturnValue();

    return WTFMove(update);
}

static ExceptionOr<ApplePayShippingContactUpdate> convertAndValidate(ApplePayShippingContactUpdate&& update)
{
    auto convertedNewShippingMethods = convertAndValidate(WTFMove(update.newShippingMethods));
    if (convertedNewShippingMethods.hasException())
        return convertedNewShippingMethods.releaseException();
    update.newShippingMethods = convertedNewShippingMethods.releaseReturnValue();

    auto convertedNewTotal = convertAndValidateTotal(WTFMove(update.newTotal));
    if (convertedNewTotal.hasException())
        return convertedNewTotal.releaseException();
    update.newTotal = convertedNewTotal.releaseReturnValue();

    auto convertedNewLineItems = convertAndValidate(WTFMove(update.newLineItems));
    if (convertedNewLineItems.hasException())
        return convertedNewLineItems.releaseException();
    update.newLineItems = convertedNewLineItems.releaseReturnValue();

    return WTFMove(update);
}

static ExceptionOr<ApplePayShippingMethodUpdate> convertAndValidate(ApplePayShippingMethodUpdate&& update)
{
    auto convertedNewTotal = convertAndValidateTotal(WTFMove(update.newTotal));
    if (convertedNewTotal.hasException())
        return convertedNewTotal.releaseException();
    update.newTotal = convertedNewTotal.releaseReturnValue();

    auto convertedNewLineItems = convertAndValidate(WTFMove(update.newLineItems));
    if (convertedNewLineItems.hasException())
        return convertedNewLineItems.releaseException();
    update.newLineItems = convertedNewLineItems.releaseReturnValue();

    return WTFMove(update);
}

#if ENABLE(APPLE_PAY_COUPON_CODE)

static ExceptionOr<ApplePayCouponCodeUpdate> convertAndValidate(ApplePayCouponCodeUpdate&& update)
{
    auto convertedNewShippingMethods = convertAndValidate(WTFMove(update.newShippingMethods));
    if (convertedNewShippingMethods.hasException())
        return convertedNewShippingMethods.releaseException();
    update.newShippingMethods = convertedNewShippingMethods.releaseReturnValue();

    auto convertedNewTotal = convertAndValidateTotal(WTFMove(update.newTotal));
    if (convertedNewTotal.hasException())
        return convertedNewTotal.releaseException();
    update.newTotal = convertedNewTotal.releaseReturnValue();

    auto convertedNewLineItems = convertAndValidate(WTFMove(update.newLineItems));
    if (convertedNewLineItems.hasException())
        return convertedNewLineItems.releaseException();
    update.newLineItems = convertedNewLineItems.releaseReturnValue();

    return WTFMove(update);
}

#endif // ENABLE(APPLE_PAY_COUPON_CODE)

ExceptionOr<Ref<ApplePaySession>> ApplePaySession::create(Document& document, unsigned version, ApplePayPaymentRequest&& paymentRequest)
{
    auto canCall = canCreateSession(document);
    if (canCall.hasException())
        return canCall.releaseException();

    if (!UserGestureIndicator::processingUserGesture())
        return Exception { ExceptionCode::InvalidAccessError, "Must create a new ApplePaySession from a user gesture handler."_s };

    if (!document.page())
        return Exception { ExceptionCode::InvalidAccessError, "Frame is detached"_s };

    auto convertedPaymentRequest = convertAndValidate(document, version, WTFMove(paymentRequest), document.page()->paymentCoordinator());
    if (convertedPaymentRequest.hasException())
        return convertedPaymentRequest.releaseException();

    auto session = adoptRef(*new ApplePaySession(document, version, convertedPaymentRequest.releaseReturnValue()));
    session->suspendIfNeeded();
    return session;
}

ApplePaySession::ApplePaySession(Document& document, unsigned version, ApplePaySessionPaymentRequest&& paymentRequest)
    : ActiveDOMObject { document }
    , m_paymentRequest { WTFMove(paymentRequest) }
    , m_version { version }
{
    ASSERT(document.page()->paymentCoordinator().supportsVersion(document, version));
}

ApplePaySession::~ApplePaySession() = default;

ExceptionOr<bool> ApplePaySession::supportsVersion(Document& document, unsigned version)
{
    if (!version)
        return Exception { ExceptionCode::InvalidAccessError };

    auto canCall = canCreateSession(document);
    if (canCall.hasException())
        return canCall.releaseException();

    auto* page = document.page();
    if (!page)
        return Exception { ExceptionCode::InvalidAccessError };

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
        return Exception { ExceptionCode::InvalidAccessError };

    return page->paymentCoordinator().canMakePayments();
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
            return Exception { ExceptionCode::InvalidAccessError };

        auto& paymentCoordinator = page->paymentCoordinator();
        bool canMakePayments = paymentCoordinator.canMakePayments();

        RunLoop::main().dispatch([promise, canMakePayments]() mutable {
            promise->resolve<IDLBoolean>(canMakePayments);
        });
        return { };
    }

    auto* page = document.page();
    if (!page)
        return Exception { ExceptionCode::InvalidAccessError };

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
        return Exception { ExceptionCode::InvalidAccessError, "Must call ApplePaySession.openPaymentSetup from a user gesture handler."_s };

    auto* page = document.page();
    if (!page)
        return Exception { ExceptionCode::InvalidAccessError };

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
        return Exception { ExceptionCode::InvalidAccessError, "Payment session is already active."_s };

    if (paymentCoordinator().hasActiveSession())
        return Exception { ExceptionCode::InvalidAccessError, "Page already has an active payment session."_s };

    if (!paymentCoordinator().beginPaymentSession(document, *this, m_paymentRequest))
        return Exception { ExceptionCode::InvalidAccessError, "There is already has an active payment session."_s };

    m_state = State::Active;

    return { };
}

ExceptionOr<void> ApplePaySession::abort()
{
    if (!canAbort())
        return Exception { ExceptionCode::InvalidAccessError };

    m_state = State::Aborted;
    paymentCoordinator().abortPaymentSession();

    return { };
}

ExceptionOr<void> ApplePaySession::completeMerchantValidation(JSC::JSGlobalObject& state, JSC::JSValue merchantSessionValue)
{
    if (!canCompleteMerchantValidation())
        return Exception { ExceptionCode::InvalidAccessError };

    if (!merchantSessionValue.isObject())
        return Exception { ExceptionCode::TypeError };

    auto& document = *downcast<Document>(scriptExecutionContext());
    auto& window = *document.domWindow();

    String errorMessage;
    auto merchantSession = PaymentMerchantSession::fromJS(state, asObject(merchantSessionValue), errorMessage);
    if (!merchantSession) {
        window.printErrorMessage(errorMessage);
        return Exception { ExceptionCode::InvalidAccessError };
    }

    // PaymentMerchantSession::fromJS() may run JS, which may abort the request so we need to
    // make sure we still have an active session.
    if (!paymentCoordinator().hasActiveSession())
        return Exception { ExceptionCode::InvalidStateError };

    m_merchantValidationState = MerchantValidationState::ValidationComplete;
    paymentCoordinator().completeMerchantValidation(*merchantSession);

    return { };
}

ExceptionOr<void> ApplePaySession::completeShippingMethodSelection(ApplePayShippingMethodUpdate&& update)
{
    if (!canCompleteShippingMethodSelection())
        return Exception { ExceptionCode::InvalidAccessError };

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
        return Exception { ExceptionCode::InvalidAccessError };

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
        return Exception { ExceptionCode::InvalidAccessError };

    auto convertedUpdate = convertAndValidate(WTFMove(update));
    if (convertedUpdate.hasException())
        return convertedUpdate.releaseException();

    m_state = State::Active;
    paymentCoordinator().completePaymentMethodSelection(convertedUpdate.releaseReturnValue());

    return { };
}

#if ENABLE(APPLE_PAY_COUPON_CODE)

ExceptionOr<void> ApplePaySession::completeCouponCodeChange(ApplePayCouponCodeUpdate&& update)
{
    if (!canCompleteCouponCodeChange())
        return Exception { ExceptionCode::InvalidAccessError };

    auto convertedUpdate = convertAndValidate(WTFMove(update));
    if (convertedUpdate.hasException())
        return convertedUpdate.releaseException();

    m_state = State::Active;
    paymentCoordinator().completeCouponCodeChange(convertedUpdate.releaseReturnValue());

    return { };
}

#endif // ENABLE(APPLE_PAY_COUPON_CODE)

ExceptionOr<void> ApplePaySession::completePayment(ApplePayPaymentAuthorizationResult&& result)
{
    if (!canCompletePayment())
        return Exception { ExceptionCode::InvalidAccessError };

    auto convertedResultOrException = convertAndValidate(WTFMove(result));
    if (convertedResultOrException.hasException())
        return convertedResultOrException.releaseException();

    auto&& convertedResult = convertedResultOrException.releaseReturnValue();
    bool isFinalState = convertedResult.isFinalState();

    paymentCoordinator().completePaymentSession(WTFMove(convertedResult));

    if (!isFinalState) {
        m_state = State::Active;
        return { };
    }

    m_state = State::Completed;

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
        return Exception { ExceptionCode::InvalidAccessError };
    }

    update.newTotal = WTFMove(newTotal);
    update.newLineItems = WTFMove(newLineItems);

    return completeShippingMethodSelection(WTFMove(update));
}

ExceptionOr<void> ApplePaySession::completeShippingContactSelection(unsigned short status, Vector<ApplePayShippingMethod>&& newShippingMethods, ApplePayLineItem&& newTotal, Vector<ApplePayLineItem>&& newLineItems)
{
    ApplePayShippingContactUpdate update;

    std::optional<ApplePayErrorCode> errorCode;
    std::optional<ApplePayErrorContactField> contactField;

    switch (status) {
    case ApplePaySession::STATUS_SUCCESS:
        break;

    case ApplePaySession::STATUS_FAILURE:
    case ApplePaySession::STATUS_PIN_REQUIRED:
    case ApplePaySession::STATUS_PIN_INCORRECT:
    case ApplePaySession::STATUS_PIN_LOCKOUT:
        errorCode = ApplePayErrorCode::Unknown;
        break;

    case ApplePaySession::STATUS_INVALID_BILLING_POSTAL_ADDRESS:
        errorCode = ApplePayErrorCode::BillingContactInvalid;
        break;

    case ApplePaySession::STATUS_INVALID_SHIPPING_POSTAL_ADDRESS:
        errorCode = ApplePayErrorCode::ShippingContactInvalid;
        contactField = ApplePayErrorContactField::PostalAddress;
        break;

    case ApplePaySession::STATUS_INVALID_SHIPPING_CONTACT:
        errorCode = ApplePayErrorCode::ShippingContactInvalid;
        break;

    default:
        return Exception { ExceptionCode::InvalidAccessError };
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

void ApplePaySession::didSelectShippingMethod(const ApplePayShippingMethod& shippingMethod)
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

#if ENABLE(APPLE_PAY_COUPON_CODE)

void ApplePaySession::didChangeCouponCode(String&& couponCode)
{
    ASSERT(m_state == State::Active);

    if (!hasEventListeners(eventNames().couponcodechangedEvent)) {
        paymentCoordinator().completeCouponCodeChange({ });
        return;
    }

    m_state = State::CouponCodeChanged;
    auto event = ApplePayCouponCodeChangedEvent::create(eventNames().couponcodechangedEvent, WTFMove(couponCode));
    dispatchEvent(event.get());
}

#endif // ENABLE(APPLE_PAY_COUPON_CODE)

void ApplePaySession::didCancelPaymentSession(PaymentSessionError&& error)
{
    ASSERT(canCancel());

    m_state = State::Canceled;

    auto event = ApplePayCancelEvent::create(eventNames().cancelEvent, WTFMove(error));
    dispatchEvent(event.get());
}

const char* ApplePaySession::activeDOMObjectName() const
{
    return "ApplePaySession";
}

bool ApplePaySession::canSuspendWithoutCanceling() const
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
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
}

void ApplePaySession::suspend(ReasonForSuspension reason)
{
    if (reason != ReasonForSuspension::BackForwardCache)
        return;

    if (canSuspendWithoutCanceling())
        return;

    auto jsWrapperProtector = makePendingActivity(*this);
    m_state = State::Canceled;
    paymentCoordinator().abortPaymentSession();
    queueTaskToDispatchEvent(*this, TaskSource::UserInteraction, ApplePayCancelEvent::create(eventNames().cancelEvent, { }));
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
    case State::CancelRequested:
        return false;

    case State::PaymentMethodSelected:
        return true;
    }
}

#if ENABLE(APPLE_PAY_COUPON_CODE)

bool ApplePaySession::canCompleteCouponCodeChange() const
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
    case State::PaymentMethodSelected:
        return false;

    case State::CouponCodeChanged:
        return true;
    }
}

#endif // ENABLE(APPLE_PAY_COUPON_CODE)

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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    case State::CouponCodeChanged:
#endif
    case State::Authorized:
    case State::CancelRequested:
        return false;

    case State::Completed:
    case State::Aborted:
    case State::Canceled:
        return true;
    }
}

bool ApplePaySession::virtualHasPendingActivity() const
{
    return m_state != State::Idle && !isFinalState();
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
