/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
            // These are WebPreference values that must always be set as they may
            // differ from the default set in the WebPreferences*.yaml configuration.
            { "AcceleratedDrawingEnabled", false },
            { "AllowCrossOriginSubresourcesToAskForCredentials", false },
            { "AllowFileAccessFromFileURLs", true },
            { "AllowTopNavigationToDataURLs", true },
            { "AllowUniversalAccessFromFileURLs", true },
            { "AspectRatioOfImgFromWidthAndHeightEnabled", false },
            { "AspectRatioEnabled", true },
            { "AsyncClipboardAPIEnabled", false },
            { "AttachmentElementEnabled", false },
            { "CSSLogicalEnabled", false },
            { "CSSOMViewSmoothScrollingEnabled", false },
            { "ColorFilterEnabled", false },
            { "ContactPickerAPIEnabled", false },
            { "CoreMathMLEnabled", false },
            { "DOMPasteAllowed", true },
            { "DeveloperExtrasEnabled", true },
            { "HiddenPageDOMTimerThrottlingEnabled", false },
            { "InspectorAdditionsEnabled", false },
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
            { "ModernMediaControlsEnabled", true },
            { "NeedsStorageAccessFromFileURLsQuirk", false },
            { "OverscrollBehaviorEnabled", true },
            { "PluginsEnabled", true },
            { "PrivateClickMeasurementEnabled", false },
            { "RequestIdleCallbackEnabled", false },
            { "ResizeObserverEnabled", false },
            { "SelectionAcrossShadowBoundariesEnabled", true },
            { "ShrinksStandaloneImagesToFit", true },
            { "SpatialNavigationEnabled", false },
            { "TabsToLinks", false },
            { "TelephoneNumberParsingEnabled", false },
            { "UsesBackForwardCache", false },
            { "WebGPUEnabled", false },
            { "XSSAuditorEnabled", false },
        };
        features.uint32WebPreferenceFeatures = {
            { "MinimumFontSize", 0 },
        };
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
