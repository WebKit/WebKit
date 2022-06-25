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

#import "config.h"
#import "PaymentAuthorizationViewController.h"

#if USE(PASSKIT) && ENABLE(APPLE_PAY)

#import "WKPaymentAuthorizationDelegate.h"
#import <pal/cocoa/PassKitSoftLink.h>

@interface WKPaymentAuthorizationViewControllerDelegate : WKPaymentAuthorizationDelegate <PKPaymentAuthorizationViewControllerDelegate, PKPaymentAuthorizationViewControllerPrivateDelegate>

- (instancetype)initWithRequest:(PKPaymentRequest *)request presenter:(WebKit::PaymentAuthorizationPresenter&)presenter;

@end

@implementation WKPaymentAuthorizationViewControllerDelegate

- (instancetype)initWithRequest:(PKPaymentRequest *)request presenter:(WebKit::PaymentAuthorizationPresenter&)presenter
{
    if (!(self = [super _initWithRequest:request presenter:presenter]))
        return nil;

    return self;
}

- (void)_getPaymentServicesMerchantURL:(void(^)(NSURL *, NSError *))completion
{
    [PAL::getPKPaymentAuthorizationViewControllerClass() paymentServicesMerchantURLForAPIType:[_request APIType] completion:completion];
}

#pragma mark PKPaymentAuthorizationViewControllerDelegate

- (void)paymentAuthorizationViewControllerDidFinish:(PKPaymentAuthorizationViewController *)controller
{
    [self _didFinish];
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didAuthorizePayment:(PKPayment *)payment handler:(void (^)(PKPaymentAuthorizationResult *result))completion
{
    [self _didAuthorizePayment:payment completion:completion];
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectShippingMethod:(PKShippingMethod *)shippingMethod handler:(void (^)(PKPaymentRequestShippingMethodUpdate *update))completion
{
    [self _didSelectShippingMethod:shippingMethod completion:completion];
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectShippingContact:(PKContact *)contact handler:(void (^)(PKPaymentRequestShippingContactUpdate *update))completion
{
    [self _didSelectShippingContact:contact completion:completion];
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectPaymentMethod:(PKPaymentMethod *)paymentMethod handler:(void (^)(PKPaymentRequestPaymentMethodUpdate *update))completion
{
    [self _didSelectPaymentMethod:paymentMethod completion:completion];
}

#if HAVE(PASSKIT_COUPON_CODE)

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didChangeCouponCode:(NSString *)couponCode handler:(void (^)(PKPaymentRequestCouponCodeUpdate *update))completion
{
    [self _didChangeCouponCode:couponCode completion:completion];
}

#endif // HAVE(PASSKIT_COUPON_CODE)

#pragma mark PKPaymentAuthorizationViewControllerDelegatePrivate

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller willFinishWithError:(NSError *)error
{
    [self _willFinishWithError:error];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didRequestMerchantSession:(void(^)(PKPaymentMerchantSession *, NSError *))completion
{
    [self _didRequestMerchantSession:completion];
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

@end

namespace WebKit {

static RetainPtr<PKPaymentAuthorizationViewController> platformViewController(PKPaymentRequest *request, PKPaymentAuthorizationViewController *viewController)
{
#if PLATFORM(IOS_FAMILY)
    ASSERT(!viewController);
    return adoptNS([PAL::allocPKPaymentAuthorizationViewControllerInstance() initWithPaymentRequest:request]);
#else
    return viewController;
#endif
}

PaymentAuthorizationViewController::PaymentAuthorizationViewController(PaymentAuthorizationPresenter::Client& client, PKPaymentRequest *request, PKPaymentAuthorizationViewController *viewController)
    : PaymentAuthorizationPresenter(client)
    , m_viewController(platformViewController(request, viewController))
    , m_delegate(adoptNS([[WKPaymentAuthorizationViewControllerDelegate alloc] initWithRequest:request presenter:*this]))
{
    [m_viewController setDelegate:m_delegate.get()];
    [m_viewController setPrivateDelegate:m_delegate.get()];
}

WKPaymentAuthorizationDelegate *PaymentAuthorizationViewController::platformDelegate()
{
    return m_delegate.get();
}

void PaymentAuthorizationViewController::dismiss()
{
#if PLATFORM(IOS_FAMILY)
    [[m_viewController presentingViewController] dismissViewControllerAnimated:YES completion:nullptr];
#endif
    [m_viewController setDelegate:nil];
    [m_viewController setPrivateDelegate:nil];
    m_viewController = nil;
    [m_delegate invalidate];
    m_delegate = nil;
}

#if PLATFORM(IOS_FAMILY)

void PaymentAuthorizationViewController::present(UIViewController *presentingViewController, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!presentingViewController || !m_viewController)
        return completionHandler(false);

    [presentingViewController presentViewController:m_viewController.get() animated:YES completion:nullptr];
    completionHandler(true);
}

#if ENABLE(APPLE_PAY_REMOTE_UI_USES_SCENE)
void PaymentAuthorizationViewController::presentInScene(const String&, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(false);
}
#endif

#endif

} // namespace WebKit

#endif // USE(PASSKIT) && ENABLE(APPLE_PAY)
