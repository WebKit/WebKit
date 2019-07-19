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
#include "PaymentCoordinator.h"

#if ENABLE(APPLE_PAY)

#include "Document.h"
#include "LinkIconCollector.h"
#include "Logging.h"
#include "Page.h"
#include "PaymentAuthorizationStatus.h"
#include "PaymentCoordinatorClient.h"
#include "PaymentSession.h"
#include "UserContentProvider.h"
#include <wtf/CompletionHandler.h>
#include <wtf/URL.h>

#undef RELEASE_LOG_ERROR_IF_ALLOWED
#undef RELEASE_LOG_IF_ALLOWED
#define RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(m_client.isAlwaysOnLoggingAllowed(), ApplePay, "%p - PaymentCoordinator::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_client.isAlwaysOnLoggingAllowed(), ApplePay, "%p - PaymentCoordinator::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

PaymentCoordinator::PaymentCoordinator(PaymentCoordinatorClient& client)
    : m_client { client }
{
}

PaymentCoordinator::~PaymentCoordinator()
{
    m_client.paymentCoordinatorDestroyed();
}

bool PaymentCoordinator::supportsVersion(Document&, unsigned version) const
{
    auto supportsVersion = m_client.supportsVersion(version);
    RELEASE_LOG_IF_ALLOWED("supportsVersion(%d) -> %d", version, supportsVersion);
    return supportsVersion;
}

bool PaymentCoordinator::canMakePayments(Document& document)
{
    auto canMakePayments = m_client.canMakePayments();
    RELEASE_LOG_IF_ALLOWED("canMakePayments() -> %d", canMakePayments);

    if (!canMakePayments)
        return false;

    if (!setApplePayIsActiveIfAllowed(document))
        return false;

    return true;
}

void PaymentCoordinator::canMakePaymentsWithActiveCard(Document& document, const String& merchantIdentifier, WTF::Function<void(bool)>&& completionHandler)
{
    m_client.canMakePaymentsWithActiveCard(merchantIdentifier, document.domain(), [this, weakThis = makeWeakPtr(*this), document = makeWeakPtr(document), completionHandler = WTFMove(completionHandler)](bool canMakePayments) {
        if (!weakThis)
            return completionHandler(false);

        RELEASE_LOG_IF_ALLOWED("canMakePaymentsWithActiveCard() -> %d", canMakePayments);

        if (!canMakePayments)
            return completionHandler(false);

        if (!document || !setApplePayIsActiveIfAllowed(*document))
            return completionHandler(false);

        completionHandler(true);
    });
}

void PaymentCoordinator::openPaymentSetup(Document& document, const String& merchantIdentifier, WTF::Function<void(bool)>&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("openPaymentSetup()");
    m_client.openPaymentSetup(merchantIdentifier, document.domain(), WTFMove(completionHandler));
}

bool PaymentCoordinator::beginPaymentSession(Document& document, PaymentSession& paymentSession, const ApplePaySessionPaymentRequest& paymentRequest)
{
    ASSERT(!m_activeSession);

    if (!setApplePayIsActiveIfAllowed(document))
        return false;

    Vector<URL> linkIconURLs;
    for (auto& icon : LinkIconCollector { document }.iconsOfTypes({ LinkIconType::TouchIcon, LinkIconType::TouchPrecomposedIcon }))
        linkIconURLs.append(icon.url);

    auto showPaymentUI = m_client.showPaymentUI(document.url(), linkIconURLs, paymentRequest);
    RELEASE_LOG_IF_ALLOWED("beginPaymentSession() -> %d", showPaymentUI);
    if (!showPaymentUI)
        return false;

    m_activeSession = &paymentSession;
    return true;
}

void PaymentCoordinator::completeMerchantValidation(const PaymentMerchantSession& paymentMerchantSession)
{
    ASSERT(m_activeSession);
    RELEASE_LOG_IF_ALLOWED("completeMerchantValidation()");
    m_client.completeMerchantValidation(paymentMerchantSession);
}

void PaymentCoordinator::completeShippingMethodSelection(Optional<ShippingMethodUpdate>&& update)
{
    ASSERT(m_activeSession);
    RELEASE_LOG_IF_ALLOWED("completeShippingMethodSelection()");
    m_client.completeShippingMethodSelection(WTFMove(update));
}

void PaymentCoordinator::completeShippingContactSelection(Optional<ShippingContactUpdate>&& update)
{
    ASSERT(m_activeSession);
    RELEASE_LOG_IF_ALLOWED("completeShippingContactSelection()");
    m_client.completeShippingContactSelection(WTFMove(update));
}

void PaymentCoordinator::completePaymentMethodSelection(Optional<PaymentMethodUpdate>&& update)
{
    ASSERT(m_activeSession);
    RELEASE_LOG_IF_ALLOWED("completePaymentMethodSelection()");
    m_client.completePaymentMethodSelection(WTFMove(update));
}

