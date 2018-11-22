/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) && ENABLE(APPLE_PAY)

#import "WebPageProxy.h"
#import "WebPaymentCoordinatorProxyCocoa.h"
#import <pal/cocoa/PassKitSoftLink.h>
#import <wtf/BlockPtr.h>

namespace WebKit {

void WebPaymentCoordinatorProxy::platformShowPaymentUI(const WebCore::URL& originatingURL, const Vector<WebCore::URL>& linkIconURLStrings, const WebCore::ApplePaySessionPaymentRequest& request, WTF::Function<void (bool)>&& completionHandler)
{
    auto paymentRequest = toPKPaymentRequest(m_webPageProxy, originatingURL, linkIconURLStrings, request);

    auto showPaymentUIRequestSeed = m_showPaymentUIRequestSeed;
    auto weakThis = makeWeakPtr(*this);
    [PAL::get_PassKit_PKPaymentAuthorizationViewControllerClass() requestViewControllerWithPaymentRequest:paymentRequest.get() completion:BlockPtr<void(PKPaymentAuthorizationViewController *, NSError *)>::fromCallable([paymentRequest, showPaymentUIRequestSeed, weakThis, completionHandler = WTFMove(completionHandler)](PKPaymentAuthorizationViewController *viewController, NSError *error) {
        auto paymentCoordinatorProxy = weakThis.get();
        if (!paymentCoordinatorProxy)
            return;

        if (error) {
            LOG_ERROR("+[PKPaymentAuthorizationViewController requestViewControllerWithPaymentRequest:completion:] error %@", error);

            completionHandler(false);
            return;
        }

        if (showPaymentUIRequestSeed != paymentCoordinatorProxy->m_showPaymentUIRequestSeed) {
            // We've already been asked to hide the payment UI. Don't attempt to show it.
            return;
        }

        ASSERT(viewController);

        paymentCoordinatorProxy->m_paymentAuthorizationViewControllerDelegate = adoptNS([[WKPaymentAuthorizationViewControllerDelegate alloc] initWithPaymentCoordinatorProxy:*paymentCoordinatorProxy]);
        paymentCoordinatorProxy->m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems = [paymentRequest paymentSummaryItems];
        paymentCoordinatorProxy->m_paymentAuthorizationViewControllerDelegate->_shippingMethods = [paymentRequest shippingMethods];
        paymentCoordinatorProxy->m_paymentAuthorizationViewController = viewController;
        [paymentCoordinatorProxy->m_paymentAuthorizationViewController setDelegate:paymentCoordinatorProxy->m_paymentAuthorizationViewControllerDelegate.get()];
        [paymentCoordinatorProxy->m_paymentAuthorizationViewController setPrivateDelegate:paymentCoordinatorProxy->m_paymentAuthorizationViewControllerDelegate.get()];

        ASSERT(!paymentCoordinatorProxy->m_sheetWindow);
        paymentCoordinatorProxy->m_sheetWindow = [NSWindow windowWithContentViewController:viewController];

        paymentCoordinatorProxy->m_sheetWindowWillCloseObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification object:paymentCoordinatorProxy->m_sheetWindow.get() queue:nil usingBlock:[paymentCoordinatorProxy](NSNotification *) {
            paymentCoordinatorProxy->hidePaymentUI();
            paymentCoordinatorProxy->didReachFinalState();
        }];

        [paymentCoordinatorProxy->m_webPageProxy.platformWindow() beginSheet:paymentCoordinatorProxy->m_sheetWindow.get() completionHandler:nullptr];

        completionHandler(true);
    }).get()];
}

void WebPaymentCoordinatorProxy::hidePaymentUI()
{
    if (m_state == State::Activating) {
        ++m_showPaymentUIRequestSeed;

        ASSERT(!m_paymentAuthorizationViewController);
        ASSERT(!m_paymentAuthorizationViewControllerDelegate);
        ASSERT(!m_sheetWindow);
        return;
    }

    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);
    ASSERT(m_sheetWindow);

    [[NSNotificationCenter defaultCenter] removeObserver:m_sheetWindowWillCloseObserver.get()];
    m_sheetWindowWillCloseObserver = nullptr;

    [[m_sheetWindow sheetParent] endSheet:m_sheetWindow.get()];
    [m_paymentAuthorizationViewController setDelegate:nil];
    [m_paymentAuthorizationViewController setPrivateDelegate:nil];
    m_paymentAuthorizationViewController = nullptr;

    [m_paymentAuthorizationViewControllerDelegate invalidate];
    m_paymentAuthorizationViewControllerDelegate = nullptr;

    m_sheetWindow = nullptr;
}

}

#endif
