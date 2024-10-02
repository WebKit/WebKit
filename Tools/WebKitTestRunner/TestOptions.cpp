/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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
#include <wtf/Assertions.h>

namespace WTR {

#if PLATFORM(COCOA)
static constexpr bool captureAudioInGPUProcessEnabledValue = true;
static constexpr bool captureVideoInGPUProcessEnabledValue = true;
#else
static constexpr bool captureAudioInGPUProcessEnabledValue = false;
static constexpr bool captureVideoInGPUProcessEnabledValue = false;
#endif

#if PLATFORM(IOS_FAMILY_SIMULATOR)
static constexpr bool mediaSourceEnabledValue = false;
#else
static constexpr bool mediaSourceEnabledValue = true;
#endif

#if ENABLE(GPU_PROCESS)
#if ENABLE(GPU_PROCESS_DOM_RENDERING_BY_DEFAULT)
static constexpr bool fullGPUProcessEnabledValue = true;
#else
static constexpr bool fullGPUProcessEnabledValue = false;
#endif
#endif

#if ENABLE(UNIFIED_PDF)
#if ENABLE(UNIFIED_PDF_FOR_TESTING)
static constexpr bool unifiedPDFEnabledValue = true;
#else
static constexpr bool unifiedPDFEnabledValue = false;
#endif
#endif

#if PLATFORM(MAC)
static constexpr bool eventHandlerDrivenSmoothKeyboardScrollingEnabledValue = true;
#else
static constexpr bool eventHandlerDrivenSmoothKeyboardScrollingEnabledValue = false;
#endif

const TestFeatures& TestOptions::defaults()
{
    static TestFeatures features;
    if (features.boolWebPreferenceFeatures.empty()) {
        features.boolWebPreferenceFeatures = {
            // These are WebPreference values that must always be set as they may
            // differ from the default set in the WebPreferences*.yaml configuration.
            
            // Please do not add new options here if they are not necessary (e.g.
            // an experimental feature which gets enabled by default automatically)
            // as it adds a small amount of unnecessary work per-test.

            { "AllowFileAccessFromFileURLs", true },
            { "AllowTopNavigationToDataURLs", true },
            { "AllowUniversalAccessFromFileURLs", true },
            { "AllowsInlineMediaPlayback", true },
            { "AppBadgeEnabled", true },
            { "AsyncFrameScrollingEnabled", false },
            { "AsyncOverflowScrollingEnabled", false },
            { "BroadcastChannelOriginPartitioningEnabled", false },
            { "BuiltInNotificationsEnabled", false },
            { "CSSOMViewScrollingAPIEnabled", true },
            { "CSSUnprefixedBackdropFilterEnabled", true },
            { "CaptureAudioInGPUProcessEnabled", captureAudioInGPUProcessEnabledValue },
            { "CaptureAudioInUIProcessEnabled", false },
            { "CaptureVideoInGPUProcessEnabled", captureVideoInGPUProcessEnabledValue },
            { "CaptureVideoInUIProcessEnabled", false },
            { "ContentChangeObserverEnabled", false },
            { "CustomPasteboardDataEnabled", true },
            { "DOMPasteAllowed", true },
            { "DOMTestingAPIsEnabled", true },
            { "DataTransferItemsEnabled", true },
            { "DeveloperExtrasEnabled", true },
            { "DirectoryUploadEnabled", true },
            { "EncryptedMediaAPIEnabled", true },
            { "EventHandlerDrivenSmoothKeyboardScrollingEnabled", eventHandlerDrivenSmoothKeyboardScrollingEnabledValue },
            { "ExposeSpeakersEnabled", true },
            { "FullScreenEnabled", true },
            { "GenericCueAPIEnabled", false },
            { "HiddenPageCSSAnimationSuspensionEnabled", false },
            { "HiddenPageDOMTimerThrottlingEnabled", false },
            { "InlineMediaPlaybackRequiresPlaysInlineAttribute", false },
            { "InputTypeDateEnabled", true },
            { "InputTypeDateTimeLocalEnabled", true },
            { "InputTypeMonthEnabled", true },
            { "InputTypeTimeEnabled", true },
            { "InputTypeWeekEnabled", true },
            { "JavaScriptCanAccessClipboard", true },
            { "JavaScriptCanOpenWindowsAutomatically", true },
            { "LargeImageAsyncDecodingEnabled", false },
            { "MediaCapabilitiesEnabled", true },
            { "MediaDevicesEnabled", true },
            { "MediaPreloadingEnabled", true },
            { "MediaSourceEnabled", mediaSourceEnabledValue },
            { "MockScrollbarsControllerEnabled", false },
            { "MockCaptureDevicesEnabled", true },
            { "MockScrollbarsEnabled", true },
            { "ModernMediaControlsEnabled", true },
            { "NeedsSiteSpecificQuirks", false },
            { "NeedsStorageAccessFromFileURLsQuirk", false },
            { "PageVisibilityBasedProcessSuppressionEnabled", false },
            { "PeerConnectionVideoScalingAdaptationDisabled", true },
            { "PDFJSViewerEnabled", false },
            { "PushAPIEnabled", true },
            { "RequiresUserGestureForAudioPlayback", false },
            { "RequiresUserGestureForMediaPlayback", false },
            { "RequiresUserGestureForVideoPlayback", false },
            { "ScrollToTextFragmentIndicatorEnabled", false },
            { "ShowModalDialogEnabled", false },
            { "SpeakerSelectionRequiresUserGesture", false },
            { "TabsToLinks", false },
            { "TextAutosizingEnabled", false },
            { "TextAutosizingUsesIdempotentMode", false },
#if ENABLE(UNIFIED_PDF)
            { "UnifiedPDFEnabled", unifiedPDFEnabledValue },
#endif
            { "UsesBackForwardCache", false },
            { "WebAuthenticationEnabled", true },
#if ENABLE(WEBGL)
            { "WebGLDraftExtensionsEnabled", true },
#endif
            { "WebRTCRemoteVideoFrameEnabled", true },
            { "XSSAuditorEnabled", false },
#if PLATFORM(IOS_FAMILY_SIMULATOR)
            { "VP9DecoderEnabled", false },
#endif
#if ENABLE(GPU_PROCESS)
            { "UseGPUProcessForDOMRenderingEnabled", fullGPUProcessEnabledValue },
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && !PLATFORM(WIN)
            { "UseGPUProcessForWebGLEnabled", fullGPUProcessEnabledValue },
#endif
        };
        features.stringWebPreferenceFeatures = {
            { "CursiveFontFamily", "Apple Chancery" },
            { "DefaultTextEncodingName", "ISO-8859-1" },
            { "FantasyFontFamily", "Papyrus" },
            { "FixedFontFamily", "Courier" },
            { "PictographFontFamily", "Apple Color Emoji" },
            { "SansSerifFontFamily", "Helvetica" },
            { "SerifFontFamily", "Times" },
            { "StandardFontFamily", "Times" },
        };
        features.boolTestRunnerFeatures = {
            { "allowsLinkPreview", true },
            { "allowTestOnlyIPC", false },
            { "appHighlightsEnabled", false },
            { "dumpJSConsoleLogInStdErr", false },
            { "editable", false },
            { "enableInAppBrowserPrivacy", false },
            { "enableProcessSwapOnNavigation", true },
            { "findInteractionEnabled", false },
            { "ignoreSynchronousMessagingTimeouts", false },
            { "ignoresViewportScaleLimits", false },
            { "isAppBoundWebView", false },
            { "isAppInitiated", true },
            { "advancedPrivacyProtectionsEnabled", false },
            { "runSingly", false },
            { "runInCrossOriginFrame", false },
            { "shouldHandleRunOpenPanel", true },
            { "shouldPresentPopovers", true },
            { "shouldShowTouches", false },
            { "shouldShowWindow", false },
            { "spellCheckingDots", false },
            { "textInteractionEnabled", true },
            { "useCharacterSelectionGranularity", false },
            { "useDataDetection", false },
            { "useEphemeralSession", false },
            { "useFlexibleViewport", false },
            { "useRemoteLayerTree", false },
            { "noUseRemoteLayerTree", false },
            { "useThreadedScrolling", false },
            { "suppressInputAccessoryView", false },
            { "allowsInlinePredictions", false },
            { "showsScrollIndicators", true },
            { "longPressActionsEnabled", true },
            { "enhancedWindowingEnabled", false },
            { "textExtractionEnabled", false },
            { "useHardwareKeyboardMode", false },
        };
        features.doubleTestRunnerFeatures = {
            { "contentInset.top", 0 },
            { "obscuredInset.top", 0 },
            { "horizontalSystemMinimumLayoutMargin", 0 },
            { "deviceScaleFactor", 1 },
            { "viewHeight", 600 },
            { "viewWidth", 800 },
        };
        features.uint16TestRunnerFeatures = {
            { "insecureUpgradePort", 80 },
            { "secureUpgradePort", 443 },
        };
        features.stringTestRunnerFeatures = {
            { "additionalSupportedImageTypes", { } },
            { "applicationBundleIdentifier", { } },
            { "applicationManifest", { } },
            { "contentMode", { } },
            { "contentSecurityPolicyExtensionMode", { } },
            { "dragInteractionPolicy", { } },
            { "focusStartsInputSessionPolicy", { } },
            { "jscOptions", { } },
            { "captionDisplayMode", { } },
            { "standaloneWebApplicationURL", { } },
        };
        features.stringVectorTestRunnerFeatures = {
            { "language", { "en-US" } },
        };
    }
    
    return features;
}

const std::unordered_map<std::string, TestHeaderKeyType>& TestOptions::keyTypeMapping()
{
    static const std::unordered_map<std::string, TestHeaderKeyType> map {
        GENERATED_WEB_PREFERENCE_KEY_TYPE_MAPPINGS

        { "allowsLinkPreview", TestHeaderKeyType::BoolTestRunner },
        { "appHighlightsEnabled", TestHeaderKeyType::BoolTestRunner },
        { "allowTestOnlyIPC", TestHeaderKeyType::BoolTestRunner },
        { "dumpJSConsoleLogInStdErr", TestHeaderKeyType::BoolTestRunner },
        { "editable", TestHeaderKeyType::BoolTestRunner },
        { "enableInAppBrowserPrivacy", TestHeaderKeyType::BoolTestRunner },
        { "enableProcessSwapOnNavigation", TestHeaderKeyType::BoolTestRunner },
        { "findInteractionEnabled", TestHeaderKeyType::BoolTestRunner },
        { "ignoreSynchronousMessagingTimeouts", TestHeaderKeyType::BoolTestRunner },
        { "ignoresViewportScaleLimits", TestHeaderKeyType::BoolTestRunner },
        { "isAppBoundWebView", TestHeaderKeyType::BoolTestRunner },
        { "isAppInitiated", TestHeaderKeyType::BoolTestRunner },
        { "advancedPrivacyProtectionsEnabled", TestHeaderKeyType::BoolTestRunner },
        { "runSingly", TestHeaderKeyType::BoolTestRunner },
        { "runInCrossOriginFrame", TestHeaderKeyType::BoolTestRunner },
        { "shouldHandleRunOpenPanel", TestHeaderKeyType::BoolTestRunner },
        { "shouldPresentPopovers", TestHeaderKeyType::BoolTestRunner },
        { "shouldShowTouches", TestHeaderKeyType::BoolTestRunner },
        { "shouldShowWindow", TestHeaderKeyType::BoolTestRunner },
        { "spellCheckingDots", TestHeaderKeyType::BoolTestRunner },
        { "textInteractionEnabled", TestHeaderKeyType::BoolTestRunner },
        { "useCharacterSelectionGranularity", TestHeaderKeyType::BoolTestRunner },
        { "useDataDetection", TestHeaderKeyType::BoolTestRunner },
        { "useEphemeralSession", TestHeaderKeyType::BoolTestRunner },
        { "useFlexibleViewport", TestHeaderKeyType::BoolTestRunner },
        { "useRemoteLayerTree", TestHeaderKeyType::BoolTestRunner },
        { "useThreadedScrolling", TestHeaderKeyType::BoolTestRunner },
        { "suppressInputAccessoryView", TestHeaderKeyType::BoolTestRunner },
        { "allowsInlinePredictions", TestHeaderKeyType::BoolTestRunner },
        { "showsScrollIndicators", TestHeaderKeyType::BoolTestRunner },
        { "longPressActionsEnabled", TestHeaderKeyType::BoolTestRunner },
        { "enhancedWindowingEnabled", TestHeaderKeyType::BoolTestRunner },
        { "textExtractionEnabled", TestHeaderKeyType::BoolTestRunner },
        { "useHardwareKeyboardMode", TestHeaderKeyType::BoolTestRunner },
        { "contentInset.top", TestHeaderKeyType::DoubleTestRunner },
        { "obscuredInset.top", TestHeaderKeyType::DoubleTestRunner },
        { "horizontalSystemMinimumLayoutMargin", TestHeaderKeyType::DoubleTestRunner },
        { "deviceScaleFactor", TestHeaderKeyType::DoubleTestRunner },
        { "viewHeight", TestHeaderKeyType::DoubleTestRunner },
        { "viewWidth", TestHeaderKeyType::DoubleTestRunner },

        { "insecureUpgradePort", TestHeaderKeyType::UInt16TestRunner },
        { "secureUpgradePort", TestHeaderKeyType::UInt16TestRunner },

        { "additionalSupportedImageTypes", TestHeaderKeyType::StringTestRunner },
        { "applicationBundleIdentifier", TestHeaderKeyType::StringTestRunner },
        { "applicationManifest", TestHeaderKeyType::StringRelativePathTestRunner },
        { "contentMode", TestHeaderKeyType::StringTestRunner },
        { "contentSecurityPolicyExtensionMode", TestHeaderKeyType::StringTestRunner },
        { "dragInteractionPolicy", TestHeaderKeyType::StringTestRunner },
        { "focusStartsInputSessionPolicy", TestHeaderKeyType::StringTestRunner },
        { "jscOptions", TestHeaderKeyType::StringTestRunner },
        { "captionDisplayMode", TestHeaderKeyType::StringTestRunner },
        { "standaloneWebApplicationURL", TestHeaderKeyType::StringURLTestRunner },

        { "language", TestHeaderKeyType::StringVectorTestRunner },
    };

    return map;
}

bool TestOptions::hasSameInitializationOptions(const TestOptions& other) const
{
    return m_features == other.m_features;
}

bool TestOptions::boolWebPreferenceFeatureValue(std::string key, bool defaultValue) const
{
    auto it = m_features.boolWebPreferenceFeatures.find(key);
    if (it != m_features.boolWebPreferenceFeatures.end())
        return it->second;
    return defaultValue;
}

template<typename T> T testRunnerFeatureValue(std::string key, const std::unordered_map<std::string, T>& map)
{
    // All test runner features should always exist in their corresponding map since the base/global defaults
    // contains default values for all of them.

    auto it = map.find(key);
    ASSERT(it != map.end());
    return it->second;
}

bool TestOptions::boolTestRunnerFeatureValue(std::string key) const
{
    return testRunnerFeatureValue(key, m_features.boolTestRunnerFeatures);
}

double TestOptions::doubleTestRunnerFeatureValue(std::string key) const
{
    return testRunnerFeatureValue(key, m_features.doubleTestRunnerFeatures);
}

uint16_t TestOptions::uint16TestRunnerFeatureValue(std::string key) const
{
    return testRunnerFeatureValue(key, m_features.uint16TestRunnerFeatures);
}

std::string TestOptions::stringTestRunnerFeatureValue(std::string key) const
{
    return testRunnerFeatureValue(key, m_features.stringTestRunnerFeatures);
}

std::vector<std::string> TestOptions::stringVectorTestRunnerFeatureValue(std::string key) const
{
    return testRunnerFeatureValue(key, m_features.stringVectorTestRunnerFeatures);
}

}
