/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "WebPageProxy.h"
#import "WebPaymentCoordinatorProxyCocoa.h"
#import <PassKit/PassKit.h>
#import <UIKit/UIViewController.h>
#import <WebCore/PaymentAuthorizationStatus.h>
#import <pal/cocoa/PassKitSoftLink.h>

namespace WebKit {

void WebPaymentCoordinatorProxy::platformShowPaymentUI(const URL& originatingURL, const Vector<URL>& linkIconURLStrings, const WebCore::ApplePaySessionPaymentRequest& request, WTF::Function<void(bool)>&& completionHandler)
{
    UIViewController *presentingViewController = m_webPageProxy.uiClient().presentingViewController();

    if (!presentingViewController) {
        completionHandler(false);
        return;
    }

    ASSERT(!m_paymentAuthorizationViewController);

    auto paymentRequest = toPKPaymentRequest(m_webPageProxy, originatingURL, linkIconURLStrings, request);

    m_paymentAuthorizationViewController = adoptNS([PAL::allocPKPaymentAuthorizationViewControllerInstance() initWithPaymentRequest:paymentRequest.get()]);
    if (!m_paymentAuthorizationViewController) {
        completionHandler(false);
        return;
    }

    m_paymentAuthorizationViewControllerDelegate = adoptNS([[WKPaymentAuthorizationViewControllerDelegate alloc] initWithPaymentCoordinatorProxy:*this]);
    m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems = [paymentRequest paymentSummaryItems];
    m_paymentAuthorizationViewControllerDelegate->_shippingMethods = [paymentRequest shippingMethods];

    [m_paymentAuthorizationViewController setDelegate:m_paymentAuthorizationViewControllerDelegate.get()];
    [m_paymentAuthorizationViewController setPrivateDelegate:m_paymentAuthorizationViewControllerDelegate.get()];

    [presentingViewController presentViewController:m_paymentAuthorizationViewController.get() animated:YES completion:nullptr];

    completionHandler(true);
}

void WebPaymentCoordinatorProxy::hidePaymentUI()
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    [[m_paymentAuthorizationViewController presentingViewController] dismissViewControllerAnimated:YES completion:nullptr];
    [m_paymentAuthorizationViewController setDelegate:nil];
    [m_paymentAuthorizationViewController setPrivateDelegate:nil];
    m_paymentAuthorizationViewController = nullptr;

    [m_paymentAuthorizationViewControllerDelegate invalidate];
    m_paymentAuthorizationViewControllerDelegate = nullptr;
}

}

#endif
