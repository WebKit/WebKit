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
#include <wtf/MonotonicTime.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
class PaymentCoordinator;
class PaymentContact;
class PaymentSessionError;
struct ApplePayShippingMethod;
}

namespace WebKit {

class NetworkProcessConnection;
class WebPage;

class WebPaymentCoordinator final : public WebCore::PaymentCoordinatorClient, public RefCounted<WebPaymentCoordinator>, private IPC::MessageReceiver, private IPC::MessageSender {
    WTF_MAKE_TZONE_ALLOCATED(WebPaymentCoordinator);
public:
    friend class NetworkProcessConnection;
    static Ref<WebPaymentCoordinator> create(WebPage&);
    ~WebPaymentCoordinator();

    void networkProcessConnectionClosed();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    explicit WebPaymentCoordinator(WebPage&);

    // WebCore::PaymentCoordinatorClient.
    std::optional<String> validatedPaymentNetwork(const String&) const override;
    bool canMakePayments() override;
    void canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&&) override;
    void openPaymentSetup(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&&) override;
    bool showPaymentUI(const URL& originatingURL, const Vector<URL>& linkIconURLs, const WebCore::ApplePaySessionPaymentRequest&) override;
    void completeMerchantValidation(const WebCore::PaymentMerchantSession&) override;
    void completeShippingMethodSelection(std::optional<WebCore::ApplePayShippingMethodUpdate>&&) override;
    void completeShippingContactSelection(std::optional<WebCore::ApplePayShippingContactUpdate>&&) override;
    void completePaymentMethodSelection(std::optional<WebCore::ApplePayPaymentMethodUpdate>&&) override;
#if ENABLE(APPLE_PAY_COUPON_CODE)
    void completeCouponCodeChange(std::optional<WebCore::ApplePayCouponCodeUpdate>&&) override;
#endif
    void completePaymentSession(WebCore::ApplePayPaymentAuthorizationResult&&) override;

    void abortPaymentSession() override;
    void cancelPaymentSession() override;

    bool isWebPaymentCoordinator() const override { return true; }

    void getSetupFeatures(const WebCore::ApplePaySetupConfiguration&, const URL&, CompletionHandler<void(Vector<Ref<WebCore::ApplePaySetupFeature>>&&)>&&) final;
    void beginApplePaySetup(const WebCore::ApplePaySetupConfiguration&, const URL&, Vector<Ref<WebCore::ApplePaySetupFeature>>&&, CompletionHandler<void(bool)>&&) final;
    void endApplePaySetup() final;

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    // Message handlers.
    void validateMerchant(const String& validationURLString);
    void didAuthorizePayment(const WebCore::Payment&);
    void didSelectShippingMethod(const WebCore::ApplePayShippingMethod&);
    void didSelectShippingContact(const WebCore::PaymentContact&);
    void didSelectPaymentMethod(const WebCore::PaymentMethod&);
#if ENABLE(APPLE_PAY_COUPON_CODE)
    void didChangeCouponCode(String&& couponCode);
#endif
    void didCancelPaymentSession(WebCore::PaymentSessionError&&);

    WebCore::PaymentCoordinator& paymentCoordinator();

    using AvailablePaymentNetworksSet = HashSet<String, ASCIICaseInsensitiveHash>;
    static AvailablePaymentNetworksSet platformAvailablePaymentNetworks();

    WeakPtr<WebPage> m_webPage;

    mutable std::optional<AvailablePaymentNetworksSet> m_availablePaymentNetworks;

    MonotonicTime m_timestampOfLastCanMakePaymentsRequest;
    std::optional<bool> m_lastCanMakePaymentsResult;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebPaymentCoordinator)
static bool isType(const WebCore::PaymentCoordinatorClient& paymentCoordinatorClient) { return paymentCoordinatorClient.isWebPaymentCoordinator(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
