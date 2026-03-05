/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "PaymentInstallmentConfigurationWebCore.h"

#if HAVE(PASSKIT_INSTALLMENTS)

#import "ApplePayInstallmentConfigurationWebCore.h"
#import "ApplePayInstallmentItemType.h"
#import "ApplePayInstallmentRetailChannel.h"
#import "ExceptionOr.h"
#import "PaymentSummaryItems.h"
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cocoa/PassKitSoftLink.h>

namespace WebCore {

static String fromDecimalNumber(NSDecimalNumber *number)
{
    auto numberFormatter = adoptNS([[NSNumberFormatter alloc] init]);
    [numberFormatter setNumberStyle:NSNumberFormatterNoStyle];
    [numberFormatter setMinimumIntegerDigits:1];
    [numberFormatter setMinimumFractionDigits:2];
    [numberFormatter setMaximumFractionDigits:[numberFormatter maximumIntegerDigits]];
    return [numberFormatter stringFromNumber:number];
}

static std::optional<ApplePaySetupFeatureType> NODELETE applePaySetupFeatureType(PKPaymentSetupFeatureType featureType)
{
    switch (featureType) {
    case PKPaymentSetupFeatureTypeApplePay:
        return ApplePaySetupFeatureType::ApplePay;

    case PKPaymentSetupFeatureTypeAppleCard:
        return ApplePaySetupFeatureType::AppleCard;

    default:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

static PKPaymentSetupFeatureType NODELETE platformFeatureType(ApplePaySetupFeatureType featureType)
{
    switch (featureType) {
    case ApplePaySetupFeatureType::ApplePay:
        return PKPaymentSetupFeatureTypeApplePay;
    case ApplePaySetupFeatureType::AppleCard:
        return PKPaymentSetupFeatureTypeAppleCard;
    }
}

static ApplePayInstallmentItemType NODELETE applePayItemType(PKInstallmentItemType itemType)
{
    switch (itemType) {
    case PKInstallmentItemTypeGeneric:
        return ApplePayInstallmentItemType::Generic;
    case PKInstallmentItemTypePhone:
        return ApplePayInstallmentItemType::Phone;
    case PKInstallmentItemTypePad:
        return ApplePayInstallmentItemType::Pad;
    case PKInstallmentItemTypeWatch:
        return ApplePayInstallmentItemType::Watch;
    case PKInstallmentItemTypeMac:
        return ApplePayInstallmentItemType::Mac;
    }
}

static PKInstallmentItemType NODELETE platformItemType(ApplePayInstallmentItemType itemType)
{
    switch (itemType) {
    case ApplePayInstallmentItemType::Generic:
        return PKInstallmentItemTypeGeneric;
    case ApplePayInstallmentItemType::Phone:
        return PKInstallmentItemTypePhone;
    case ApplePayInstallmentItemType::Pad:
        return PKInstallmentItemTypePad;
    case ApplePayInstallmentItemType::Watch:
        return PKInstallmentItemTypeWatch;
    case ApplePayInstallmentItemType::Mac:
        return PKInstallmentItemTypeMac;
    }
}

static ApplePayInstallmentRetailChannel NODELETE applePayRetailChannel(PKInstallmentRetailChannel retailChannel)
{
    switch (retailChannel) {
    case PKInstallmentRetailChannelUnknown:
        return ApplePayInstallmentRetailChannel::Unknown;
    case PKInstallmentRetailChannelApp:
        return ApplePayInstallmentRetailChannel::App;
    case PKInstallmentRetailChannelWeb:
        return ApplePayInstallmentRetailChannel::Web;
    case PKInstallmentRetailChannelInStore:
        return ApplePayInstallmentRetailChannel::InStore;
    }
}

static PKInstallmentRetailChannel NODELETE platformRetailChannel(ApplePayInstallmentRetailChannel retailChannel)
{
    switch (retailChannel) {
    case ApplePayInstallmentRetailChannel::Unknown:
        return PKInstallmentRetailChannelUnknown;
    case ApplePayInstallmentRetailChannel::App:
        return PKInstallmentRetailChannelApp;
    case ApplePayInstallmentRetailChannel::Web:
        return PKInstallmentRetailChannelWeb;
    case ApplePayInstallmentRetailChannel::InStore:
        return PKInstallmentRetailChannelInStore;
    }
}

static RetainPtr<id> makeNSArrayElement(const ApplePayInstallmentItem& item)
{
    ASSERT(PAL::getPKPaymentInstallmentItemClassSingleton());
    // FIXME: This is a safer cpp false positive.
    SUPPRESS_UNRETAINED_ARG auto installmentItem = adoptNS([PAL::allocPKPaymentInstallmentItemInstance() init]);
    [installmentItem setInstallmentItemType:platformItemType(item.type)];
    [installmentItem setAmount:protect(toDecimalNumber(item.amount)).get()];
    [installmentItem setCurrencyCode:item.currencyCode.createNSString().get()];
    [installmentItem setProgramIdentifier:item.programIdentifier.createNSString().get()];
    [installmentItem setApr:protect(toDecimalNumber(item.apr)).get()];
    [installmentItem setProgramTerms:item.programTerms.createNSString().get()];
    return installmentItem;
}

static std::optional<ApplePayInstallmentItem> makeVectorElement(const ApplePayInstallmentItem*, id arrayElement)
{
    // FIXME: This is a static analysis false positive (rdar://160259918).
    SUPPRESS_UNRETAINED_ARG if (![arrayElement isKindOfClass:PAL::getPKPaymentInstallmentItemClassSingleton()])
        return std::nullopt;

    PKPaymentInstallmentItem *item = arrayElement;
    return ApplePayInstallmentItem {
        applePayItemType([item installmentItemType]),
        fromDecimalNumber(retainPtr([item amount]).get()),
        [item currencyCode],
        [item programIdentifier],
        fromDecimalNumber(retainPtr([item apr]).get()),
        [item programTerms],
    };
}

static RetainPtr<NSDictionary> applicationMetadataDictionary(const ApplePayInstallmentConfiguration& configuration)
{
    if (RetainPtr applicationMetadata = [configuration.applicationMetadata.createNSString() dataUsingEncoding:NSUTF8StringEncoding])
        return dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:applicationMetadata.get() options:0 error:nil]);
    return { };
}

static String applicationMetadataString(NSDictionary *dictionary)
{
    if (RetainPtr applicationMetadata = dictionary ? [NSJSONSerialization dataWithJSONObject:dictionary options:NSJSONWritingSortedKeys error:nil] : nil)
        return adoptNS([[NSString alloc] initWithData:applicationMetadata.get() encoding:NSUTF8StringEncoding]).get();
    return { };
}

static RetainPtr<PKPaymentInstallmentConfiguration> createPlatformConfiguration(const ApplePayInstallmentConfiguration& coreConfiguration)
{
    if (!PAL::getPKPaymentInstallmentConfigurationClassSingleton())
        return nil;

    RetainPtr configuration = adoptNS([PAL::allocPKPaymentInstallmentConfigurationInstance() init]);

    [configuration setFeature:platformFeatureType(coreConfiguration.featureType)];

    [configuration setBindingTotalAmount:protect(toDecimalNumber(coreConfiguration.bindingTotalAmount)).get()];
    [configuration setCurrencyCode:coreConfiguration.currencyCode.createNSString().get()];
    [configuration setInStorePurchase:coreConfiguration.isInStorePurchase];
    [configuration setOpenToBuyThresholdAmount:protect(toDecimalNumber(coreConfiguration.openToBuyThresholdAmount)).get()];

    RetainPtr merchandisingImageData = adoptNS([[NSData alloc] initWithBase64EncodedString:coreConfiguration.merchandisingImageData.createNSString().get() options:0]);
    [configuration setMerchandisingImageData:merchandisingImageData.get()];
    [configuration setInstallmentMerchantIdentifier:coreConfiguration.merchantIdentifier.createNSString().get()];
    [configuration setReferrerIdentifier:coreConfiguration.referrerIdentifier.createNSString().get()];

    if (!PAL::getPKPaymentInstallmentItemClassSingleton())
        return configuration;

    [configuration setInstallmentItems:createNSArray(coreConfiguration.items).get()];
    [configuration setApplicationMetadata:applicationMetadataDictionary(coreConfiguration).get()];
    [configuration setRetailChannel:platformRetailChannel(coreConfiguration.retailChannel)];

    return configuration;
}

ExceptionOr<PaymentInstallmentConfiguration> PaymentInstallmentConfiguration::create(const ApplePayInstallmentConfiguration& configuration)
{
    auto dictionary = applicationMetadataDictionary(configuration);
    if (!configuration.applicationMetadata.isNull() && !dictionary)
        return Exception { ExceptionCode::TypeError, "applicationMetadata must be a JSON object"_s };

    return PaymentInstallmentConfiguration(ApplePayInstallmentConfiguration(configuration), WTF::move(dictionary));
}

static ApplePayInstallmentConfiguration addApplicationMetadata(ApplePayInstallmentConfiguration configuration, RetainPtr<NSDictionary>&& applicationMetadata)
{
    if (applicationMetadata)
        configuration.applicationMetadata = applicationMetadataString(applicationMetadata.get());
    return configuration;
}

PaymentInstallmentConfiguration::PaymentInstallmentConfiguration(const ApplePayInstallmentConfiguration& configuration, RetainPtr<NSDictionary>&& applicationMetadata)
    : m_configuration { addApplicationMetadata(configuration, WTF::move(applicationMetadata)) }
{
}

PaymentInstallmentConfiguration::PaymentInstallmentConfiguration(std::optional<ApplePayInstallmentConfiguration>&& configuration)
    : m_configuration { WTF::move(configuration) }
{
}

PaymentInstallmentConfiguration::PaymentInstallmentConfiguration(RetainPtr<PKPaymentInstallmentConfiguration>&& configuration)
    : m_configuration { applePayInstallmentConfiguration(configuration.get()) }
{
}

const std::optional<ApplePayInstallmentConfiguration>& NODELETE PaymentInstallmentConfiguration::applePayInstallmentConfiguration() const
{
    return m_configuration;
}

RetainPtr<PKPaymentInstallmentConfiguration> PaymentInstallmentConfiguration::platformConfiguration() const
{
    return m_configuration ? createPlatformConfiguration(*m_configuration) : nil;
}

std::optional<ApplePayInstallmentConfiguration> PaymentInstallmentConfiguration::applePayInstallmentConfiguration(PKPaymentInstallmentConfiguration *configuration)
{
    if (!configuration)
        return std::nullopt;

    ApplePayInstallmentConfiguration installmentConfiguration;
    if (!PAL::getPKPaymentInstallmentConfigurationClassSingleton())
        return std::nullopt;

    if (auto featureType = applePaySetupFeatureType([configuration feature]))
        installmentConfiguration.featureType = *featureType;
    else
        return std::nullopt;

    installmentConfiguration.bindingTotalAmount = fromDecimalNumber(retainPtr([configuration bindingTotalAmount]).get());
    installmentConfiguration.currencyCode = [configuration currencyCode];
    installmentConfiguration.isInStorePurchase = [configuration isInStorePurchase];
    installmentConfiguration.openToBuyThresholdAmount = fromDecimalNumber(retainPtr([configuration openToBuyThresholdAmount]).get());

    installmentConfiguration.merchandisingImageData = [retainPtr([configuration merchandisingImageData]) base64EncodedStringWithOptions:0];
    installmentConfiguration.merchantIdentifier = [configuration installmentMerchantIdentifier];
    installmentConfiguration.referrerIdentifier = [configuration referrerIdentifier];

    if (!PAL::getPKPaymentInstallmentItemClassSingleton())
        return WTF::move(installmentConfiguration);

    installmentConfiguration.items = makeVector<ApplePayInstallmentItem>(retainPtr([configuration installmentItems]).get());
    installmentConfiguration.applicationMetadata = applicationMetadataString(retainPtr([configuration applicationMetadata]).get());
    installmentConfiguration.retailChannel = applePayRetailChannel([configuration retailChannel]);

    return WTF::move(installmentConfiguration);
}

} // namespace WebCore

#endif // HAVE(PASSKIT_INSTALLMENTS)
