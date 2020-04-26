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
#import <pal/cocoa/PassKitSoftLink.h>

namespace WebCore {

// FIXME: Merge with toDecimalNumber() in WebPaymentCoordinatorProxyCocoa.
static NSDecimalNumber *toDecimalNumber(const String& amount)
{
    if (!amount)
        return [NSDecimalNumber zero];
    return [NSDecimalNumber decimalNumberWithString:amount locale:@{ NSLocaleDecimalSeparator : @"." }];
}

static PKPaymentSetupFeatureType platformFeatureType(const ApplePaySetupFeatureType& featureType)
{
    switch (featureType) {
    case ApplePaySetupFeatureType::ApplePay:
        return PKPaymentSetupFeatureTypeApplePay;
    case ApplePaySetupFeatureType::AppleCard:
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        return PKPaymentSetupFeatureTypeApplePay_X;
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
}

static RetainPtr<PKPaymentInstallmentConfiguration> createPlatformConfiguration(const ApplePayInstallmentConfiguration& coreConfiguration)
{
    auto configuration = adoptNS([PAL::allocPKPaymentInstallmentConfigurationInstance() init]);

    [configuration setFeature:platformFeatureType(coreConfiguration.featureType)];

    [configuration setBindingTotalAmount:toDecimalNumber(coreConfiguration.bindingTotalAmount)];
    [configuration setCurrencyCode:coreConfiguration.currencyCode];
    [configuration setInStorePurchase:coreConfiguration.isInStorePurchase];
    [configuration setOpenToBuyThresholdAmount:toDecimalNumber(coreConfiguration.openToBuyThresholdAmount)];

    auto merchandisingImageData = adoptNS([[NSData alloc] initWithBase64EncodedString:coreConfiguration.merchandisingImageData options:0]);
    [configuration setMerchandisingImageData:merchandisingImageData.get()];

#if HAVE(PASSKIT_INSTALLMENT_IDENTIFIERS)
#if PLATFORM(MAC)
    if (![configuration respondsToSelector:@selector(setInstallmentMerchantIdentifier:)] || ![configuration respondsToSelector:@selector(setReferrerIdentifier:)])
        return configuration;
#endif
    [configuration setInstallmentMerchantIdentifier:coreConfiguration.merchantIdentifier];
    [configuration setReferrerIdentifier:coreConfiguration.referrerIdentifier];
#endif

    return configuration;
}

PaymentInstallmentConfiguration::PaymentInstallmentConfiguration(const ApplePayInstallmentConfiguration& configuration)
    : m_configuration { createPlatformConfiguration(configuration) }
{
}

PaymentInstallmentConfiguration::PaymentInstallmentConfiguration(RetainPtr<PKPaymentInstallmentConfiguration>&& configuration)
    : m_configuration { WTFMove(configuration) }
{
}

PKPaymentInstallmentConfiguration *PaymentInstallmentConfiguration::platformConfiguration() const
{
    return m_configuration.get();
}

} // namespace WebCore

#endif // HAVE(PASSKIT_INSTALLMENTS)
