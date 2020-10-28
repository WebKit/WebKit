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

const TestFeatures& TestOptions::defaults()
{
    static TestFeatures features;
    if (features.boolWebPreferenceFeatures.empty()) {
        features.boolWebPreferenceFeatures = {
            // These are WebPreference values that must always be set as they may
            // differ from the default set in the WebPreferences*.yaml configuration.
            { "AllowCrossOriginSubresourcesToAskForCredentials", false },
            { "AllowTopNavigationToDataURLs", true },
            { "AcceleratedDrawingEnabled", false },
            { "AttachmentElementEnabled", false },
            { "UsesBackForwardCache", false },
            { "ColorFilterEnabled", false },
            { "InspectorAdditionsEnabled", false },
            { "IntersectionObserverEnabled", false },
            { "KeygenElementEnabled", false },
            { "MenuItemElementEnabled", false },
            { "ModernMediaControlsEnabled", true },

            { "CSSLogicalEnabled", false },
            { "LineHeightUnitsEnabled", false },
            { "SelectionAcrossShadowBoundariesEnabled", true },
            { "LayoutFormattingContextIntegrationEnabled", true },

            { "AdClickAttributionEnabled", false },
            { "AspectRatioOfImgFromWidthAndHeightEnabled", false },
            { "AsyncClipboardAPIEnabled", false },
            { "CSSOMViewSmoothScrollingEnabled", false },
            { "ContactPickerAPIEnabled", false },
            { "CoreMathMLEnabled", false },
            { "RequestIdleCallbackEnabled", false },
            { "ResizeObserverEnabled", false },
            { "WebGPUEnabled", false },
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

bool TestOptions::webViewIsCompatibleWithOptions(const TestOptions& options) const
{
    if (m_features.experimentalFeatures != options.m_features.experimentalFeatures)
        return false;
    if (m_features.internalDebugFeatures != options.m_features.internalDebugFeatures)
        return false;
    if (m_features.boolWebPreferenceFeatures != options.m_features.boolWebPreferenceFeatures)
        return false;
    if (m_features.doubleWebPreferenceFeatures != options.m_features.doubleWebPreferenceFeatures)
        return false;
    if (m_features.uint32WebPreferenceFeatures != options.m_features.uint32WebPreferenceFeatures)
        return false;
    if (m_features.stringWebPreferenceFeatures != options.m_features.stringWebPreferenceFeatures)
        return false;
    if (m_features.boolTestRunnerFeatures != options.m_features.boolTestRunnerFeatures)
        return false;
    if (m_features.doubleTestRunnerFeatures != options.m_features.doubleTestRunnerFeatures)
        return false;
    if (m_features.stringTestRunnerFeatures != options.m_features.stringTestRunnerFeatures)
        return false;
    if (m_features.stringVectorTestRunnerFeatures != options.m_features.stringVectorTestRunnerFeatures)
        return false;
    return true;
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
