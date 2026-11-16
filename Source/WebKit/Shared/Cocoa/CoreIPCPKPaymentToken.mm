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
#import "CoreIPCPKPaymentToken.h"

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKPAYMENTTOKEN)

#import "ArgumentCodersCocoa.h"
#import "Logging.h"
#import "WKKeyedCoder.h"
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <pal/cocoa/PassKitSoftLink.h>


namespace WebKit {

CoreIPCPKPaymentToken::CoreIPCPKPaymentToken(PKPaymentToken *token)
{
    if (!token)
        return;

    RetainPtr archiver = adoptNS([WKKeyedCoder new]);
    [token encodeWithCoder:archiver.get()];
    RetainPtr dictionary = [archiver accumulatedDictionary];

    CoreIPCPKPaymentTokenData data;

    if (RetainPtr paymentInstrumentName = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"paymentInstrumentName"]))
        data.paymentInstrumentName = WTF::move(paymentInstrumentName);

    if (RetainPtr paymentNetwork = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"paymentNetwork"]))
        data.paymentNetwork = WTF::move(paymentNetwork);

    if (RetainPtr transactionIdentifier = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"transactionIdentifier"]))
        data.transactionIdentifier = WTF::move(transactionIdentifier);

    if (RetainPtr paymentData = dynamic_objc_cast<NSData>([dictionary.get() objectForKey:@"paymentData"]))
        data.paymentData = WTF::move(paymentData);

    if (id paymentMethod = [dictionary.get() objectForKey:@"paymentMethod"]) {
        if ([paymentMethod isKindOfClass:PAL::getPKPaymentMethodClassSingleton()])
            data.paymentMethod = paymentMethod;
    }

    if (RetainPtr redeemURL = dynamic_objc_cast<NSURL>([dictionary.get() objectForKey:@"redeemURL"]))
        data.redeemURL = WTF::move(redeemURL);

    if (RetainPtr retryNonce = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"retryNonce"]))
        data.retryNonce = WTF::move(retryNonce);

    m_data = WTF::move(data);
}

CoreIPCPKPaymentToken::CoreIPCPKPaymentToken(std::optional<CoreIPCPKPaymentTokenData>&& data)
    : m_data { WTF::move(data) }
{
}

RetainPtr<id> CoreIPCPKPaymentToken::toID() const
{
    if (!m_data)
        return { };

    RetainPtr dictionary = [NSMutableDictionary dictionaryWithCapacity:7];

    if (m_data->paymentInstrumentName)
        [dictionary setObject:m_data->paymentInstrumentName.get() forKey:@"paymentInstrumentName"];
    if (m_data->paymentNetwork)
        [dictionary setObject:m_data->paymentNetwork.get() forKey:@"paymentNetwork"];
    if (m_data->transactionIdentifier)
        [dictionary setObject:m_data->transactionIdentifier.get() forKey:@"transactionIdentifier"];
    if (m_data->paymentData)
        [dictionary setObject:m_data->paymentData.get() forKey:@"paymentData"];
    if (m_data->paymentMethod)
        [dictionary setObject:m_data->paymentMethod.get() forKey:@"paymentMethod"];
    if (m_data->redeemURL)
        [dictionary setObject:m_data->redeemURL.get() forKey:@"redeemURL"];
    if (m_data->retryNonce)
        [dictionary setObject:m_data->retryNonce.get() forKey:@"retryNonce"];

    RetainPtr unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:dictionary.get()]);
    RetainPtr token = adoptNS([[PAL::getPKPaymentTokenClassSingleton() alloc] initWithCoder:unarchiver.get()]);
    if (!token)
        RELEASE_LOG_ERROR(IPC, "CoreIPCPKPaymentToken was not able to reconstruct a PKPaymentToken object");
    return token;
}

} // namespace WebKit

#endif
