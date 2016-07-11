/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#include "PaymentRequest.h"
#include <functional>
#include <wtf/Forward.h>

namespace WebCore {

class PaymentMerchantSession;
class URL;
enum class PaymentAuthorizationStatus;

class PaymentCoordinatorClient {
public:
    virtual bool supportsVersion(unsigned version) = 0;
    virtual bool canMakePayments() = 0;
    virtual void canMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, std::function<void (bool)> completionHandler) = 0;

    virtual bool showPaymentUI(const URL& originatingURL, const Vector<URL>& linkIconURLs, const PaymentRequest&) = 0;
    virtual void completeMerchantValidation(const PaymentMerchantSession&) = 0;
    virtual void completeShippingMethodSelection(PaymentAuthorizationStatus, Optional<PaymentRequest::TotalAndLineItems> newTotalAndItems) = 0;
    virtual void completeShippingContactSelection(PaymentAuthorizationStatus, const Vector<PaymentRequest::ShippingMethod>& newShippingMethods, Optional<PaymentRequest::TotalAndLineItems> newTotalAndItems) = 0;
    virtual void completePaymentMethodSelection(Optional<WebCore::PaymentRequest::TotalAndLineItems> newTotalAndItems) = 0;
    virtual void completePaymentSession(PaymentAuthorizationStatus) = 0;
    virtual void abortPaymentSession() = 0;
    virtual void paymentCoordinatorDestroyed() = 0;

protected:
    virtual ~PaymentCoordinatorClient() { }
};

}

#endif
