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

#import "WebPaymentCoordinatorClient.h"

#if ENABLE(APPLE_PAY)

#import <wtf/CompletionHandler.h>
#import <wtf/MainThread.h>
#import <wtf/URL.h>

WebPaymentCoordinatorClient::WebPaymentCoordinatorClient()
{
}

WebPaymentCoordinatorClient::~WebPaymentCoordinatorClient()
{
}

Optional<String> WebPaymentCoordinatorClient::validatedPaymentNetwork(const String&)
{
    return WTF::nullopt;
}

bool WebPaymentCoordinatorClient::canMakePayments()
{
    return false;
}

void WebPaymentCoordinatorClient::canMakePaymentsWithActiveCard(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler)
{
    callOnMainThread([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(false);
    });
}

void WebPaymentCoordinatorClient::openPaymentSetup(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler)
{
    callOnMainThread([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(false);
    });
}

bool WebPaymentCoordinatorClient::showPaymentUI(const URL&, const Vector<URL>&, const WebCore::ApplePaySessionPaymentRequest&)
{
    return false;
}

void WebPaymentCoordinatorClient::completeMerchantValidation(const WebCore::PaymentMerchantSession&)
{
}

void WebPaymentCoordinatorClient::completeShippingMethodSelection(Optional<WebCore::ShippingMethodUpdate>&&)
{
}

void WebPaymentCoordinatorClient::completeShippingContactSelection(Optional<WebCore::ShippingContactUpdate>&&)
{
}

void WebPaymentCoordinatorClient::completePaymentMethodSelection(Optional<WebCore::PaymentMethodUpdate>&&)
{
}

void WebPaymentCoordinatorClient::completePaymentSession(Optional<WebCore::PaymentAuthorizationResult>&&)
{
}

void WebPaymentCoordinatorClient::abortPaymentSession()
{
}

void WebPaymentCoordinatorClient::cancelPaymentSession()
{
}

void WebPaymentCoordinatorClient::paymentCoordinatorDestroyed()
{
    delete this;
}

bool WebPaymentCoordinatorClient::supportsUnrestrictedApplePay() const
{
    return false;
}

#endif
