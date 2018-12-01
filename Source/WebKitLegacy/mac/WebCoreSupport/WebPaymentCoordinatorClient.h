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

#import <WebCore/PaymentCoordinatorClient.h>

#if ENABLE(APPLE_PAY)

class WebPaymentCoordinatorClient final : public WebCore::PaymentCoordinatorClient {
public:
    WebPaymentCoordinatorClient();

private:
    ~WebPaymentCoordinatorClient();

    bool supportsVersion(unsigned) override;
    std::optional<String> validatedPaymentNetwork(const String&) override;
    bool canMakePayments() override;
    void canMakePaymentsWithActiveCard(const String&, const String&, WTF::Function<void (bool)>&& completionHandler) override;
    void openPaymentSetup(const String& merchantIdentifier, const String& domainName, WTF::Function<void (bool)>&& completionHandler) override;
    bool showPaymentUI(const URL&, const Vector<URL>& linkIconURLs, const WebCore::ApplePaySessionPaymentRequest&) override;
    void completeMerchantValidation(const WebCore::PaymentMerchantSession&) override;
    void completeShippingMethodSelection(std::optional<WebCore::ShippingMethodUpdate>&&) override;
    void completeShippingContactSelection(std::optional<WebCore::ShippingContactUpdate>&&) override;
    void completePaymentMethodSelection(std::optional<WebCore::PaymentMethodUpdate>&&) override;
    void completePaymentSession(std::optional<WebCore::PaymentAuthorizationResult>&&) override;
    void abortPaymentSession() override;
    void cancelPaymentSession() override;
    void paymentCoordinatorDestroyed() override;
};

#endif
