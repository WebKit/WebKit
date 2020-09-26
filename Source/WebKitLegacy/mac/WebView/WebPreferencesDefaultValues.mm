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

#import "WebPreferencesDefaultValues.h"

#import <Foundation/NSBundle.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <mach-o/dyld.h>
#import <mutex>
#import <pal/spi/cocoa/FeatureFlagsSPI.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

#if PLATFORM(COCOA) && HAVE(SYSTEM_FEATURE_FLAGS)

// Because of <rdar://problem/60608008>, WebKit has to parse the feature flags plist file
bool isFeatureFlagEnabled(const String& featureName)
{
    BOOL isWebKitBundleFromStagedFramework = [[[NSBundle mainBundle] bundlePath] hasPrefix:@"/Library/Apple/System/Library/StagedFrameworks/WebKit"];

    if (!isWebKitBundleFromStagedFramework)
        return _os_feature_enabled_impl("WebKit", (const char*)featureName.utf8().data());

    static NSDictionary* dictionary = [[NSDictionary dictionaryWithContentsOfFile:@"/Library/Apple/System/Library/FeatureFlags/Domain/WebKit.plist"] retain];

    if (![[dictionary objectForKey:featureName] objectForKey:@"Enabled"])
        return _os_feature_enabled_impl("WebKit", (const char*)featureName.characters8());

    return [[[dictionary objectForKey:featureName] objectForKey:@"Enabled"] isKindOfClass:[NSNumber class]] && [[[dictionary objectForKey:featureName] objectForKey:@"Enabled"] boolValue];
}

#endif

#if ENABLE(WEBGPU)

bool defaultWebGPUEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("WebGPU");
#endif

    return false;
}

#endif // ENABLE(WEBGPU)

#if HAVE(INCREMENTAL_PDF_APIS)
bool defaultIncrementalPDFEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("incremental_pdf");
#endif

    return false;
}
#endif

#if ENABLE(WEBXR)

bool defaultWebXREnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("WebXR");
#endif

    return false;
}

#endif // ENABLE(WEBXR)

} // namespace WebKit
