/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 */

#import "config.h"
#import "PaymentSetupConfiguration.h"

#if HAVE(PASSKIT_PAYMENT_SETUP)

#import "ArgumentCodersCocoa.h"
#import "Decoder.h"
#import "Encoder.h"
#import "PassKitSoftLinkAdditions.h"
#import <wtf/URL.h>

@interface PKPaymentSetupConfiguration (WebKit)
@property (nonatomic, copy) NSArray<NSString *> *signedFields;
@end

namespace WebKitAdditions {

static RetainPtr<PKPaymentSetupConfiguration> toPlatformConfiguration(const WebCore::ApplePaySetup::Configuration& coreConfiguration, const URL& url)
{
#if PLATFORM(MAC)
    if (!PAL::getPKPaymentSetupConfigurationClass())
        return nil;
#endif

    auto configuration = adoptNS([PAL::allocPKPaymentSetupConfigurationInstance() init]);
    [configuration setMerchantIdentifier:coreConfiguration.merchantIdentifier];
    [configuration setOriginatingURL:url];
    [configuration setReferrerIdentifier:coreConfiguration.referrerIdentifier];
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    [configuration setSignature:coreConfiguration.signature];
ALLOW_NEW_API_WITHOUT_GUARDS_END

    if ([configuration respondsToSelector:@selector(setSignedFields:)]) {
        NSMutableArray *signedFields = [NSMutableArray arrayWithCapacity:coreConfiguration.signedFields.size()];
        for (auto& signedField : coreConfiguration.signedFields)
            [signedFields addObject:signedField];
        [configuration setSignedFields:signedFields];
    }

    return configuration;
}

PaymentSetupConfiguration::PaymentSetupConfiguration(const WebCore::ApplePaySetup::Configuration& configuration, const URL& url)
    : m_configuration { toPlatformConfiguration(configuration, url) }
{
}

PaymentSetupConfiguration::PaymentSetupConfiguration(RetainPtr<PKPaymentSetupConfiguration>&& configuration)
    : m_configuration { WTFMove(configuration) }
{
}

void PaymentSetupConfiguration::encode(IPC::Encoder& encoder) const
{
    encoder << m_configuration;
}

Optional<PaymentSetupConfiguration> PaymentSetupConfiguration::decode(IPC::Decoder& decoder)
{
    static NSArray *allowedClasses;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        auto allowed = adoptNS([[NSMutableArray alloc] initWithCapacity:1]);
        if (auto pkPaymentSetupConfigurationClass = PAL::getPKPaymentSetupConfigurationClass())
            [allowed addObject:pkPaymentSetupConfigurationClass];
        allowedClasses = [allowed copy];
    });

    auto configuration = IPC::decode<PKPaymentSetupConfiguration>(decoder, allowedClasses);
    if (!configuration)
        return WTF::nullopt;

    return PaymentSetupConfiguration { WTFMove(*configuration) };
}

} // namespace WebKitAdditions

#endif // HAVE(PASSKIT_PAYMENT_SETUP)
