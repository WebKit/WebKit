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
#import "PaymentAuthorizationController.h"

#if USE(PASSKIT) && PLATFORM(IOS_FAMILY)

#import "WKPaymentAuthorizationDelegate.h"
#import <pal/cocoa/PassKitSoftLink.h>

@interface WKPaymentAuthorizationControllerDelegate : WKPaymentAuthorizationDelegate <PKPaymentAuthorizationControllerDelegate, PKPaymentAuthorizationControllerPrivateDelegate>

- (instancetype)initWithRequest:(PKPaymentRequest *)request presenter:(WebKit::PaymentAuthorizationPresenter&)presenter;

@end

@implementation WKPaymentAuthorizationControllerDelegate

- (instancetype)initWithRequest:(PKPaymentRequest *)request presenter:(WebKit::PaymentAuthorizationPresenter&)presenter
{
    if (!(self = [super _initWithRequest:request presenter:presenter]))
        return nil;

    return self;
}

- (void)_getPaymentServicesMerchantURL:(void(^)(NSURL *, NSError *))completion
{
    // FIXME: This -respondsToSelector: check can be removed once rdar://problem/48771320 is in an iOS SDK.
    if ([PAL::getPKPaymentAuthorizationControllerClass() respondsToSelector:@selector(paymentServicesMerchantURLForAPIType:completion:)])
        [PAL::getPKPaymentAuthorizationControllerClass() paymentServicesMerchantURLForAPIType:[_request APIType] completion:completion];
    else
        [PAL::getPKPaymentAuthorizationViewControllerClass() paymentServicesMerchantURLForAPIType:[_request APIType] completion:completion];
}

#pragma mark PKPaymentAuthorizationControllerDelegate

- (void)paymentAuthorizationControllerDidFinish:(PKPaymentAuthorizationController *)controller
{
    [self _didFinish];
}

- (void)paymentAuthorizationController:(PKPaymentAuthorizationController *)controller didAuthorizePayment:(PKPayment *)payment handler:(void(^)(PKPaymentAuthorizationResult *result))completion
{
    [self _didAuthorizePayment:payment completion:completion];
}

- (void)paymentAuthorizationController:(PKPaymentAuthorizationController *)controller didSelectShippingMethod:(PKShippingMethod *)shippingMethod handler:(void(^)(PKPaymentRequestShippingMethodUpdate *requestUpdate))completion
{
    [self _didSelectShippingMethod:shippingMethod completion:completion];
}

- (void)paymentAuthorizationController:(PKPaymentAuthorizationController *)controller didSelectShippingContact:(PKContact *)contact handler:(void(^)(PKPaymentRequestShippingContactUpdate *requestUpdate))completion
{
    [self _didSelectShippingContact:contact completion:completion];
}

- (void)paymentAuthorizationController:(PKPaymentAuthorizationController *)controller didSelectPaymentMethod:(PKPaymentMethod *)paymentMethod handler:(void(^)(PKPaymentRequestPaymentMethodUpdate *requestUpdate))completion
{
    [self _didSelectPaymentMethod:paymentMethod completion:completion];
}

#pragma mark PKPaymentAuthorizationControllerPrivateDelegate

- (void)paymentAuthorizationController:(PKPaymentAuthorizationController *)controller willFinishWithError:(NSError *)error
{
    [self _willFinishWithError:error];
}

- (void)paymentAuthorizationController:(PKPaymentAuthorizationController *)controller didRequestMerchantSession:(void(^)(PKPaymentMerchantSession *, NSError *))sessionBlock
{
    [self _didRequestMerchantSession:sessionBlock];
}

@end

namespace WebKit {

PaymentAuthorizationController::PaymentAuthorizationController(PaymentAuthorizationPresenter::Client& client, PKPaymentRequest *request)
    : PaymentAuthorizationPresenter(client)
    , m_controller(adoptNS([PAL::allocPKPaymentAuthorizationControllerInstance() initWithPaymentRequest:request]))
    , m_delegate(adoptNS([[WKPaymentAuthorizationControllerDelegate alloc] initWithRequest:request presenter:*this]))
{
    [m_controller setDelegate:m_delegate.get()];
    [m_controller setPrivateDelegate:m_delegate.get()];
}

WKPaymentAuthorizationDelegate *PaymentAuthorizationController::platformDelegate()
{
    return m_delegate.get();
}

void PaymentAuthorizationController::dismiss()
{
    [m_controller dismissWithCompletion:nil];
    [m_controller setDelegate:nil];
    [m_controller setPrivateDelegate:nil];
    m_controller = nil;
    [m_delegate invalidate];
    m_delegate = nil;
}

void PaymentAuthorizationController::present(UIViewController *, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_controller)
        return completionHandler(false);

    [m_controller presentWithCompletion:makeBlockPtr([completionHandler = WTFMove(completionHandler)](BOOL success) mutable {
        completionHandler(success);
    }).get()];
}

} // namespace WebKit

#endif // USE(PASSKIT) && PLATFORM(IOS_FAMILY)
