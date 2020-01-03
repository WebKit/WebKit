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

#pragma once

#if ENABLE(APPLE_PAY)

#include "MessageReceiver.h"
#include "MessageSender.h"
#include <WebCore/PaymentCoordinatorClient.h>
#include <WebCore/PaymentHeaders.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace IPC {
class DataReference;
}

namespace WebCore {
class PaymentCoordinator;
class PaymentContact;
class PaymentSessionError;
}

namespace WebKit {

class NetworkProcessConnection;
class WebPage;

class WebPaymentCoordinator final : public WebCore::PaymentCoordinatorClient, private IPC::MessageReceiver, private IPC::MessageSender {
    WTF_MAKE_FAST_ALLOCATED;
public:
    friend class NetworkProcessConnection;
    explicit WebPaymentCoordinator(WebPage&);
    ~WebPaymentCoordinator();

    void networkProcessConnectionClosed();

private:
    // WebCore::PaymentCoordinatorClient.
    Optional<String> validatedPaymentNetwork(const String&) override;
    bool canMakePayments() override;
    void canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&&) override;
    void openPaymentSetup(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&&) override;
    bool showPaymentUI(const URL& originatingURL, const Vector<URL>& linkIconURLs, const WebCore::ApplePaySessionPaymentRequest&) override;
    void completeMerchantValidation(const WebCore::PaymentMerchantSession&) override;
    void completeShippingMethodSelection(Optional<WebCore::ShippingMethodUpdate>&&) override;
    void completeShippingContactSelection(Optional<WebCore::ShippingContactUpdate>&&) override;
    void completePaymentMethodSelection(Optional<WebCore::PaymentMethodUpdate>&&) override;
    void completePaymentSession(Optional<WebCore::PaymentAuthorizationResult>&&) override;

    void abortPaymentSession() override;
    void cancelPaymentSession() override;

    void paymentCoordinatorDestroyed() override;

    bool isWebPaymentCoordinator() const override { return true; }

    bool isAlwaysOnLoggingAllowed() const override;
    bool supportsUnrestrictedApplePay() const override;

    String userAgentScriptsBlockedErrorMessage() const final;

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    // Message handlers.
    void validateMerchant(const String& validationURLString);
    void didAuthorizePayment(const WebCore::Payment&);
    void didSelectShippingMethod(const WebCore::ApplePaySessionPaymentRequest::ShippingMethod&);
    void didSelectShippingContact(const WebCore::PaymentContact&);
    void didSelectPaymentMethod(const WebCore::PaymentMethod&);
    void didCancelPaymentSession(WebCore::PaymentSessionError&&);

    WebCore::PaymentCoordinator& paymentCoordinator();

#if ENABLE(APPLE_PAY_REMOTE_UI)
    bool remoteUIEnabled() const;
#endif

    using AvailablePaymentNetworksSet = HashSet<String, ASCIICaseInsensitiveHash>;
    static AvailablePaymentNetworksSet platformAvailablePaymentNetworks();

    WebPage& m_webPage;

    Optional<AvailablePaymentNetworksSet> m_availablePaymentNetworks;

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebPaymentCoordinatorAdditions.h>
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebPaymentCoordinator)
static bool isType(const WebCore::PaymentCoordinatorClient& paymentCoordinatorClient) { return paymentCoordinatorClient.isWebPaymentCoordinator(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
