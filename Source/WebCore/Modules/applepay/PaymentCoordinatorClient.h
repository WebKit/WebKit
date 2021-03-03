/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#include "ApplePaySessionPaymentRequest.h"
#include "ApplePaySetupFeatureWebCore.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Optional.h>

namespace WebCore {

class Document;
class PaymentMerchantSession;
#if ENABLE(APPLE_PAY_PAYMENT_METHOD_MODE)
struct ApplePayPaymentMethodModeUpdate;
#endif // ENABLE(APPLE_PAY_PAYMENT_METHOD_MODE)
struct ApplePayPaymentMethodUpdate;
struct ApplePaySetupConfiguration;
struct ApplePayShippingContactUpdate;
struct ApplePayShippingMethodUpdate;
struct PaymentAuthorizationResult;

class PaymentCoordinatorClient {
public:
    bool supportsVersion(unsigned version);

    virtual Optional<String> validatedPaymentNetwork(const String&) = 0;
    virtual bool canMakePayments() = 0;
    virtual void canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&&) = 0;
    virtual void openPaymentSetup(const String& merchantIdentifier, const String& domainName, CompletionHandler<void(bool)>&&) = 0;

    virtual bool showPaymentUI(const URL& originatingURL, const Vector<URL>& linkIconURLs, const ApplePaySessionPaymentRequest&) = 0;
    virtual void completeMerchantValidation(const PaymentMerchantSession&) = 0;
    virtual void completeShippingMethodSelection(Optional<ApplePayShippingMethodUpdate>&&) = 0;
    virtual void completeShippingContactSelection(Optional<ApplePayShippingContactUpdate>&&) = 0;
    virtual void completePaymentMethodSelection(Optional<ApplePayPaymentMethodUpdate>&&) = 0;
#if ENABLE(APPLE_PAY_PAYMENT_METHOD_MODE)
    virtual void completePaymentMethodModeChange(Optional<ApplePayPaymentMethodModeUpdate>&&) = 0;
#endif // ENABLE(APPLE_PAY_PAYMENT_METHOD_MODE)
    virtual void completePaymentSession(Optional<PaymentAuthorizationResult>&&) = 0;
    virtual void abortPaymentSession() = 0;
    virtual void cancelPaymentSession() = 0;
    virtual void paymentCoordinatorDestroyed() = 0;
    virtual bool supportsUnrestrictedApplePay() const = 0;

    virtual String userAgentScriptsBlockedErrorMessage() const { return { }; }

    virtual bool isMockPaymentCoordinator() const { return false; }
    virtual bool isWebPaymentCoordinator() const { return false; }

    virtual bool isAlwaysOnLoggingAllowed() const { return false; }

    virtual void getSetupFeatures(const ApplePaySetupConfiguration&, const URL&, CompletionHandler<void(Vector<Ref<ApplePaySetupFeature>>&&)>&& completionHandler) { completionHandler({ }); }
    virtual void beginApplePaySetup(const ApplePaySetupConfiguration&, const URL&, Vector<RefPtr<ApplePaySetupFeature>>&&, CompletionHandler<void(bool)>&& completionHandler) { completionHandler(false); }
    virtual void endApplePaySetup() { }

protected:
    virtual ~PaymentCoordinatorClient() = default;
};

}

#endif
