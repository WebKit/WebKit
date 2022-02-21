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
#import <UIKit/UIViewController.h>
#import <pal/cocoa/PassKitSoftLink.h>

namespace WebKit {

void WebPaymentCoordinatorProxy::platformCanMakePayments(CompletionHandler<void(bool)>&& completionHandler)
{
    m_canMakePaymentsQueue->dispatch([theClass = retainPtr(PAL::getPKPaymentAuthorizationControllerClass()), completionHandler = WTFMove(completionHandler)]() mutable {
        RunLoop::main().dispatch([canMakePayments = [theClass canMakePayments], completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(canMakePayments);
        });
    });
}

void WebPaymentCoordinatorProxy::platformShowPaymentUI(WebPageProxyIdentifier webPageProxyID, const URL& originatingURL, const Vector<URL>& linkIconURLStrings, const WebCore::ApplePaySessionPaymentRequest& request, CompletionHandler<void(bool)>&& completionHandler)
{
    auto paymentRequest = platformPaymentRequest(originatingURL, linkIconURLStrings, request);

    ASSERT(!m_authorizationPresenter);
    m_authorizationPresenter = m_client.paymentCoordinatorAuthorizationPresenter(*this, paymentRequest.get());
    if (!m_authorizationPresenter)
        return completionHandler(false);

#if ENABLE(APPLE_PAY_REMOTE_UI_USES_SCENE)
    m_client.getWindowSceneIdentifierForPaymentPresentation(webPageProxyID, [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)](const String& sceneID) mutable {
        if (!weakThis) {
            completionHandler(false);
            return;
        }

        if (!weakThis->m_authorizationPresenter) {
            completionHandler(false);
            return;
        }

        weakThis->m_authorizationPresenter->presentInScene(sceneID, WTFMove(completionHandler));
    });
#else
    UNUSED_PARAM(webPageProxyID);
    m_authorizationPresenter->present(m_client.paymentCoordinatorPresentingViewController(*this), WTFMove(completionHandler));
#endif
}

void WebPaymentCoordinatorProxy::platformHidePaymentUI()
{
    if (m_authorizationPresenter)
        m_authorizationPresenter->dismiss();
}

}

#endif
