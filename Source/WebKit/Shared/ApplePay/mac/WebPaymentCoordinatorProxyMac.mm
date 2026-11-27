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

#if PLATFORM(MAC) && ENABLE(APPLE_PAY)

#import "AppKitSPI.h"
#import "DisbursementRequest.h"
#import "PaymentAuthorizationViewController.h"
#import "WebPageProxy.h"
#import <WebCore/ApplePayDisbursementRequest.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <pal/cocoa/PassKitSoftLink.h>
#import <wtf/BlockPtr.h>

namespace WebKit {

using namespace WebCore;

void WebPaymentCoordinatorProxy::platformCanMakePayments(CompletionHandler<void(bool)>&& completionHandler)
{
#if HAVE(PASSKIT_MODULARIZATION) && HAVE(PASSKIT_MAC_HELPER_TEMP)
    if (!PAL::isPassKitMacHelperTempFrameworkAvailable())
#elif HAVE(PASSKIT_MODULARIZATION)
    if (!PAL::isPassKitMacHelperFrameworkAvailable())
#else
    if (!PAL::isPassKitCoreFrameworkAvailable())
#endif
        return completionHandler(false);

    m_canMakePaymentsQueue->dispatch([theClass = retainPtr(PAL::getPKPaymentAuthorizationViewControllerClassSingleton()), completionHandler = WTF::move(completionHandler)]() mutable {
        RunLoop::mainSingleton().dispatch([canMakePayments = [theClass canMakePayments], completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(canMakePayments);
        });
    });
}

void WebPaymentCoordinatorProxy::platformShowPaymentUI(WebPageProxyIdentifier webPageProxyID, const URL& originatingURL, const Vector<URL>& linkIconURLs, const WebCore::ApplePaySessionPaymentRequest& request, CompletionHandler<void(bool)>&& completionHandler)
{
#if HAVE(PASSKIT_MODULARIZATION) && HAVE(PASSKIT_MAC_HELPER_TEMP)
    if (!PAL::isPassKitMacHelperTempFrameworkAvailable())
#elif HAVE(PASSKIT_MODULARIZATION)
    if (!PAL::isPassKitMacHelperFrameworkAvailable())
#else
    if (!PAL::isPassKitCoreFrameworkAvailable())
#endif
        return completionHandler(false);

    RetainPtr<PKPaymentRequest> paymentRequest;
#if HAVE(PASSKIT_DISBURSEMENTS)
    std::optional<WebCore::ApplePayDisbursementRequest> webDisbursementRequest = request.disbursementRequest();
    if (webDisbursementRequest) {
        auto disbursementRequest = platformDisbursementRequest(request, originatingURL, webDisbursementRequest->requiredRecipientContactFields);
        paymentRequest = RetainPtr<PKPaymentRequest>((PKPaymentRequest *)disbursementRequest.get());
    } else
#endif
        paymentRequest = platformPaymentRequest(originatingURL, linkIconURLs, request);

    protect(m_client)->getPaymentCoordinatorEmbeddingUserAgent(webPageProxyID, [weakThis = WeakPtr { *this }, paymentRequest, completionHandler = WTF::move(completionHandler)](const String& userAgent) mutable {
        RefPtr paymentCoordinatorProxy = weakThis.get();
        if (!paymentCoordinatorProxy)
            return completionHandler(false);

        paymentCoordinatorProxy->platformSetPaymentRequestUserAgent(paymentRequest.get(), userAgent);

        auto showPaymentUIRequestSeed = weakThis->m_showPaymentUIRequestSeed;

        [PAL::getPKPaymentAuthorizationViewControllerClassSingleton() requestViewControllerWithPaymentRequest:paymentRequest.get() completion:makeBlockPtr([paymentRequest, showPaymentUIRequestSeed, weakThis = WTF::move(weakThis), completionHandler = WTF::move(completionHandler)](PKPaymentAuthorizationViewController *viewController, NSError *error) mutable {
            RefPtr paymentCoordinatorProxy = weakThis.get();
            if (!paymentCoordinatorProxy)
                return completionHandler(false);

            if (error) {
                LOG_ERROR("+[PKPaymentAuthorizationViewController requestViewControllerWithPaymentRequest:completion:] error %@", error);

                completionHandler(false);
                return;
            }

            // We've already been asked to hide the payment UI. Don't attempt to show it.
            if (showPaymentUIRequestSeed != paymentCoordinatorProxy->m_showPaymentUIRequestSeed)
                return completionHandler(false);

            RetainPtr presentingWindow = protect(paymentCoordinatorProxy->m_client)->paymentCoordinatorPresentingWindow(*paymentCoordinatorProxy);
            if (!presentingWindow)
                return completionHandler(false);

            ASSERT(viewController);

            paymentCoordinatorProxy->m_authorizationPresenter = PaymentAuthorizationViewController::create(*paymentCoordinatorProxy, paymentRequest.get(), viewController);
#if HAVE(PASSKIT_DELEGATED_WEB_CHECKOUT)
            // FIXME: <rdar://193748128> (Remove delegated web checkout staging code from WebKit)
            if ([paymentRequest respondsToSelector:@selector(usesDelegateSession)])
                protect(paymentCoordinatorProxy->m_authorizationPresenter)->setUsesDelegateValidation([paymentRequest usesDelegateSession]);
#endif

            paymentCoordinatorProxy->beginPaymentSheet(viewController, presentingWindow.get());

            completionHandler(true);
        }).get()];
    });
}

void WebPaymentCoordinatorProxy::beginPaymentSheet(NSViewController *viewController, NSWindow *presentingWindow)
{
    ASSERT(!m_sheetWindow);
    m_sheetWindow = [NSWindow windowWithContentViewController:viewController];
#if HAVE(LIQUID_GLASS)
    [m_sheetWindow setStyleMask:NSWindowStyleMaskAlertWindow | NSWindowStyleMaskTitled];
#endif
    m_sheetWindowWillCloseObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification object:m_sheetWindow.get() queue:nil usingBlock:[weakThis = WeakPtr { *this }](NSNotification *) {
        if (RefPtr protectedThis = weakThis)
            protectedThis->didReachFinalState();
    }];

