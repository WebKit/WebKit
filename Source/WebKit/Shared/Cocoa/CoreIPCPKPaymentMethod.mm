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
#import "CoreIPCPKPaymentMethod.h"

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKPAYMENTMETHOD)

#import "ArgumentCodersCocoa.h"
#import "Logging.h"
#import "WKKeyedCoder.h"
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <pal/cocoa/ContactsSoftLink.h>
#import <pal/cocoa/PassKitSoftLink.h>


namespace WebKit {

CoreIPCPKPaymentMethod::CoreIPCPKPaymentMethod(PKPaymentMethod *paymentMethod)
{
    if (!paymentMethod)
        return;

    RetainPtr archiver = adoptNS([WKKeyedCoder new]);
    [paymentMethod encodeWithCoder:archiver.get()];
    RetainPtr dictionary = [archiver accumulatedDictionary];

    CoreIPCPKPaymentMethodData data;

    if (RetainPtr typeNumber = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"type"])) {
        auto value = [typeNumber unsignedCharValue];
        if (isValidEnum<PKPaymentMethodType>(value))
            data.type = static_cast<PKPaymentMethodType>(value);
    }

    if (RetainPtr displayName = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"displayName"]))
        data.displayName = WTF::move(displayName);

    if (RetainPtr network = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"network"]))
        data.network = WTF::move(network);

    if (id paymentPass = [dictionary.get() objectForKey:@"paymentPass"]) {
        if ([paymentPass isKindOfClass:PAL::getPKSecureElementPassClassSingleton()])
            data.paymentPass = paymentPass;
    }

    if (RetainPtr peerPaymentQuoteIdentifier = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"peerPaymentQuoteIdentifier"]))
        data.peerPaymentQuoteIdentifier = WTF::move(peerPaymentQuoteIdentifier);

    if (id billingAddress = [dictionary.get() objectForKey:@"billingAddress"]) {
        if ([billingAddress isKindOfClass:PAL::getCNContactClassSingleton()])
            data.billingAddress = billingAddress;
    }

    if (RetainPtr installmentBindToken = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"installmentBindToken"]))
        data.installmentBindToken = WTF::move(installmentBindToken);

    if (RetainPtr usePeerPaymentBalanceValue = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"usePeerPaymentBalance"]))
        data.usePeerPaymentBalance = [usePeerPaymentBalanceValue boolValue];

    m_data = WTF::move(data);
}

CoreIPCPKPaymentMethod::CoreIPCPKPaymentMethod(std::optional<CoreIPCPKPaymentMethodData>&& data)
    : m_data { WTF::move(data) }
{
}

RetainPtr<id> CoreIPCPKPaymentMethod::toID() const
{
    if (!m_data)
        return { };

    RetainPtr dictionary = [NSMutableDictionary dictionaryWithCapacity:8];

    if (m_data->type)
        [dictionary setObject:[NSNumber numberWithUnsignedChar:static_cast<uint8_t>(*m_data->type)] forKey:@"type"];
    if (m_data->displayName)
        [dictionary setObject:m_data->displayName.get() forKey:@"displayName"];
    if (m_data->network)
        [dictionary setObject:m_data->network.get() forKey:@"network"];
    if (m_data->paymentPass)
        [dictionary setObject:m_data->paymentPass.get() forKey:@"paymentPass"];
    if (m_data->peerPaymentQuoteIdentifier)
        [dictionary setObject:m_data->peerPaymentQuoteIdentifier.get() forKey:@"peerPaymentQuoteIdentifier"];
    if (m_data->billingAddress)
        [dictionary setObject:m_data->billingAddress.get() forKey:@"billingAddress"];
    if (m_data->installmentBindToken)
        [dictionary setObject:m_data->installmentBindToken.get() forKey:@"installmentBindToken"];
    if (m_data->usePeerPaymentBalance)
        [dictionary setObject:[NSNumber numberWithBool:*m_data->usePeerPaymentBalance] forKey:@"usePeerPaymentBalance"];

    RetainPtr unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:dictionary.get()]);
    RetainPtr paymentMethod = adoptNS([[PAL::getPKPaymentMethodClassSingleton() alloc] initWithCoder:unarchiver.get()]);
    if (!paymentMethod)
        RELEASE_LOG_ERROR(IPC, "CoreIPCPKPaymentMethod was not able to reconstruct a PKPaymentMethod object");
    return paymentMethod;
}

} // namespace WebKit

#endif
