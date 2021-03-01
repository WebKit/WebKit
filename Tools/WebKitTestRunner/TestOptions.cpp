/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

const TestFeatures& TestOptions::defaults()
{
    static TestFeatures features;
    if (features.boolWebPreferenceFeatures.empty()) {
        features.boolWebPreferenceFeatures = {
            // These are WebPreference values that must always be set as they may
            // differ from the default set in the WebPreferences*.yaml configuration.
            { "AcceleratedDrawingEnabled", false },
            { "AllowFileAccessFromFileURLs", true },
            { "AllowTopNavigationToDataURLs", true },
            { "AllowUniversalAccessFromFileURLs", true },
            { "AsyncFrameScrollingEnabled", false },
            { "AsyncOverflowScrollingEnabled", false },
            { "CaptureAudioInGPUProcessEnabled", captureAudioInGPUProcessEnabledValue },
            { "CaptureAudioInUIProcessEnabled", false },
            { "CaptureVideoInGPUProcessEnabled", captureVideoInGPUProcessEnabledValue },
            { "CaptureVideoInUIProcessEnabled", false },
            { "DOMPasteAllowed", true },
            { "FrameFlatteningEnabled", false },
            { "GenericCueAPIEnabled", false },
            { "InputTypeDateEnabled", true },
            { "InputTypeDateTimeLocalEnabled", true },
            { "InputTypeMonthEnabled", true },
            { "InputTypeTimeEnabled", true },
            { "InputTypeWeekEnabled", true },
            { "JavaScriptCanAccessClipboard", true },
            { "JavaScriptCanOpenWindowsAutomatically", true },
            { "MockScrollbarsEnabled", true },
            { "ModernMediaControlsEnabled", true },
            { "NeedsSiteSpecificQuirks", false },
            { "NeedsStorageAccessFromFileURLsQuirk", false },
            { "OffscreenCanvasEnabled", true },
            { "PageVisibilityBasedProcessSuppressionEnabled", false },
            { "PluginsEnabled", true },
            { "SpeakerSelectionRequiresUserGesture", false },
            { "UsesBackForwardCache", false },
            { "WebAuthenticationEnabled", true },
            { "XSSAuditorEnabled", false },
        };
        features.boolTestRunnerFeatures = {
            { "allowsLinkPreview", true },
            { "dumpJSConsoleLogInStdErr", false },
            { "editable", false },
            { "enableInAppBrowserPrivacy", false },
            { "enableProcessSwapOnNavigation", true },
            { "enableProcessSwapOnWindowOpen", false },
            { "ignoreSynchronousMessagingTimeouts", false },
            { "ignoresViewportScaleLimits", false },
            { "isAppBoundWebView", false },
            { "runSingly", false },
            { "shouldHandleRunOpenPanel", true },
            { "shouldPresentPopovers", true },
            { "shouldShowTouches", false },
            { "shouldShowWebView", false },
            { "spellCheckingDots", false },
            { "textInteractionEnabled", true },
            { "useCharacterSelectionGranularity", false },
            { "useDataDetection", false },
            { "useEphemeralSession", false },
            { "useFlexibleViewport", false },
            { "useRemoteLayerTree", false },
            { "useThreadedScrolling", false },
        };
        features.doubleTestRunnerFeatures = {
            { "contentInset.top", 0 },
            { "deviceScaleFactor", 1 },
            { "viewHeight", 600 },
            { "viewWidth", 800 },
        };
        features.stringTestRunnerFeatures = {
            { "additionalSupportedImageTypes", { } },
            { "applicationBundleIdentifier", { } },
            { "applicationManifest", { } },
            { "contentMode", { } },
            { "jscOptions", { } },
            { "standaloneWebApplicationURL", { } },
        };
        features.stringVectorTestRunnerFeatures = {
            { "language", { } },
        };
    }
    
    return features;
}

const std::unordered_map<std::string, TestHeaderKeyType>& TestOptions::keyTypeMapping()
{
    static const std::unordered_map<std::string, TestHeaderKeyType> map {
        GENERATED_WEB_PREFERENCE_KEY_TYPE_MAPPINGS

        { "allowsLinkPreview", TestHeaderKeyType::BoolTestRunner },
        { "dumpJSConsoleLogInStdErr", TestHeaderKeyType::BoolTestRunner },
        { "editable", TestHeaderKeyType::BoolTestRunner },
        { "enableInAppBrowserPrivacy", TestHeaderKeyType::BoolTestRunner },
        { "enableProcessSwapOnNavigation", TestHeaderKeyType::BoolTestRunner },
        { "enableProcessSwapOnWindowOpen", TestHeaderKeyType::BoolTestRunner },
        { "ignoreSynchronousMessagingTimeouts", TestHeaderKeyType::BoolTestRunner },
        { "ignoresViewportScaleLimits", TestHeaderKeyType::BoolTestRunner },
        { "isAppBoundWebView", TestHeaderKeyType::BoolTestRunner },
        { "runSingly", TestHeaderKeyType::BoolTestRunner },
        { "shouldHandleRunOpenPanel", TestHeaderKeyType::BoolTestRunner },
        { "shouldPresentPopovers", TestHeaderKeyType::BoolTestRunner },
        { "shouldShowTouches", TestHeaderKeyType::BoolTestRunner },
        { "shouldShowWebView", TestHeaderKeyType::BoolTestRunner },
        { "spellCheckingDots", TestHeaderKeyType::BoolTestRunner },
        { "textInteractionEnabled", TestHeaderKeyType::BoolTestRunner },
        { "useCharacterSelectionGranularity", TestHeaderKeyType::BoolTestRunner },
        { "useDataDetection", TestHeaderKeyType::BoolTestRunner },
        { "useEphemeralSession", TestHeaderKeyType::BoolTestRunner },
        { "useFlexibleViewport", TestHeaderKeyType::BoolTestRunner },
        { "useRemoteLayerTree", TestHeaderKeyType::BoolTestRunner },
        { "useThreadedScrolling", TestHeaderKeyType::BoolTestRunner },
    
        { "contentInset.top", TestHeaderKeyType::DoubleTestRunner },
        { "deviceScaleFactor", TestHeaderKeyType::DoubleTestRunner },
        { "viewHeight", TestHeaderKeyType::DoubleTestRunner },
        { "viewWidth", TestHeaderKeyType::DoubleTestRunner },

        { "additionalSupportedImageTypes", TestHeaderKeyType::StringTestRunner },
        { "applicationBundleIdentifier", TestHeaderKeyType::StringTestRunner },
        { "applicationManifest", TestHeaderKeyType::StringRelativePathTestRunner },
        { "contentMode", TestHeaderKeyType::StringTestRunner },
        { "jscOptions", TestHeaderKeyType::StringTestRunner },
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

std::string TestOptions::stringTestRunnerFeatureValue(std::string key) const
{
    return testRunnerFeatureValue(key, m_features.stringTestRunnerFeatures);
}

std::vector<std::string> TestOptions::stringVectorTestRunnerFeatureValue(std::string key) const
{
    return testRunnerFeatureValue(key, m_features.stringVectorTestRunnerFeatures);
}

}
