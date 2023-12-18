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
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cocoa/PassKitSoftLink.h>

namespace WebCore {

// FIXME: Merge with toDecimalNumber() in WebPaymentCoordinatorProxyCocoa.
static NSDecimalNumber *toDecimalNumber(const String& amount)
{
    if (!amount)
        return [NSDecimalNumber zero];
    return [NSDecimalNumber decimalNumberWithString:amount locale:@{ NSLocaleDecimalSeparator : @"." }];
}

static String fromDecimalNumber(NSDecimalNumber *number)
{
    auto numberFormatter = adoptNS([[NSNumberFormatter alloc] init]);
    [numberFormatter setNumberStyle:NSNumberFormatterNoStyle];
    [numberFormatter setMinimumIntegerDigits:1];
    [numberFormatter setMinimumFractionDigits:2];
    [numberFormatter setMaximumFractionDigits:[numberFormatter maximumIntegerDigits]];
    return [numberFormatter stringFromNumber:number];
}

static std::optional<ApplePaySetupFeatureType> applePaySetupFeatureType(PKPaymentSetupFeatureType featureType)
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

static PKPaymentSetupFeatureType platformFeatureType(ApplePaySetupFeatureType featureType)
{
    switch (featureType) {
    case ApplePaySetupFeatureType::ApplePay:
        return PKPaymentSetupFeatureTypeApplePay;
    case ApplePaySetupFeatureType::AppleCard:
        return PKPaymentSetupFeatureTypeAppleCard;
    }
}

static ApplePayInstallmentItemType applePayItemType(PKInstallmentItemType itemType)
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

static PKInstallmentItemType platformItemType(ApplePayInstallmentItemType itemType)
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

static ApplePayInstallmentRetailChannel applePayRetailChannel(PKInstallmentRetailChannel retailChannel)
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

static PKInstallmentRetailChannel platformRetailChannel(ApplePayInstallmentRetailChannel retailChannel)
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
    ASSERT(PAL::getPKPaymentInstallmentItemClass());
    auto installmentItem = adoptNS([PAL::allocPKPaymentInstallmentItemInstance() init]);
    [installmentItem setInstallmentItemType:platformItemType(item.type)];
    [installmentItem setAmount:toDecimalNumber(item.amount)];
    [installmentItem setCurrencyCode:item.currencyCode];
    [installmentItem setProgramIdentifier:item.programIdentifier];
    [installmentItem setApr:toDecimalNumber(item.apr)];
    [installmentItem setProgramTerms:item.programTerms];
    return installmentItem;
}

static std::optional<ApplePayInstallmentItem> makeVectorElement(const ApplePayInstallmentItem*, id arrayElement)
{
    if (![arrayElement isKindOfClass:PAL::getPKPaymentInstallmentItemClass()])
        return std::nullopt;

    PKPaymentInstallmentItem *item = arrayElement;
    return ApplePayInstallmentItem {
        applePayItemType([item installmentItemType]),
        fromDecimalNumber([item amount]),
        [item currencyCode],
        [item programIdentifier],
        fromDecimalNumber([item apr]),
        [item programTerms],
    };
}

static RetainPtr<NSDictionary> applicationMetadataDictionary(const ApplePayInstallmentConfiguration& configuration)
{
    if (NSData *applicationMetadata = [configuration.applicationMetadata dataUsingEncoding:NSUTF8StringEncoding])
        return dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:applicationMetadata options:0 error:nil]);
    return { };
}

static String applicationMetadataString(NSDictionary *dictionary)
{
    if (NSData *applicationMetadata = dictionary ? [NSJSONSerialization dataWithJSONObject:dictionary options:NSJSONWritingSortedKeys error:nil] : nil)
        return adoptNS([[NSString alloc] initWithData:applicationMetadata encoding:NSUTF8StringEncoding]).get();
    return { };
}

