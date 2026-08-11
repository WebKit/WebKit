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
#import "CoreIPCPKPaymentSetupFeature.h"

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKPAYMENTSETUPFEATURE)

#import "ArgumentCodersCocoa.h"
#import "Logging.h"
#import "WKKeyedCoder.h"
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <pal/cocoa/PassKitSoftLink.h>


namespace WebKit {

CoreIPCPKPaymentSetupFeature::CoreIPCPKPaymentSetupFeature(PKPaymentSetupFeature *feature)
{
    if (!feature)
        return;

    RetainPtr archiver = adoptNS([WKKeyedCoder new]);
    [feature encodeWithCoder:archiver.get()];
    RetainPtr dictionary = [archiver accumulatedDictionary];

    CoreIPCPKPaymentSetupFeatureData data;

    if (RetainPtr identifiersSet = dynamic_objc_cast<NSSet>([dictionary.get() objectForKey:@"identifiers"])) {
        Vector<RetainPtr<NSString>> identifiersVector;
        identifiersVector.reserveInitialCapacity([identifiersSet.get() count]);
        for (NSString *identifier in identifiersSet.get()) {
            if ([identifier isKindOfClass:NSString.class])
                identifiersVector.append(identifier);
        }
        data.identifiers = WTF::move(identifiersVector);
    }

    if (RetainPtr localizedDisplayName = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"localizedDisplayName"]))
        data.localizedDisplayName = WTF::move(localizedDisplayName);

    if (RetainPtr typeNumber = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"type"])) {
        auto value = [typeNumber unsignedCharValue];
        if (isValidEnum<PKPaymentSetupFeatureType>(value))
            data.type = static_cast<PKPaymentSetupFeatureType>(value);
    }

    if (RetainPtr stateNumber = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"state"])) {
        auto value = [stateNumber unsignedCharValue];
        if (isValidEnum<PKPaymentSetupFeatureState>(value))
            data.state = static_cast<PKPaymentSetupFeatureState>(value);
    }

    if (RetainPtr optionsNumber = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"supportedOptions"])) {
        auto rawValue = [optionsNumber unsignedCharValue];
        auto optionSet = OptionSet<PKPaymentSetupFeatureSupportedOptions>::fromRaw(rawValue);
        if (isValidOptionSet(optionSet))
            data.supportedOptions = optionSet;
    }

    if (RetainPtr devicesNumber = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"supportedDevices"])) {
        auto rawValue = [devicesNumber unsignedCharValue];
        auto optionSet = OptionSet<PKPaymentSetupFeatureSupportedDevices>::fromRaw(rawValue);
        if (isValidOptionSet(optionSet))
            data.supportedDevices = optionSet;
    }

    if (RetainPtr productIdentifier = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"productIdentifier"]))
        data.productIdentifier = WTF::move(productIdentifier);

    if (RetainPtr partnerIdentifier = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"partnerIdentifier"]))
        data.partnerIdentifier = WTF::move(partnerIdentifier);

    if (RetainPtr featureIdentifier = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"featureIdentifier"]))
        data.featureIdentifier = WTF::move(featureIdentifier);

    if (RetainPtr lastUpdated = dynamic_objc_cast<NSDate>([dictionary.get() objectForKey:@"lastUpdated"]))
        data.lastUpdated = WTF::move(lastUpdated);

    if (RetainPtr expiry = dynamic_objc_cast<NSDate>([dictionary.get() objectForKey:@"expiry"]))
        data.expiry = WTF::move(expiry);

    if (RetainPtr productType = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"productType"]))
        data.productType = WTF::move(productType);

    if (RetainPtr productState = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"productState"]))
        data.productState = WTF::move(productState);

    if (RetainPtr notificationTitle = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"notificationTitle"]))
        data.notificationTitle = WTF::move(notificationTitle);

    if (RetainPtr notificationMessage = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"notificationMessage"]))
        data.notificationMessage = WTF::move(notificationMessage);

    if (RetainPtr discoveryCardIdentifier = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"discoveryCardIdentifier"]))
        data.discoveryCardIdentifier = WTF::move(discoveryCardIdentifier);

    m_data = WTF::move(data);
}

CoreIPCPKPaymentSetupFeature::CoreIPCPKPaymentSetupFeature(std::optional<CoreIPCPKPaymentSetupFeatureData>&& data)
    : m_data { WTF::move(data) }
{
}

RetainPtr<id> CoreIPCPKPaymentSetupFeature::toID() const
{
    if (!m_data)
        return { };

    RetainPtr dictionary = [NSMutableDictionary dictionaryWithCapacity:16];

    if (m_data->identifiers) {
        RetainPtr identifiersSet = adoptNS([[NSMutableSet alloc] initWithCapacity:m_data->identifiers->size()]);
        for (auto& identifier : *m_data->identifiers)
            [identifiersSet addObject:identifier.get()];
        [dictionary setObject:identifiersSet.get() forKey:@"identifiers"];
    }

    if (m_data->localizedDisplayName)
        [dictionary setObject:m_data->localizedDisplayName.get() forKey:@"localizedDisplayName"];
    if (m_data->type)
        [dictionary setObject:[NSNumber numberWithUnsignedChar:static_cast<uint8_t>(*m_data->type)] forKey:@"type"];
    if (m_data->state)
        [dictionary setObject:[NSNumber numberWithUnsignedChar:static_cast<uint8_t>(*m_data->state)] forKey:@"state"];
    if (m_data->supportedOptions)
        [dictionary setObject:[NSNumber numberWithUnsignedChar:m_data->supportedOptions->toRaw()] forKey:@"supportedOptions"];
    if (m_data->supportedDevices)
        [dictionary setObject:[NSNumber numberWithUnsignedChar:m_data->supportedDevices->toRaw()] forKey:@"supportedDevices"];
    if (m_data->productIdentifier)
        [dictionary setObject:m_data->productIdentifier.get() forKey:@"productIdentifier"];
    if (m_data->partnerIdentifier)
        [dictionary setObject:m_data->partnerIdentifier.get() forKey:@"partnerIdentifier"];
    if (m_data->featureIdentifier)
        [dictionary setObject:m_data->featureIdentifier.get() forKey:@"featureIdentifier"];
    if (m_data->lastUpdated)
        [dictionary setObject:m_data->lastUpdated.get() forKey:@"lastUpdated"];
    if (m_data->expiry)
        [dictionary setObject:m_data->expiry.get() forKey:@"expiry"];
    if (m_data->productType)
        [dictionary setObject:m_data->productType.get() forKey:@"productType"];
    if (m_data->productState)
        [dictionary setObject:m_data->productState.get() forKey:@"productState"];
    if (m_data->notificationTitle)
        [dictionary setObject:m_data->notificationTitle.get() forKey:@"notificationTitle"];
    if (m_data->notificationMessage)
        [dictionary setObject:m_data->notificationMessage.get() forKey:@"notificationMessage"];
    if (m_data->discoveryCardIdentifier)
        [dictionary setObject:m_data->discoveryCardIdentifier.get() forKey:@"discoveryCardIdentifier"];

    RetainPtr unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:dictionary.get()]);
    RetainPtr feature = adoptNS([[PAL::getPKPaymentSetupFeatureClassSingleton() alloc] initWithCoder:unarchiver.get()]);
    if (!feature)
        RELEASE_LOG_ERROR(IPC, "CoreIPCPKPaymentSetupFeature was not able to reconstruct a PKPaymentSetupFeature object");
    return feature;
}

} // namespace WebKit

#endif
