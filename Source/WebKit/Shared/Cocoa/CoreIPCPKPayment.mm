/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "CoreIPCPKPayment.h"

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKPAYMENT)

#import "Logging.h"
#import "WKKeyedCoder.h"
#import <wtf/cocoa/TypeCastsCocoa.h>

#import <pal/cocoa/PassKitSoftLink.h>

namespace WebKit {

CoreIPCPKPayment::CoreIPCPKPayment(PKPayment *payment)
{
    if (!payment)
        return;

    RetainPtr archiver = adoptNS([WKKeyedCoder new]);
    [payment encodeWithCoder:archiver.get()];
    RetainPtr dictionary = [archiver accumulatedDictionary];

    CoreIPCPKPaymentData data;

    if (id token = [dictionary.get() objectForKey:@"token"]) {
        if ([token isKindOfClass:PAL::getPKPaymentTokenClassSingleton()])
            data.token = token;
    }

    if (id shippingContact = [dictionary.get() objectForKey:@"shippingContact"]) {
        if ([shippingContact isKindOfClass:PAL::getPKContactClassSingleton()])
            data.shippingContact = shippingContact;
    }

    if (id billingContact = [dictionary.get() objectForKey:@"billingContact"]) {
        if ([billingContact isKindOfClass:PAL::getPKContactClassSingleton()])
            data.billingContact = billingContact;
    }

    if (id shippingMethod = [dictionary.get() objectForKey:@"shippingMethod"]) {
        if ([shippingMethod isKindOfClass:PAL::getPKShippingMethodClassSingleton()])
            data.shippingMethod = shippingMethod;
    }

    if (RetainPtr credential = dynamic_objc_cast<NSData>([dictionary.get() objectForKey:@"credential"]))
        data.credential = WTF::move(credential);

    if (RetainPtr attempts = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"biometryAttempts"]))
        data.biometryAttempts = WTF::move(attempts);

    if (RetainPtr installmentToken = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"installmentAuthorizationToken"]))
        data.installmentAuthorizationToken = WTF::move(installmentToken);

    m_data = WTF::move(data);
}

CoreIPCPKPayment::CoreIPCPKPayment(std::optional<CoreIPCPKPaymentData>&& data)
    : m_data(WTF::move(data))
{
}

RetainPtr<id> CoreIPCPKPayment::toID() const
{
    if (!m_data)
        return { };

    RetainPtr dictionary = [NSMutableDictionary dictionaryWithCapacity:7];

    if (m_data->token)
        [dictionary setObject:m_data->token.get() forKey:@"token"];
    if (m_data->shippingContact)
        [dictionary setObject:m_data->shippingContact.get() forKey:@"shippingContact"];
    if (m_data->billingContact)
        [dictionary setObject:m_data->billingContact.get() forKey:@"billingContact"];
    if (m_data->shippingMethod)
        [dictionary setObject:m_data->shippingMethod.get() forKey:@"shippingMethod"];
    if (m_data->credential)
        [dictionary setObject:m_data->credential.get() forKey:@"credential"];
    if (m_data->biometryAttempts)
        [dictionary setObject:m_data->biometryAttempts.get() forKey:@"biometryAttempts"];
    if (m_data->installmentAuthorizationToken)
        [dictionary setObject:m_data->installmentAuthorizationToken.get() forKey:@"installmentAuthorizationToken"];

    RetainPtr unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:dictionary.get()]);
    RetainPtr payment = adoptNS([[PAL::getPKPaymentClassSingleton() alloc] initWithCoder:unarchiver.get()]);
    if (!payment)
        RELEASE_LOG_ERROR(IPC, "CoreIPCPKPayment was not able to reconstruct a PKPayment object");
    return payment;
}

} // namespace WebKit

#endif // USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKPAYMENT)