static RetainPtr<PKPaymentInstallmentConfiguration> createPlatformConfiguration(const ApplePayInstallmentConfiguration& coreConfiguration)
{
    if (!PAL::getPKPaymentInstallmentConfigurationClass())
        return nil;

    auto configuration = adoptNS([PAL::allocPKPaymentInstallmentConfigurationInstance() init]);

    [configuration setFeature:platformFeatureType(coreConfiguration.featureType)];

    [configuration setBindingTotalAmount:toDecimalNumber(coreConfiguration.bindingTotalAmount)];
    [configuration setCurrencyCode:coreConfiguration.currencyCode];
    [configuration setInStorePurchase:coreConfiguration.isInStorePurchase];
    [configuration setOpenToBuyThresholdAmount:toDecimalNumber(coreConfiguration.openToBuyThresholdAmount)];

    auto merchandisingImageData = adoptNS([[NSData alloc] initWithBase64EncodedString:coreConfiguration.merchandisingImageData options:0]);
    [configuration setMerchandisingImageData:merchandisingImageData.get()];
    [configuration setInstallmentMerchantIdentifier:coreConfiguration.merchantIdentifier];
    [configuration setReferrerIdentifier:coreConfiguration.referrerIdentifier];

    if (!PAL::getPKPaymentInstallmentItemClass())
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

    return PaymentInstallmentConfiguration(ApplePayInstallmentConfiguration(configuration), WTFMove(dictionary));
}

static ApplePayInstallmentConfiguration addApplicationMetadata(ApplePayInstallmentConfiguration configuration, RetainPtr<NSDictionary>&& applicationMetadata)
{
    if (applicationMetadata)
        configuration.applicationMetadata = applicationMetadataString(applicationMetadata.get());
    return configuration;
}

PaymentInstallmentConfiguration::PaymentInstallmentConfiguration(const ApplePayInstallmentConfiguration& configuration, RetainPtr<NSDictionary>&& applicationMetadata)
    : m_configuration { addApplicationMetadata(configuration, WTFMove(applicationMetadata)) }
{
}

PaymentInstallmentConfiguration::PaymentInstallmentConfiguration(std::optional<ApplePayInstallmentConfiguration>&& configuration)
    : m_configuration { WTFMove(configuration) }
{
}

PaymentInstallmentConfiguration::PaymentInstallmentConfiguration(RetainPtr<PKPaymentInstallmentConfiguration>&& configuration)
    : m_configuration { applePayInstallmentConfiguration(configuration.get()) }
{
}

const std::optional<ApplePayInstallmentConfiguration>& PaymentInstallmentConfiguration::applePayInstallmentConfiguration() const
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
    if (!PAL::getPKPaymentInstallmentConfigurationClass())
        return std::nullopt;

    if (auto featureType = applePaySetupFeatureType([configuration feature]))
        installmentConfiguration.featureType = *featureType;
    else
        return std::nullopt;

    installmentConfiguration.bindingTotalAmount = fromDecimalNumber([configuration bindingTotalAmount]);
    installmentConfiguration.currencyCode = [configuration currencyCode];
    installmentConfiguration.isInStorePurchase = [configuration isInStorePurchase];
    installmentConfiguration.openToBuyThresholdAmount = fromDecimalNumber([configuration openToBuyThresholdAmount]);

    installmentConfiguration.merchandisingImageData = [[configuration merchandisingImageData] base64EncodedStringWithOptions:0];
    installmentConfiguration.merchantIdentifier = [configuration installmentMerchantIdentifier];
    installmentConfiguration.referrerIdentifier = [configuration referrerIdentifier];

    if (!PAL::getPKPaymentInstallmentItemClass())
        return WTFMove(installmentConfiguration);

    installmentConfiguration.items = makeVector<ApplePayInstallmentItem>([configuration installmentItems]);
    installmentConfiguration.applicationMetadata = applicationMetadataString([configuration applicationMetadata]);
    installmentConfiguration.retailChannel = applePayRetailChannel([configuration retailChannel]);

    return WTFMove(installmentConfiguration);
}

} // namespace WebCore

#endif // HAVE(PASSKIT_INSTALLMENTS)
