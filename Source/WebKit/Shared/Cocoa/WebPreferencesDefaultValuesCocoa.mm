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

#import "ImageAnalysisUtilities.h"
#import <Foundation/NSBundle.h>
#import <pal/spi/cocoa/FeatureFlagsSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

#if PLATFORM(MAC)
bool defaultScrollAnimatorEnabled()
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"NSScrollAnimationEnabled"];
}
#endif

#if ENABLE(IMAGE_ANALYSIS)

bool defaultTextRecognitionInVideosEnabled()
{
    static bool enabled = false;
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    static std::once_flag flag;
    std::call_once(flag, [] {
        enabled = os_feature_enabled(VisualIntelligence, LiveText);
    });
#endif
    return enabled;
}

bool defaultVisualTranslationEnabled()
{
    static bool enabled = false;
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    static std::once_flag flag;
    std::call_once(flag, [] {
        enabled = os_feature_enabled(Translate, EnableVisualIntelligenceUI);
    });
#endif
    return enabled;
}

bool defaultRemoveBackgroundEnabled()
{
    static bool enabled = false;
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    static std::once_flag flag;
    std::call_once(flag, [] {
        enabled = os_feature_enabled(VisualIntelligence, RemoveBackground);
    });
#endif
    return enabled;
}

#endif // ENABLE(IMAGE_ANALYSIS)

#if HAVE(SC_CONTENT_SHARING_PICKER)
bool defaultUseSCContentSharingPicker()
{
    static bool enabled = false;
    static std::once_flag flag;
    std::call_once(flag, [] {
        enabled = os_feature_enabled(Bilby, Newsroom);
    });
    return enabled;
}
#endif

} // namespace WebKit

#endif // PLATFORM(COCOA)
