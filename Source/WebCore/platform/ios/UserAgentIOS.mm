/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#import "UserAgent.h"

#if PLATFORM(IOS_FAMILY)

#import "Quirks.h"
#import "SystemVersion.h"
#import <pal/spi/ios/MobileGestaltSPI.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <pal/system/ios/Device.h>
#import <wtf/RetainPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/text/MakeString.h>

#import <pal/ios/UIKitSoftLink.h>

namespace WebCore {

static inline bool isClassic()
{
    return [[PAL::getUIApplicationClassSingleton() sharedApplication] _isClassic];
}

static inline bool isClassicPad()
{
    return [PAL::getUIApplicationClassSingleton() _classicMode] == UIApplicationSceneClassicModeOriginalPad;
}

static inline bool isClassicPhone()
{
    return isClassic() && [PAL::getUIApplicationClassSingleton() _classicMode] != UIApplicationSceneClassicModeOriginalPad;
}

ASCIILiteral osNameForUserAgent()
{
    if (PAL::deviceHasIPadCapability() && !isClassicPhone())
        return "OS"_s;
    return "iPhone OS"_s;
}

static StringView deviceNameForUserAgent()
{
    if (isClassic()) {
        if (isClassicPad())
            return "iPad"_s;
        return "iPhone"_s;
    }

    static NeverDestroyed<String> name = [] {
        auto name = PAL::deviceName();
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        size_t location = name.find(" Simulator"_s);
        if (location != notFound)
            return name.left(location);
#endif
        return name;
    }();
    return name.get();
}

String standardUserAgentWithApplicationName(const String& applicationName, const String& userAgentOSVersion, UserAgentType type)
{
    String overrideUserAgent = Quirks::standardUserAgentWithApplicationNameIncludingCompatOverrides(applicationName, userAgentOSVersion, type);
    if (overrideUserAgent.length())
        return overrideUserAgent;

    auto separator = applicationName.isEmpty() ? ""_s : " "_s;

    if (type == UserAgentType::Desktop)
        return makeString("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko)"_s, separator, applicationName);

    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DoesNotOverrideUAFromNSUserDefault)) {
        if (auto override = dynamic_cf_cast<CFStringRef>(adoptCF(CFPreferencesCopyAppValue(CFSTR("UserAgent"), CFSTR("com.apple.WebFoundation"))))) {
            static BOOL hasLoggedDeprecationWarning = NO;
            if (!hasLoggedDeprecationWarning) {
                NSLog(@"Reading an override UA from the NSUserDefault [com.apple.WebFoundation UserAgent]. This is incompatible with the modern need to compose the UA and clients should use the API to set the application name or UA instead.");
                hasLoggedDeprecationWarning = YES;
            }
            return override.get();
        }
    }

    auto osVersion = userAgentOSVersion.isEmpty() ? systemMarketingVersionForUserAgentString() : userAgentOSVersion;
    return makeString("Mozilla/5.0 ("_s, deviceNameForUserAgent(), "; CPU "_s, osNameForUserAgent(), ' ', osVersion, " like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko)"_s, separator, applicationName);
}

} // namespace WebCore.

#endif // PLATFORM(IOS_FAMILY)
