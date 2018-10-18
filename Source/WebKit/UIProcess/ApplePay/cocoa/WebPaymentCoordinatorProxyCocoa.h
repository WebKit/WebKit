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

#pragma once

#if ENABLE(APPLE_PAY)

#import <WebCore/PaymentHeaders.h>
#import <pal/spi/cocoa/PassKitSPI.h>
#import <wtf/BlockPtr.h>

namespace WebCore {
class URL;
}

namespace WebKit {
class WebPageProxy;
class WebPaymentCoordinatorProxy;

RetainPtr<PKPaymentRequest> toPKPaymentRequest(WebPageProxy&, const WebCore::URL& originatingURL, const Vector<WebCore::URL>& linkIconURLs, const WebCore::ApplePaySessionPaymentRequest&);

}

@interface WKPaymentAuthorizationViewControllerDelegate : NSObject <PKPaymentAuthorizationViewControllerDelegate, PKPaymentAuthorizationViewControllerPrivateDelegate> {
@package
    WebKit::WebPaymentCoordinatorProxy* _webPaymentCoordinatorProxy;
    RetainPtr<NSArray> _paymentSummaryItems;
    RetainPtr<NSArray> _shippingMethods;

    BlockPtr<void (PKPaymentMerchantSession *, NSError *)> _sessionBlock;

    BOOL _didReachFinalState;
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
    BlockPtr<void (PKPaymentAuthorizationResult *)> _paymentAuthorizedCompletion;
    BlockPtr<void (PKPaymentRequestPaymentMethodUpdate *)> _didSelectPaymentMethodCompletion;
    BlockPtr<void (PKPaymentRequestShippingMethodUpdate *)> _didSelectShippingMethodCompletion;
    BlockPtr<void (PKPaymentRequestShippingContactUpdate *)> _didSelectShippingContactCompletion;
#else
    BlockPtr<void (PKPaymentAuthorizationStatus)> _paymentAuthorizedCompletion;
    BlockPtr<void (NSArray *)> _didSelectPaymentMethodCompletion;
    BlockPtr<void (PKPaymentAuthorizationStatus, NSArray *)> _didSelectShippingMethodCompletion;
    BlockPtr<void (PKPaymentAuthorizationStatus, NSArray *, NSArray *)> _didSelectShippingContactCompletion;
#endif
}

- (instancetype)initWithPaymentCoordinatorProxy:(WebKit::WebPaymentCoordinatorProxy&)webPaymentCoordinatorProxy;

- (void)invalidate;

@end

#endif
