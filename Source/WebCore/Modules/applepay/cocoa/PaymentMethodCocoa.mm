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
#import "PaymentMethod.h"

#if ENABLE(APPLE_PAY)

#import <PassKit/PassKit.h>
#import <runtime/JSONObject.h>

namespace WebCore {

RetainPtr<NSDictionary> toDictionary(PKPaymentMethod *paymentMethod)
{
    auto result = adoptNS([[NSMutableDictionary alloc] init]);

    if (NSString *displayName = paymentMethod.displayName)
        [result setObject:displayName forKey:@"displayName"];

    if (NSString *network = paymentMethod.network)
        [result setObject:network forKey:@"network"];

    return result;
}

JSC::JSValue PaymentMethod::toJS(JSC::ExecState& exec) const
{
    auto dictionary = toDictionary(m_pkPaymentMethod.get());

    // FIXME: Don't round-trip using NSString.
    auto jsonString = adoptNS([[NSString alloc] initWithData:[NSJSONSerialization dataWithJSONObject:dictionary.get() options:0 error:nullptr] encoding:NSUTF8StringEncoding]);
    return JSONParse(&exec, jsonString.get());
}

}
#endif
