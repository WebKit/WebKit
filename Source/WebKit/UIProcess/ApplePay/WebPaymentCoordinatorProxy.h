/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(APPLE_PAY)

#include "MessageReceiver.h"
#include <WebCore/PaymentHeaders.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebPaymentCoordinatorProxyAdditions.h>
#endif

namespace IPC {
class DataReference;
}

namespace WebCore {
enum class PaymentAuthorizationStatus;
class Payment;
class PaymentContact;
class PaymentMerchantSession;
class PaymentMethod;
}

OBJC_CLASS NSObject;
OBJC_CLASS NSWindow;
OBJC_CLASS PKPaymentAuthorizationViewController;
OBJC_CLASS WKPaymentAuthorizationViewControllerDelegate;

namespace WebKit {

class WebPageProxy;

class WebPaymentCoordinatorProxy : private IPC::MessageReceiver, public CanMakeWeakPtr<WebPaymentCoordinatorProxy> {
public:
    explicit WebPaymentCoordinatorProxy(WebPageProxy&);
    ~WebPaymentCoordinatorProxy();

    void didCancelPaymentSession();
    void validateMerchant(const URL&);
    void didAuthorizePayment(const WebCore::Payment&);
    void didSelectShippingMethod(const WebCore::ApplePaySessionPaymentRequest::ShippingMethod&);
    void didSelectShippingContact(const WebCore::PaymentContact&);
    void didSelectPaymentMethod(const WebCore::PaymentMethod&);

    void hidePaymentUI();

private:
    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    // Message handlers.
    void availablePaymentNetworks(Vector<String>&);
    void canMakePayments(bool& reply);
    void canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, uint64_t requestID);
    void openPaymentSetup(const String& merchantIdentifier, const String& domainName, uint64_t requestID);
    void showPaymentUI(const String& originatingURLString, const Vector<String>& linkIconURLStrings, const WebCore::ApplePaySessionPaymentRequest&, bool& result);
    void completeMerchantValidation(const WebCore::PaymentMerchantSession&);
    void completeShippingMethodSelection(const std::optional<WebCore::ShippingMethodUpdate>&);
    void completeShippingContactSelection(const std::optional<WebCore::ShippingContactUpdate>&);
    void completePaymentMethodSelection(const std::optional<WebCore::PaymentMethodUpdate>&);
    void completePaymentSession(const std::optional<WebCore::PaymentAuthorizationResult>&);
    void abortPaymentSession();
    void cancelPaymentSession();

    bool canBegin() const;
    bool canCancel() const;
    bool canCompletePayment() const;
    bool canAbort() const;

    void didReachFinalState();

    Vector<String> platformAvailablePaymentNetworks();
    bool platformCanMakePayments();
    void platformCanMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, WTF::Function<void (bool)>&& completionHandler);
    void platformOpenPaymentSetup(const String& merchantIdentifier, const String& domainName, WTF::Function<void (bool)>&& completionHandler);
    void platformShowPaymentUI(const URL& originatingURL, const Vector<URL>& linkIconURLs, const WebCore::ApplePaySessionPaymentRequest&, WTF::Function<void(bool)>&& completionHandler);
    void platformCompleteMerchantValidation(const WebCore::PaymentMerchantSession&);
    void platformCompleteShippingMethodSelection(const std::optional<WebCore::ShippingMethodUpdate>&);
    void platformCompleteShippingContactSelection(const std::optional<WebCore::ShippingContactUpdate>&);
    void platformCompletePaymentMethodSelection(const std::optional<WebCore::PaymentMethodUpdate>&);
    void platformCompletePaymentSession(const std::optional<WebCore::PaymentAuthorizationResult>&);

    WebPageProxy& m_webPageProxy;

    enum class State {
        // Idle - Nothing's happening.
        Idle,

        // Activating - Waiting to show the payment UI.
        Activating,

        // Active - Showing payment UI.
        Active,

        // Authorized - Dispatching the authorized event and waiting for the paymentSessionCompleted message.
        Authorized,

        // ShippingMethodSelected - Dispatching the shippingmethodselected event and waiting for a reply.
        ShippingMethodSelected,

        // ShippingContactSelected - Dispatching the shippingcontactselected event and waiting for a reply.
        ShippingContactSelected,

        // PaymentMethodSelected - Dispatching the paymentmethodselected event and waiting for a reply.
        PaymentMethodSelected,
    } m_state;

    enum class MerchantValidationState {
        // Idle - Nothing's happening.
        Idle,

        // Validating - Dispatching the validatemerchant event and waiting for a reply.
        Validating,

        // ValidationComplete - A merchant session has been sent along to PassKit.
        ValidationComplete
    } m_merchantValidationState;

    RetainPtr<PKPaymentAuthorizationViewController> m_paymentAuthorizationViewController;
    RetainPtr<WKPaymentAuthorizationViewControllerDelegate> m_paymentAuthorizationViewControllerDelegate;

#if PLATFORM(MAC)
    uint64_t m_showPaymentUIRequestSeed { 0 };
    RetainPtr<NSWindow> m_sheetWindow;
    RetainPtr<NSObject *> m_sheetWindowWillCloseObserver;
#endif
    
#if defined(WEBPAYMENTCOORDINATORPROXY_ADDITIONS)
WEBPAYMENTCOORDINATORPROXY_ADDITIONS
#undef WEBPAYMENTCOORDINATORPROXY_ADDITIONS
#else
    void finishConstruction(WebPaymentCoordinatorProxy&) { }
#endif
};

}

#endif
