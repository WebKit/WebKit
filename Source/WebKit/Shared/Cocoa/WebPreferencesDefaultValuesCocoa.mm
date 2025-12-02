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
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/system/ios/UserInterfaceIdiom.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/text/WTFString.h>

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WebPreferencesDefaultValuesCocoaAdditions.mm>)
#import <WebKitAdditions/WebPreferencesDefaultValuesCocoaAdditions.mm>
#else
#if HAVE(LIQUID_GLASS)
static bool platformIsLiquidGlassEnabled()
{
    return true;
}
#endif
#endif

namespace WebKit {

#if HAVE(LIQUID_GLASS)
static std::optional<bool>& cachedIsLiqudGlassEnabled()
{
    static std::optional<bool> isLiquidGlassEnabled;
    return isLiquidGlassEnabled;
}

bool isLiquidGlassEnabled()
{
    if (auto isLiquidGlassEnabled = cachedIsLiqudGlassEnabled())
        return *isLiquidGlassEnabled;
    ASSERT_WITH_MESSAGE(!isInAuxiliaryProcess(), "isLiquidGlassEnabled() must not be called before setLiquidGlassEnabled() in auxiliary processes");
    return platformIsLiquidGlassEnabled();
}

void setLiquidGlassEnabled(bool isLiquidGlassEnabled)
{
    cachedIsLiqudGlassEnabled() = isLiquidGlassEnabled;
}
#endif // HAVE(LIQUID_GLASS)

#if PLATFORM(MAC)
bool defaultScrollAnimatorEnabled()
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"NSScrollAnimationEnabled"];
}
#endif

#if ENABLE(IMAGE_ANALYSIS)

bool defaultTextRecognitionInVideosEnabled()
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    static bool enabled = os_feature_enabled(VisualIntelligence, LiveText);
#else
    static bool enabled = false;
#endif
    return enabled;
}

bool defaultVisualTranslationEnabled()
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    static bool enabled = os_feature_enabled(Translate, EnableVisualIntelligenceUI);
#else
    static bool enabled = false;
#endif
    return enabled;
}

bool defaultRemoveBackgroundEnabled()
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    static bool enabled = os_feature_enabled(VisualIntelligence, RemoveBackground);
#else
    static bool enabled = false;
#endif
    return enabled;
}

#endif // ENABLE(IMAGE_ANALYSIS)

bool defaultTopContentInsetBackgroundCanChangeAfterScrolling()
{
#if PLATFORM(IOS_FAMILY)
    return PAL::currentUserInterfaceIdiomIsSmallScreen();
#else
    return false;
#endif
}

bool defaultContentInsetBackgroundFillEnabled()
{
#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    return isLiquidGlassEnabled();
#else
    return false;
#endif
}

#if HAVE(MATERIAL_HOSTING)
bool defaultHostedBlurMaterialInMediaControlsEnabled()
{
    return isLiquidGlassEnabled();
}
#endif

bool defaultIOSurfaceLosslessCompressionEnabled()
{
#if HAVE(COREVIDEO_COMPRESSED_PIXEL_FORMAT_TYPES) && HAVE(LOSSLESS_COMPRESSED_IOSURFACE_CG_SUPPORT)
#define WK_CA_FEATURE_CG_COMPRESSED_IOSURFACES 15
    return CASupportsFeature(WK_CA_FEATURE_CG_COMPRESSED_IOSURFACES);
#undef WK_CA_FEATURE_CG_COMPRESSED_IOSURFACES
#else
    return false;
#endif
}

#if ENABLE(UNIFIED_PDF)
bool defaultUnifiedPDFEnabled()
{
#if ENABLE(UNIFIED_PDF_BY_DEFAULT)
    return true;
#else
    return false;
#endif
}
#endif

} // namespace WebKit

#endif // PLATFORM(COCOA)
