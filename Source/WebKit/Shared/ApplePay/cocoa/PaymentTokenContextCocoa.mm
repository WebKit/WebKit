/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "PaymentTokenContext.h"

#if HAVE(PASSKIT_MULTI_MERCHANT_PAYMENTS)

#import "WebPaymentCoordinatorProxyCocoa.h"
#import <WebCore/ApplePayPaymentTokenContext.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

#import <pal/cocoa/PassKitSoftLink.h>

namespace WebKit {
using namespace WebCore;

RetainPtr<PKPaymentTokenContext> platformPaymentTokenContext(const ApplePayPaymentTokenContext& webTokenContext)
{
    RetainPtr<NSString> merchantDomain;
    if (!webTokenContext.merchantDomain.isNull())
        merchantDomain = webTokenContext.merchantDomain;
    return adoptNS([PAL::allocPKPaymentTokenContextInstance() initWithMerchantIdentifier:webTokenContext.merchantIdentifier externalIdentifier:webTokenContext.externalIdentifier merchantName:webTokenContext.merchantName merchantDomain:merchantDomain.get() amount:toDecimalNumber(webTokenContext.amount)]);
}

RetainPtr<NSArray<PKPaymentTokenContext *>> platformPaymentTokenContexts(const Vector<ApplePayPaymentTokenContext>& webTokenContexts)
{
    return createNSArray(webTokenContexts, platformPaymentTokenContext);
}

} // namespace WebKit

#endif // HAVE(PASSKIT_MULTI_MERCHANT_PAYMENTS)
