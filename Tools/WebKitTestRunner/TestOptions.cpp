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

#include <fstream>
#include <string>
#include <wtf/Assertions.h>
#include <wtf/StdFilesystem.h>

namespace WTR {

static const std::unordered_map<std::string, bool>& boolWebPreferencesDefaultsMap()
{
    static std::unordered_map<std::string, bool> map {
        { "AcceleratedDrawingEnabled", false },
        { "AllowCrossOriginSubresourcesToAskForCredentials", false },
        { "AllowTopNavigationToDataURLs", true },
        { "AttachmentElementEnabled", false },
        { "CaptureAudioInGPUProcessEnabled", false },
        { "CaptureAudioInUIProcessEnabled", false },
        { "CaptureVideoInGPUProcessEnabled", false },
        { "CaptureVideoInUIProcessEnabled", false },
        { "ColorFilterEnabled", false },
        { "DOMPasteAllowed", true },
        { "InspectorAdditionsEnabled", false },
        { "KeygenElementEnabled", false },
        { "MenuItemElementEnabled", false },
        { "MockScrollbarsEnabled", true },
        { "ModernMediaControlsEnabled", true },
        { "NeedsSiteSpecificQuirks", false },
        { "PageVisibilityBasedProcessSuppressionEnabled", false },
        { "PunchOutWhiteBackgroundsInDarkMode", false },
        { "ServiceControlsEnabled", false },
        { "ShouldIgnoreMetaViewport", false },
        { "ShouldUseServiceWorkerShortTimeout", false },
        { "UsesBackForwardCache", false },
        { "WebAuthenticationEnabled", true },
        { "WebAuthenticationLocalAuthenticatorEnabled", true },
    };
    return map;
}

static const std::unordered_map<std::string, bool>& boolTestRunnerDefaultsMap()
{
    static std::unordered_map<std::string, bool> map {
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
        { "useCharacterSelectionGranularity", false },
        { "useDataDetection", false },
        { "useEphemeralSession", false },
        { "useFlexibleViewport", false },
        { "useRemoteLayerTree", false },
        { "useThreadedScrolling", false },
    };
    return map;
}

static const std::unordered_map<std::string, double>& doubleTestRunnerDefaultsMap()
{
    static std::unordered_map<std::string, double> map {
        { "contentInset.top", 0 },
        { "deviceScaleFactor", 1 },
        { "viewHeight", 600 },
        { "viewWidth", 800 },
    };
    return map;
}

static const std::unordered_map<std::string, std::string>& stringTestRunnerDefaultsMap()
{
    static std::unordered_map<std::string, std::string> map {
        { "additionalSupportedImageTypes", { } },
        { "applicationBundleIdentifier", { } },
        { "applicationManifest", { } },
        { "contentMode", { } },
        { "jscOptions", { } },
        { "standaloneWebApplicationURL", { } },
    };
    return map;
}

static const std::unordered_map<std::string, std::vector<std::string>>& stringVectorTestRunnerDefaultsMap()
{
    static std::unordered_map<std::string, std::vector<std::string>> map {
        { "language", { } },
    };
    return map;
}

const std::unordered_map<std::string, TestHeaderKeyType>& TestOptions::keyTypeMapping()
{
    static std::unordered_map<std::string, TestHeaderKeyType> map {
        { "AcceleratedDrawingEnabled", TestHeaderKeyType::BoolWebPreference },
        { "AllowCrossOriginSubresourcesToAskForCredentials", TestHeaderKeyType::BoolWebPreference },
        { "AllowTopNavigationToDataURLs", TestHeaderKeyType::BoolWebPreference },
        { "AttachmentElementEnabled", TestHeaderKeyType::BoolWebPreference },
        { "CaptureAudioInGPUProcessEnabled", TestHeaderKeyType::BoolWebPreference },
        { "CaptureAudioInUIProcessEnabled", TestHeaderKeyType::BoolWebPreference },
        { "CaptureVideoInGPUProcessEnabled", TestHeaderKeyType::BoolWebPreference },
        { "CaptureVideoInUIProcessEnabled", TestHeaderKeyType::BoolWebPreference },
        { "ColorFilterEnabled", TestHeaderKeyType::BoolWebPreference },
        { "DOMPasteAllowed", TestHeaderKeyType::BoolWebPreference },
        { "InspectorAdditionsEnabled", TestHeaderKeyType::BoolWebPreference },
        { "KeygenElementEnabled", TestHeaderKeyType::BoolWebPreference },
        { "MenuItemElementEnabled", TestHeaderKeyType::BoolWebPreference },
        { "MockScrollbarsEnabled", TestHeaderKeyType::BoolWebPreference },
        { "ModernMediaControlsEnabled", TestHeaderKeyType::BoolWebPreference },
        { "NeedsSiteSpecificQuirks", TestHeaderKeyType::BoolWebPreference },
        { "PageVisibilityBasedProcessSuppressionEnabled", TestHeaderKeyType::BoolWebPreference },
        { "PunchOutWhiteBackgroundsInDarkMode", TestHeaderKeyType::BoolWebPreference },
        { "ServiceControlsEnabled", TestHeaderKeyType::BoolWebPreference },
        { "ShouldIgnoreMetaViewport", TestHeaderKeyType::BoolWebPreference },
        { "ShouldUseServiceWorkerShortTimeout", TestHeaderKeyType::BoolWebPreference },
        { "UsesBackForwardCache", TestHeaderKeyType::BoolWebPreference },
        { "WebAuthenticationEnabled", TestHeaderKeyType::BoolWebPreference },
        { "WebAuthenticationLocalAuthenticatorEnabled", TestHeaderKeyType::BoolWebPreference },

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

TestOptions::TestOptions(TestFeatures features)
    : m_features { features }
{
}

bool TestOptions::hasSameInitializationOptions(const TestOptions& options) const
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

std::vector<std::pair<std::string, bool>> TestOptions::boolWKPreferences() const
{
    std::vector<std::pair<std::string, bool>> result;

    for (auto [key, defaultValue] : boolWebPreferencesDefaultsMap())
        result.push_back({ key, boolWebPreferenceFeatureValue(key) });

    return result;
}

template<typename T> T featureValue(std::string key, const std::unordered_map<std::string, T>& map, const std::unordered_map<std::string, T>& defaultsMap)
{
    auto it = map.find(key);
    if (it != map.end())
        return it->second;
    
    auto defaultsMapIt = defaultsMap.find(key);
    ASSERT(defaultsMapIt != defaultsMap.end());
    return defaultsMapIt->second;
}

bool TestOptions::boolWebPreferenceFeatureValue(std::string key) const
{
    return featureValue(key, m_features.boolWebPreferenceFeatures, boolWebPreferencesDefaultsMap());
}
bool TestOptions::boolTestRunnerFeatureValue(std::string key) const
{
    return featureValue(key, m_features.boolTestRunnerFeatures, boolTestRunnerDefaultsMap());
}
double TestOptions::doubleTestRunnerFeatureValue(std::string key) const
{
    return featureValue(key, m_features.doubleTestRunnerFeatures, doubleTestRunnerDefaultsMap());
}
std::string TestOptions::stringTestRunnerFeatureValue(std::string key) const
{
    return featureValue(key, m_features.stringTestRunnerFeatures, stringTestRunnerDefaultsMap());
}
std::vector<std::string> TestOptions::stringVectorTestRunnerFeatureValue(std::string key) const
{
    return featureValue(key, m_features.stringVectorTestRunnerFeatures, stringVectorTestRunnerDefaultsMap());
}

}
