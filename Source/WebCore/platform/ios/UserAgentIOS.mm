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

#import "Device.h"
#import "SystemVersion.h"
#import <pal/spi/ios/MobileGestaltSPI.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(UIKit);
SOFT_LINK_CLASS(UIKit, UIApplication);

namespace WebCore {

static inline bool isClassic()
{
    return [[getUIApplicationClass() sharedApplication] _isClassic];
}

static inline bool isClassicPad()
{
    return [getUIApplicationClass() _classicMode] == UIApplicationSceneClassicModeOriginalPad;
}

static inline bool isClassicPhone()
{
    return isClassic() && [getUIApplicationClass() _classicMode] != UIApplicationSceneClassicModeOriginalPad;
}

static inline NSString *osNameForUserAgent()
{
    if (deviceHasIPadCapability() && !isClassicPhone())
        return @"OS";
    return @"iPhone OS";
}

static inline NSString *deviceNameForUserAgent()
{
    if (isClassic()) {
        if (isClassicPad())
            return @"iPad";
        return @"iPhone";
    }

    NSString *name = deviceName();
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    NSUInteger location = [name rangeOfString:@" Simulator" options:NSBackwardsSearch].location;
    if (location != NSNotFound && location > 0)
        return [name substringToIndex:location];
#endif
    return name;
}

String standardUserAgentWithApplicationName(const String& applicationName)
{
    // Check to see if there is a user agent override for all WebKit clients.
    CFPropertyListRef override = CFPreferencesCopyAppValue(CFSTR("UserAgent"), CFSTR("com.apple.WebFoundation"));
    if (override) {
        if (CFGetTypeID(override) == CFStringGetTypeID())
            return static_cast<NSString *>(CFBridgingRelease(override));
        CFRelease(override);
    }

    // FIXME: We should implement this with String and/or StringBuilder instead.
    NSString *webKitVersion = userAgentBundleVersion();
    NSString *osMarketingVersionString = systemMarketingVersionForUserAgentString();
    if (applicationName.isEmpty())
        return [NSString stringWithFormat:@"Mozilla/5.0 (%@; CPU %@ %@ like Mac OS X) AppleWebKit/%@ (KHTML, like Gecko)", deviceNameForUserAgent(), osNameForUserAgent(), osMarketingVersionString, webKitVersion];
    return [NSString stringWithFormat:@"Mozilla/5.0 (%@; CPU %@ %@ like Mac OS X) AppleWebKit/%@ (KHTML, like Gecko) %@", deviceNameForUserAgent(), osNameForUserAgent(), osMarketingVersionString, webKitVersion, static_cast<NSString *>(applicationName)];
}

} // namespace WebCore.

#endif // PLATFORM(IOS_FAMILY)
