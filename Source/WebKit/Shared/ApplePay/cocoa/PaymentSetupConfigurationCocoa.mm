/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "PaymentSetupConfigurationWebKit.h"

#if ENABLE(APPLE_PAY)

#import "ArgumentCodersCocoa.h"
#import "Decoder.h"
#import "Encoder.h"
#import <WebCore/ApplePaySetupConfiguration.h>
#import <wtf/URL.h>

#import <pal/cocoa/PassKitSoftLink.h>

@interface PKPaymentSetupConfiguration (WebKit)
@property (nonatomic, copy) NSArray<NSString *> *signedFields;
@end

namespace WebKit {

static RetainPtr<PKPaymentSetupConfiguration> toPlatformConfiguration(const WebCore::ApplePaySetupConfiguration& coreConfiguration, const URL& url)
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

RetainPtr<PKPaymentSetupConfiguration> PaymentSetupConfiguration::platformConfiguration() const
{
    return toPlatformConfiguration(m_configuration, m_url);
}

PaymentSetupConfiguration::PaymentSetupConfiguration(const WebCore::ApplePaySetupConfiguration& configuration, const URL& url)
    : m_configuration(configuration)
    , m_url(url)
{
}

} // namespace WebKit

#endif // ENABLE(APPLE_PAY)
