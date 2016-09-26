/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
    bool canMakePayments() override;
    void canMakePaymentsWithActiveCard(const String&, const String&, std::function<void (bool)> completionHandler) override;
    void openPaymentSetup(const String& merchantIdentifier, const String& domainName, std::function<void (bool)> completionHandler) override;
    bool showPaymentUI(const WebCore::URL&, const Vector<WebCore::URL>& linkIconURLs, const WebCore::PaymentRequest&) override;
    void completeMerchantValidation(const WebCore::PaymentMerchantSession&) override;
    void completeShippingMethodSelection(WebCore::PaymentAuthorizationStatus, Optional<WebCore::PaymentRequest::TotalAndLineItems> newTotalAndItems) override;
    void completeShippingContactSelection(WebCore::PaymentAuthorizationStatus, const Vector<WebCore::PaymentRequest::ShippingMethod>&, Optional<WebCore::PaymentRequest::TotalAndLineItems> newTotalAndItems) override;
    void completePaymentMethodSelection(Optional<WebCore::PaymentRequest::TotalAndLineItems>) override;
    void completePaymentSession(WebCore::PaymentAuthorizationStatus) override;
    void abortPaymentSession() override;
    void paymentCoordinatorDestroyed() override;
};

#endif