void PaymentCoordinator::completePaymentSession(Optional<PaymentAuthorizationResult>&& result)
{
    ASSERT(m_activeSession);

    bool isFinalState = isFinalStateResult(result);
    RELEASE_LOG_IF_ALLOWED("completePaymentSession() (isFinalState: %d)", isFinalState);
    m_client.completePaymentSession(WTFMove(result));

    if (!isFinalState)
        return;

    m_activeSession = nullptr;
}

void PaymentCoordinator::abortPaymentSession()
{
    ASSERT(m_activeSession);
    RELEASE_LOG_IF_ALLOWED("abortPaymentSession()");
    m_client.abortPaymentSession();
    m_activeSession = nullptr;
}

void PaymentCoordinator::cancelPaymentSession()
{
    ASSERT(m_activeSession);
    RELEASE_LOG_IF_ALLOWED("cancelPaymentSession()");
    m_client.cancelPaymentSession();
}

void PaymentCoordinator::validateMerchant(URL&& validationURL)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    RELEASE_LOG_IF_ALLOWED("validateMerchant()");
    m_activeSession->validateMerchant(WTFMove(validationURL));
}

void PaymentCoordinator::didAuthorizePayment(const Payment& payment)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    RELEASE_LOG_IF_ALLOWED("validateMerchant()");
    m_activeSession->didAuthorizePayment(payment);
}

void PaymentCoordinator::didSelectPaymentMethod(const PaymentMethod& paymentMethod)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    RELEASE_LOG_IF_ALLOWED("didSelectPaymentMethod()");
    m_activeSession->didSelectPaymentMethod(paymentMethod);
}

void PaymentCoordinator::didSelectShippingMethod(const ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    RELEASE_LOG_IF_ALLOWED("didSelectShippingMethod()");
    m_activeSession->didSelectShippingMethod(shippingMethod);
}

void PaymentCoordinator::didSelectShippingContact(const PaymentContact& shippingContact)
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    RELEASE_LOG_IF_ALLOWED("didSelectShippingContact()");
    m_activeSession->didSelectShippingContact(shippingContact);
}

void PaymentCoordinator::didCancelPaymentSession()
{
    if (!m_activeSession) {
        // It's possible that the payment has been aborted already.
        return;
    }

    RELEASE_LOG_IF_ALLOWED("didCancelPaymentSession()");
    m_activeSession->didCancelPaymentSession();
    m_activeSession = nullptr;
}

Optional<String> PaymentCoordinator::validatedPaymentNetwork(Document&, unsigned version, const String& paymentNetwork) const
{
    if (version < 2 && equalIgnoringASCIICase(paymentNetwork, "jcb"))
        return WTF::nullopt;

    if (version < 3 && equalIgnoringASCIICase(paymentNetwork, "carteBancaire"))
        return WTF::nullopt;

    return m_client.validatedPaymentNetwork(paymentNetwork);
}

bool PaymentCoordinator::shouldEnableApplePayAPIs(Document& document) const
{
    if (m_client.supportsUnrestrictedApplePay())
        return true;

    bool shouldEnableAPIs = true;
    document.page()->userContentProvider().forEachUserScript([&](DOMWrapperWorld&, const UserScript&) {
        shouldEnableAPIs = false;
    });

    if (!shouldEnableAPIs)
        RELEASE_LOG_IF_ALLOWED("shouldEnableApplePayAPIs() -> false (user scripts)");

    return shouldEnableAPIs;
}

bool PaymentCoordinator::setApplePayIsActiveIfAllowed(Document& document) const
{
    auto hasEvaluatedUserAgentScripts = document.hasEvaluatedUserAgentScripts();
    auto isRunningUserScripts = document.isRunningUserScripts();
    auto supportsUnrestrictedApplePay = m_client.supportsUnrestrictedApplePay();

    if (!supportsUnrestrictedApplePay && (hasEvaluatedUserAgentScripts || isRunningUserScripts)) {
        ASSERT(!document.isApplePayActive());
        RELEASE_LOG_IF_ALLOWED("setApplePayIsActiveIfAllowed() -> false (hasEvaluatedUserAgentScripts: %d, isRunningUserScripts: %d)", hasEvaluatedUserAgentScripts, isRunningUserScripts);
        return false;
    }

    RELEASE_LOG_IF_ALLOWED("setApplePayIsActiveIfAllowed() -> true (supportsUnrestrictedApplePay: %d)", supportsUnrestrictedApplePay);
    document.setApplePayIsActive();
    return true;
}

bool PaymentCoordinator::shouldAllowUserAgentScripts(Document& document) const
{
    if (m_client.supportsUnrestrictedApplePay() || !document.isApplePayActive())
        return true;

    ASSERT(!document.hasEvaluatedUserAgentScripts());
    ASSERT(!document.isRunningUserScripts());
    RELEASE_LOG_ERROR_IF_ALLOWED("shouldAllowUserAgentScripts() -> false (active session)");
    return false;
}

} // namespace WebCore

#undef RELEASE_LOG_ERROR_IF_ALLOWED
#undef RELEASE_LOG_IF_ALLOWED

#endif // ENABLE(APPLE_PAY)
