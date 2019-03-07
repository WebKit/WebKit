/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebPaymentCoordinatorProxy.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(APPLE_PAY)

#import "APIUIClient.h"
#import "PaymentAuthorizationPresenter.h"
#import "WebPageProxy.h"
#import <PassKit/PassKit.h>
#import <UIKit/UIViewController.h>
#import <WebCore/PaymentAuthorizationStatus.h>
#import <pal/cocoa/PassKitSoftLink.h>

namespace WebKit {

void WebPaymentCoordinatorProxy::platformShowPaymentUI(const URL& originatingURL, const Vector<URL>& linkIconURLStrings, const WebCore::ApplePaySessionPaymentRequest& request, CompletionHandler<void(bool)>&& completionHandler)
{
    auto paymentRequest = platformPaymentRequest(originatingURL, linkIconURLStrings, request);

    ASSERT(!m_authorizationPresenter);
    m_authorizationPresenter = m_client.paymentCoordinatorAuthorizationPresenter(*this, paymentRequest.get());
    if (!m_authorizationPresenter)
        return completionHandler(false);

    m_authorizationPresenter->present(m_client.paymentCoordinatorPresentingViewController(*this), WTFMove(completionHandler));
}

void WebPaymentCoordinatorProxy::hidePaymentUI()
{
    m_authorizationPresenter->dismiss();
    m_authorizationPresenter = nullptr;
}

}

#endif
