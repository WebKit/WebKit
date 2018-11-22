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
#import "PaymentMerchantSession.h"

#if ENABLE(APPLE_PAY)

#import <JavaScriptCore/JSONObject.h>
#import <pal/cocoa/PassKitSoftLink.h>

namespace WebCore {

std::optional<PaymentMerchantSession> PaymentMerchantSession::fromJS(JSC::ExecState& state, JSC::JSValue value, String&)
{
    // FIXME: Don't round-trip using NSString.
    auto jsonString = JSONStringify(&state, value, 0);
    if (!jsonString)
        return std::nullopt;

    auto dictionary = dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:[(NSString *)jsonString dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil]);
    if (!dictionary || ![dictionary isKindOfClass:[NSDictionary class]])
        return std::nullopt;

    auto pkPaymentMerchantSession = adoptNS([PAL::allocPKPaymentMerchantSessionInstance() initWithDictionary:dictionary]);

    return PaymentMerchantSession(pkPaymentMerchantSession.get());
}

}

#endif
