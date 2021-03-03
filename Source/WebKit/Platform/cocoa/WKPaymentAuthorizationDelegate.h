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

#if USE(PASSKIT)

#import <pal/spi/cocoa/PassKitSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSError;
OBJC_CLASS UIViewController;

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WKPaymentAuthorizationDelegateAdditions.h>
#endif

namespace WebKit {

class PaymentAuthorizationPresenter;

using DidRequestMerchantSessionCompletion = BlockPtr<void(PKPaymentMerchantSession *, NSError *)>;

using DidAuthorizePaymentCompletion = BlockPtr<void(PKPaymentAuthorizationResult *)>;
using DidSelectPaymentMethodCompletion = BlockPtr<void(PKPaymentRequestPaymentMethodUpdate *)>;
using DidSelectShippingContactCompletion = BlockPtr<void(PKPaymentRequestShippingContactUpdate *)>;
using DidSelectShippingMethodCompletion = BlockPtr<void(PKPaymentRequestShippingMethodUpdate *)>;

}

@interface WKPaymentAuthorizationDelegate : NSObject {
    RetainPtr<PKPaymentRequest> _request;
}

- (instancetype)init NS_UNAVAILABLE;

- (void)completeMerchantValidation:(PKPaymentMerchantSession *)session error:(NSError *)error;
- (void)completePaymentMethodSelection:(PKPaymentRequestPaymentMethodUpdate *)paymentMethodUpdate;
- (void)completePaymentSession:(PKPaymentAuthorizationStatus)status errors:(NSArray<NSError *> *)errors;
- (void)completeShippingContactSelection:(PKPaymentRequestShippingContactUpdate *)shippingContactUpdate;
- (void)completeShippingMethodSelection:(PKPaymentRequestShippingMethodUpdate *)shippingMethodUpdate;
#if defined(WKPaymentAuthorizationDelegateAdditions_interface_public)
WKPaymentAuthorizationDelegateAdditions_interface_public
#endif
- (void)invalidate;

@end

// Helper methods called by WKPaymentAuthorizationSession's subclasses
@interface WKPaymentAuthorizationDelegate (Protected)

- (instancetype)_initWithRequest:(PKPaymentRequest *)request presenter:(WebKit::PaymentAuthorizationPresenter&)presenter;

- (void)_didAuthorizePayment:(PKPayment *)payment completion:(WebKit::DidAuthorizePaymentCompletion::BlockType)completion;
- (void)_didFinish;
- (void)_didRequestMerchantSession:(WebKit::DidRequestMerchantSessionCompletion::BlockType)completion;
- (void)_didSelectPaymentMethod:(PKPaymentMethod *)paymentMethod completion:(WebKit::DidSelectPaymentMethodCompletion::BlockType)completion;
- (void)_didSelectShippingContact:(PKContact *)contact completion:(WebKit::DidSelectShippingContactCompletion::BlockType)completion;
- (void)_didSelectShippingMethod:(PKShippingMethod *)shippingMethod completion:(WebKit::DidSelectShippingMethodCompletion::BlockType)completion;
#if defined(WKPaymentAuthorizationDelegateAdditions_interface_protected)
WKPaymentAuthorizationDelegateAdditions_interface_protected
#endif
- (void)_getPaymentServicesMerchantURL:(void(^)(NSURL *, NSError *))completion;
- (void)_willFinishWithError:(NSError *)error;

@end

#endif // USE(PASSKIT)
