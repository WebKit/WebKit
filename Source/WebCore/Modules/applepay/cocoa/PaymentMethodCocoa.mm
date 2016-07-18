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

#import "PassKitSPI.h"
#import <runtime/JSONObject.h>

namespace WebCore {

static NSString *toString(PKPaymentPassActivationState paymentPassActivationState)
{
    switch (paymentPassActivationState) {
    case PKPaymentPassActivationStateActivated:
        return @"activated";
    case PKPaymentPassActivationStateRequiresActivation:
        return @"requiresActivation";
    case PKPaymentPassActivationStateActivating:
        return @"activating";
    case PKPaymentPassActivationStateSuspended:
        return @"suspended";
    case PKPaymentPassActivationStateDeactivated:
        return @"deactivated";
    default:
        return nil;
    }
}

static RetainPtr<NSDictionary> toDictionary(PKPaymentPass *paymentPass)
{
    auto result = adoptNS([[NSMutableDictionary alloc] init]);

    [result setObject:paymentPass.primaryAccountIdentifier forKey:@"primaryAccountIdentifier"];
    [result setObject:paymentPass.primaryAccountNumberSuffix forKey:@"primaryAccountNumberSuffix"];
    if (NSString *deviceAccountIdentifier = paymentPass.deviceAccountIdentifier)
        [result setObject:deviceAccountIdentifier forKey:@"deviceAccountIdentifier"];
    if (NSString *deviceAccountNumberSuffix = paymentPass.deviceAccountNumberSuffix)
        [result setObject:deviceAccountNumberSuffix forKey:@"deviceAccountNumberSuffix"];
    if (NSString *activationState = toString(paymentPass.activationState))
        [result setObject:activationState forKey:@"activationState"];

    return result;
}

static NSString *toString(PKPaymentMethodType paymentMethodType)
{
    switch (paymentMethodType) {
    case PKPaymentMethodTypeDebit:
        return @"debit";
    case PKPaymentMethodTypeCredit:
        return @"credit";
    case PKPaymentMethodTypePrepaid:
        return @"prepaid";
    case PKPaymentMethodTypeStore:
        return @"store";
    default:
        return nil;
    }
}

RetainPtr<NSDictionary> toDictionary(PKPaymentMethod *paymentMethod)
{
    auto result = adoptNS([[NSMutableDictionary alloc] init]);

    if (NSString *displayName = paymentMethod.displayName)
        [result setObject:displayName forKey:@"displayName"];

    if (NSString *network = paymentMethod.network)
        [result setObject:network forKey:@"network"];

    if (NSString *paymentMethodType = toString(paymentMethod.type))
        [result setObject:paymentMethodType forKey:@"type"];

    if (PKPaymentPass *paymentPass = paymentMethod.paymentPass)
        [result setObject:toDictionary(paymentPass).get() forKey:@"paymentPass"];

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