    [presentingWindow beginSheet:m_sheetWindow.get() completionHandler:nullptr];
}

#if HAVE(PASSKIT_DELEGATED_WEB_CHECKOUT)

void WebPaymentCoordinatorProxy::platformShowDelegatedWebCheckoutUI(WebPageProxyIdentifier, const URL& originatingURL, const Vector<URL>& linkIconURLs, CompletionHandler<void(bool)>&& completionHandler)
{
#if HAVE(PASSKIT_MODULARIZATION) && HAVE(PASSKIT_MAC_HELPER_TEMP)
    if (!PAL::isPassKitMacHelperTempFrameworkAvailable())
#elif HAVE(PASSKIT_MODULARIZATION)
    if (!PAL::isPassKitMacHelperFrameworkAvailable())
#else
    if (!PAL::isPassKitCoreFrameworkAvailable())
#endif
        return completionHandler(false);

    // FIXME: <rdar://193748128> (Remove delegated web checkout staging code from WebKit)
    if (![PAL::getPKPaymentAuthorizationViewControllerClassSingleton() respondsToSelector:@selector(delegatedWebCheckoutViewControllerWithOriginatingURL:thumbnailURLs:)])
        return completionHandler(false);

    RetainPtr<PKPaymentAuthorizationViewController> viewController = [PAL::getPKPaymentAuthorizationViewControllerClassSingleton() delegatedWebCheckoutViewControllerWithOriginatingURL:originatingURL.createNSURL().get() thumbnailURLs:createNSArray(linkIconURLs).get()];
    if (!viewController)
        return completionHandler(false);

    // FIXME: <rdar://193748128> (Remove delegated web checkout staging code from WebKit)
    if (![viewController respondsToSelector:@selector(prepareForDelegatedWebCheckoutPresentationWithCompletion:)])
        return completionHandler(false);

    ASSERT(!m_authorizationPresenter);
    m_authorizationPresenter = PaymentAuthorizationViewController::create(*this, nil, viewController.get());
    protect(m_authorizationPresenter)->setUsesDelegateValidation(true);

    auto showPaymentUIRequestSeed = m_showPaymentUIRequestSeed;
    [viewController prepareForDelegatedWebCheckoutPresentationWithCompletion:makeBlockPtr([viewController, showPaymentUIRequestSeed, weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)](BOOL ready) mutable {
        RefPtr paymentCoordinatorProxy = weakThis;
        if (!paymentCoordinatorProxy || !ready)
            return completionHandler(false);

        if (showPaymentUIRequestSeed != paymentCoordinatorProxy->m_showPaymentUIRequestSeed)
            return completionHandler(false);

        RetainPtr presentingWindow = protect(paymentCoordinatorProxy->m_client)->paymentCoordinatorPresentingWindow(*paymentCoordinatorProxy);
        if (!presentingWindow)
            return completionHandler(false);

        paymentCoordinatorProxy->beginPaymentSheet(viewController.get(), presentingWindow.get());
        completionHandler(true);
    }).get()];
}

#endif // HAVE(PASSKIT_DELEGATED_WEB_CHECKOUT)

void WebPaymentCoordinatorProxy::platformHidePaymentUI()
{
    if (m_state == State::Activating) {
        ++m_showPaymentUIRequestSeed;

        ASSERT(!m_sheetWindow);
#if HAVE(PASSKIT_DELEGATED_WEB_CHECKOUT)
        if (RefPtr authorizationPresenter = std::exchange(m_authorizationPresenter, nullptr))
            authorizationPresenter->dismiss();
#else
        ASSERT(!m_authorizationPresenter);
#endif
        return;
    }

    ASSERT(m_authorizationPresenter);
    ASSERT(m_sheetWindow);

    [[NSNotificationCenter defaultCenter] removeObserver:m_sheetWindowWillCloseObserver.get()];
    m_sheetWindowWillCloseObserver = nullptr;

    [retainPtr([m_sheetWindow sheetParent]) endSheet:m_sheetWindow.get()];

    if (RefPtr authorizationPresenter = m_authorizationPresenter)
        authorizationPresenter->dismiss();

    m_sheetWindow = nullptr;
}

}

#endif
