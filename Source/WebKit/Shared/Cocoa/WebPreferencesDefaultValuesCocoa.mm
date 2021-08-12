/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WebPreferencesDefaultValues.h"

#if PLATFORM(COCOA)

#import <Foundation/NSBundle.h>

#import <pal/spi/cocoa/FeatureFlagsSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

// Because of <rdar://problem/60608008>, WebKit has to parse the feature flags plist file
bool isFeatureFlagEnabled(const char* featureName, bool defaultValue)
{
#if HAVE(SYSTEM_FEATURE_FLAGS)

#if PLATFORM(MAC)
    static bool isSystemWebKit = [] {
        NSBundle *bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
        return [bundle.bundlePath hasPrefix:@"/System/"];
    }();

    if (isSystemWebKit)
        return _os_feature_enabled_impl("WebKit", featureName);

    return defaultValue;
#else
    UNUSED_PARAM(defaultValue);
    return _os_feature_enabled_impl("WebKit", featureName);
#endif // PLATFORM(MAC)

#else

    UNUSED_PARAM(featureName);
    return defaultValue;

#endif // HAVE(SYSTEM_FEATURE_FLAGS)
}

} // namespace WebKit

#endif
