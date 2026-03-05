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
#import "CoreIPCPKShippingMethod.h"

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKSHIPPINGMETHOD)

#import "ArgumentCodersCocoa.h"
#import "Logging.h"
#import "WKKeyedCoder.h"
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <pal/cocoa/PassKitSoftLink.h>

namespace WebKit {

CoreIPCPKShippingMethod::CoreIPCPKShippingMethod(PKShippingMethod *shippingMethod)
{
    if (!shippingMethod)
        return;

    RetainPtr archiver = adoptNS([WKKeyedCoder new]);
    [shippingMethod encodeWithCoder:archiver.get()];
    RetainPtr dictionary = [archiver accumulatedDictionary];

    CoreIPCPKShippingMethodData data;

    if (RetainPtr label = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"label"]))
        data.label = WTF::move(label);

    if (RetainPtr amount = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"amount"]))
        data.amount = WTF::move(amount);

    if (RetainPtr typeNumber = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"type"])) {
        auto value = [typeNumber unsignedCharValue];
        if (isValidEnum<PKPaymentSummaryItemType>(value))
            data.type = static_cast<PKPaymentSummaryItemType>(value);
    }

    if (RetainPtr localizedTitle = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"localizedTitle"]))
        data.localizedTitle = WTF::move(localizedTitle);

    if (RetainPtr localizedAmount = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"localizedAmount"]))
        data.localizedAmount = WTF::move(localizedAmount);

    if (RetainPtr useDarkColor = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"useDarkColor"]))
        data.useDarkColor = [useDarkColor boolValue];

    if (RetainPtr useLargeFont = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"useLargeFont"]))
        data.useLargeFont = [useLargeFont boolValue];

    if (RetainPtr identifier = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"identifier"]))
        data.identifier = WTF::move(identifier);

    if (RetainPtr detail = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"detail"]))
        data.detail = WTF::move(detail);

    if (id dateComponentsRange = [dictionary.get() objectForKey:@"dateComponentsRange"]) {
        if ([dateComponentsRange isKindOfClass:PAL::getPKDateComponentsRangeClassSingleton()])
            data.dateComponentsRange = dateComponentsRange;
    }

    m_data = WTF::move(data);
}

CoreIPCPKShippingMethod::CoreIPCPKShippingMethod(std::optional<CoreIPCPKShippingMethodData>&& data)
    : m_data { WTF::move(data) }
{
}

RetainPtr<id> CoreIPCPKShippingMethod::toID() const
{
    if (!m_data)
        return { };

    RetainPtr dictionary = [NSMutableDictionary dictionaryWithCapacity:10];

    if (m_data->label)
        [dictionary setObject:m_data->label.get() forKey:@"label"];
    if (m_data->amount)
        [dictionary setObject:m_data->amount.get() forKey:@"amount"];
    if (m_data->type)
        [dictionary setObject:[NSNumber numberWithUnsignedChar:static_cast<uint8_t>(*m_data->type)] forKey:@"type"];
    if (m_data->localizedTitle)
        [dictionary setObject:m_data->localizedTitle.get() forKey:@"localizedTitle"];
    if (m_data->localizedAmount)
        [dictionary setObject:m_data->localizedAmount.get() forKey:@"localizedAmount"];
    if (m_data->useDarkColor)
        [dictionary setObject:@(*m_data->useDarkColor) forKey:@"useDarkColor"];
    if (m_data->useLargeFont)
        [dictionary setObject:@(*m_data->useLargeFont) forKey:@"useLargeFont"];
    if (m_data->identifier)
        [dictionary setObject:m_data->identifier.get() forKey:@"identifier"];
    if (m_data->detail)
        [dictionary setObject:m_data->detail.get() forKey:@"detail"];
    if (m_data->dateComponentsRange)
        [dictionary setObject:m_data->dateComponentsRange.get() forKey:@"dateComponentsRange"];

    RetainPtr unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:dictionary.get()]);
    RetainPtr shippingMethod = adoptNS([[PAL::getPKShippingMethodClassSingleton() alloc] initWithCoder:unarchiver.get()]);
    if (!shippingMethod)
        RELEASE_LOG_ERROR(IPC, "CoreIPCPKShippingMethod was not able to reconstruct a PKShippingMethod object");
    return shippingMethod;
}

} // namespace WebKit

#endif
