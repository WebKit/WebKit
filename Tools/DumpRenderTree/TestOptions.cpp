/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TestOptions.h"

#include "TestOptionsGeneratedKeys.h"

namespace WTR {

const std::vector<std::string>& TestOptions::supportedBoolWebPreferenceFeatures()
{
    // FIXME: Remove this once there is a viable mechanism for reseting WebPreferences between tests,
    // at which point, we will not need to manually reset every supported preference for each test.

    static std::vector<std::string> supported = [] {
        std::vector<std::string> keys;
        for (const auto& [key, value] : defaults().boolWebPreferenceFeatures)
            keys.push_back(key);
        return keys;
    }();
    return supported;
}

const std::vector<std::string>& TestOptions::supportedUInt32WebPreferenceFeatures()
{
    // FIXME: Remove this once there is a viable mechanism for reseting WebPreferences between tests,
    // at which point, we will not need to manually reset every supported preference for each test.

    static std::vector<std::string> supported = [] {
        std::vector<std::string> keys;
        for (const auto& [key, value] : defaults().uint32WebPreferenceFeatures)
            keys.push_back(key);
        return keys;
    }();
    return supported;
}

const TestFeatures& TestOptions::defaults()
{
    static TestFeatures features;
    if (features.boolWebPreferenceFeatures.empty()) {
        features.boolWebPreferenceFeatures = {
#if PLATFORM(COCOA)
            // These are non-experimental WebPreference values that must always be set as they
            // differ from the default set in the WebPreferences*.yaml configuration.
            { "AllowsInlineMediaPlayback", true },
            { "CanvasUsesAcceleratedDrawing", true },
            { "ColorFilterEnabled", true },
            { "CustomPasteboardDataEnabled", true },
            { "DOMPasteAllowed", true },
            { "DeveloperExtrasEnabled", true },
            { "DirectoryUploadEnabled", true },
            { "DownloadAttributeEnabled", true },
            { "EncryptedMediaAPIEnabled", true },
            { "FullScreenEnabled", true },
            { "GamepadsEnabled", true },
            { "HiddenPageCSSAnimationSuspensionEnabled", false },
            { "InlineMediaPlaybackRequiresPlaysInlineAttribute", false },
            { "JavaScriptCanAccessClipboard", true },
            { "JavaScriptCanOpenWindowsAutomatically", true },
            { "LargeImageAsyncDecodingEnabled", false },
            { "LinkPreloadEnabled", true },
            { "MediaCapabilitiesEnabled", true },
            { "MediaDataLoadsAutomatically", true },
            { "MediaDevicesEnabled", true },
            { "MediaPreloadingEnabled", true },
            { "MockScrollbarsEnabled", true },
            { "NeedsStorageAccessFromFileURLsQuirk", false },
            { "OfflineWebApplicationCacheEnabled", true },
            { "RequiresUserGestureForAudioPlayback", false },
            { "RequiresUserGestureForMediaPlayback", false },
            { "RequiresUserGestureForVideoPlayback", false },
            { "ScrollToTextFragmentIndicatorEnabled", false },
            { "ShouldPrintBackgrounds", true },
            { "ShrinksStandaloneImagesToFit", true },
            { "TextAreasAreResizable", true },
            { "TextAutosizingEnabled", false },
            { "UsesBackForwardCache", false },
            { "WebAudioEnabled", true },
            { "WebSQLEnabled", true },
            { "XSSAuditorEnabled", false },

            // FIXME: These experimental features are currently the only ones not enabled for WebKitLegacy, we
            // should either enable them or stop exposing them (as we do with with preferences).
            // All other experimental features are automatically enabled regardless of their specified defaults.
            { "AsyncClipboardAPIEnabled", false },
            { "CSSOMViewSmoothScrollingEnabled", false },
            { "ContactPickerAPIEnabled", false },
            { "CoreMathMLEnabled", false },
            { "GenericCueAPIEnabled", false },
            { "IsLoggedInAPIEnabled", false },
            { "LazyIframeLoadingEnabled", false },
            { "LazyImageLoadingEnabled", false },
            { "RequestIdleCallbackEnabled", false },
            { "WebAuthenticationEnabled", false },
#elif PLATFORM(WIN)
            // These are WebPreference values that must always be set as they may
            // differ from the default set in the WebPreferences*.yaml configuration.
            { "AcceleratedDrawingEnabled", false },
            { "AllowCrossOriginSubresourcesToAskForCredentials", false },
            { "AllowFileAccessFromFileURLs", true },
            { "AllowTopNavigationToDataURLs", true },
            { "AllowUniversalAccessFromFileURLs", true },
            { "AspectRatioEnabled", true },
            { "AsyncClipboardAPIEnabled", false },
            { "AttachmentElementEnabled", false },
            { "CSSContainmentEnabled", false },
            { "CSSCounterStyleAtRuleImageSymbolsEnabled", false },
            { "CSSCounterStyleAtRulesEnabled", false },
            { "CSSGradientInterpolationColorSpacesEnabled", true },
            { "CSSGradientPremultipliedAlphaInterpolationEnabled", true },
            { "CSSInputSecurityEnabled", true },
            { "CSSLogicalEnabled", false },
            { "CSSOMViewSmoothScrollingEnabled", false },
            { "CSSTextAlignLastEnabled", true },
            { "CSSTextJustifyEnabled", true },
            { "CanvasColorSpaceEnabled", true },
            { "ColorFilterEnabled", false },
            { "ContactPickerAPIEnabled", false },
            { "CoreMathMLEnabled", false },
            { "DOMPasteAllowed", true },
            { "DeveloperExtrasEnabled", true },
            { "HiddenPageDOMTimerThrottlingEnabled", false },
            { "IntersectionObserverEnabled", false },
            { "JavaScriptCanAccessClipboard", true },
            { "JavaScriptCanOpenWindowsAutomatically", true },
            { "JavaScriptEnabled", true },
            { "KeygenElementEnabled", false },
            { "LayoutFormattingContextIntegrationEnabled", true },
            { "LineHeightUnitsEnabled", false },
            { "LoadsImagesAutomatically", true },
            { "MainContentUserGestureOverrideEnabled", false },
            { "MenuItemElementEnabled", false },
            { "MockScrollbarsControllerEnabled", false },
            { "ModernMediaControlsEnabled", true },
            { "NeedsStorageAccessFromFileURLsQuirk", false },
            { "OverscrollBehaviorEnabled", true },
            { "PerformanceNavigationTimingAPIEnabled", true },
            { "PluginsEnabled", true },
            { "PrivateClickMeasurementEnabled", false },
            { "RequestIdleCallbackEnabled", false },
            { "SelectionAcrossShadowBoundariesEnabled", true },
            { "ShrinksStandaloneImagesToFit", true },
            { "SpatialNavigationEnabled", false },
            { "TabsToLinks", false },
            { "TelephoneNumberParsingEnabled", false },
            { "UsesBackForwardCache", false },
            { "XSSAuditorEnabled", false },
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)
            { "UseGPUProcessForWebGLEnabled", false },
#endif
        };
#if PLATFORM(WIN)
        features.uint32WebPreferenceFeatures = {
            { "MinimumFontSize", 0 },
        };
#endif
#if PLATFORM(COCOA)
        features.stringWebPreferenceFeatures = {
            { "CursiveFontFamily", "Apple Chancery" },
            { "FantasyFontFamily", "Papyrus" },
            { "FixedFontFamily", "Courier" },
            { "PictographFontFamily", "Apple Color Emoji" },
            { "SansSerifFontFamily", "Helvetica" },
            { "SerifFontFamily", "Times" },
            { "StandardFontFamily", "Times" },
        };
#endif
    }
    return features;
}

