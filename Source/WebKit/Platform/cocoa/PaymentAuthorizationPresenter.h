/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if USE(PASSKIT) && ENABLE(APPLE_PAY)

#include <WebCore/ApplePaySessionPaymentRequest.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS UIViewController;
OBJC_CLASS WKPaymentAuthorizationDelegate;

namespace WebCore {
class Payment;
class PaymentContact;
class PaymentMerchantSession;
class PaymentMethod;
class PaymentMethodUpdate;
class PaymentSessionError;
}

namespace WebKit {

class PaymentAuthorizationPresenter : public CanMakeWeakPtr<PaymentAuthorizationPresenter> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PaymentAuthorizationPresenter);
public:
    struct Client {
        virtual ~Client() = default;

        virtual void presenterDidAuthorizePayment(PaymentAuthorizationPresenter&, const WebCore::Payment&) = 0;
        virtual void presenterDidFinish(PaymentAuthorizationPresenter&, WebCore::PaymentSessionError&&) = 0;
        virtual void presenterDidSelectPaymentMethod(PaymentAuthorizationPresenter&, const WebCore::PaymentMethod&) = 0;
        virtual void presenterDidSelectShippingContact(PaymentAuthorizationPresenter&, const WebCore::PaymentContact&) = 0;
        virtual void presenterDidSelectShippingMethod(PaymentAuthorizationPresenter&, const WebCore::ApplePaySessionPaymentRequest::ShippingMethod&) = 0;
        virtual void presenterWillValidateMerchant(PaymentAuthorizationPresenter&, const URL&) = 0;
    };

    virtual ~PaymentAuthorizationPresenter() = default;

    Client& client() { return m_client; }

    void completeMerchantValidation(const WebCore::PaymentMerchantSession&);
    void completePaymentMethodSelection(const Optional<WebCore::PaymentMethodUpdate>&);
    void completePaymentSession(const Optional<WebCore::PaymentAuthorizationResult>&);
    void completeShippingContactSelection(const Optional<WebCore::ShippingContactUpdate>&);
    void completeShippingMethodSelection(const Optional<WebCore::ShippingMethodUpdate>&);

    virtual void dismiss() = 0;
#if PLATFORM(IOS_FAMILY)
    virtual void present(UIViewController *, CompletionHandler<void(bool)>&&) = 0;
#endif

protected:
    explicit PaymentAuthorizationPresenter(Client& client)
        : m_client(client)
    {
    }

    virtual WKPaymentAuthorizationDelegate *platformDelegate() = 0;

private:
    Client& m_client;
};

} // namespace WebKit

#endif // USE(PASSKIT) && ENABLE(APPLE_PAY)