const std::unordered_map<std::string, TestHeaderKeyType>& TestOptions::keyTypeMapping()
{
    static const std::unordered_map<std::string, TestHeaderKeyType> map {
        GENERATED_WEB_PREFERENCE_KEY_TYPE_MAPPINGS

        { "dumpJSConsoleLogInStdErr", TestHeaderKeyType::BoolTestRunner },
        { "enableDragDestinationActionLoad", TestHeaderKeyType::BoolTestRunner },
        { "layerBackedWebView", TestHeaderKeyType::BoolTestRunner },
        { "useEphemeralSession", TestHeaderKeyType::BoolTestRunner },

        { "additionalSupportedImageTypes", TestHeaderKeyType::StringTestRunner },
        { "jscOptions", TestHeaderKeyType::StringTestRunner },
    };

    return map;
}

bool TestOptions::webViewIsCompatibleWithOptions(const TestOptions& other) const
{
    return m_features == other.m_features;
}

template<typename T> T featureValue(std::string key, T defaultValue, const std::unordered_map<std::string, T>& map)
{
    auto it = map.find(key);
    if (it != map.end())
        return it->second;
    return defaultValue;
}

bool TestOptions::boolTestRunnerFeatureValue(std::string key, bool defaultValue) const
{
    return featureValue(key, defaultValue, m_features.boolTestRunnerFeatures);
}

std::string TestOptions::stringTestRunnerFeatureValue(std::string key, std::string defaultValue) const
{
    return featureValue(key, defaultValue, m_features.stringTestRunnerFeatures);
}

}
