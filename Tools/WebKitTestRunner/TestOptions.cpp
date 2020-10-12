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

static const std::unordered_map<std::string, bool>& boolDefaultsMap()
{
    static std::unordered_map<std::string, bool> map {
        { "allowCrossOriginSubresourcesToAskForCredentials", false },
        { "allowTopNavigationToDataURLs", true },
        { "allowsLinkPreview", true },
        { "domPasteAllowed", true },
        { "dumpJSConsoleLogInStdErr", false },
        { "editable", false },
        { "enableAppNap", false },
        { "enableAttachmentElement", false },
        { "enableBackForwardCache", false },
        { "enableCaptureAudioInGPUProcess", false },
        { "enableCaptureAudioInUIProcess", false },
        { "enableCaptureVideoInGPUProcess", false },
        { "enableCaptureVideoInUIProcess", false },
        { "enableColorFilter", false },
        { "enableInAppBrowserPrivacy", false },
        { "enableInspectorAdditions", false },
        { "enableKeygenElement", false },
        { "enableMenuItemElement", false },
        { "enableModernMediaControls", true },
        { "enableProcessSwapOnNavigation", true },
        { "enableProcessSwapOnWindowOpen", false },
        { "enableServiceControls", false },
        { "enableWebAuthentication", true },
        { "enableWebAuthenticationLocalAuthenticator", true },
        { "ignoreSynchronousMessagingTimeouts", false },
        { "ignoresViewportScaleLimits", false },
        { "isAppBoundWebView", false },
        { "needsSiteSpecificQuirks", false },
        { "punchOutWhiteBackgroundsInDarkMode", false },
        { "runSingly", false },
        { "shouldHandleRunOpenPanel", true },
        { "shouldIgnoreMetaViewport", false },
        { "shouldPresentPopovers", true },
        { "shouldShowTouches", false },
        { "shouldShowWebView", false },
        { "spellCheckingDots", false },
        { "useAcceleratedDrawing", false },
        { "useCharacterSelectionGranularity", false },
        { "useDataDetection", false },
        { "useEphemeralSession", false },
        { "useFlexibleViewport", false },
        { "useMockScrollbars", true },
        { "useRemoteLayerTree", false },
        { "useServiceWorkerShortTimeout", false },
        { "useThreadedScrolling", false },
    };
    return map;
}

static const std::unordered_map<std::string, double>& doubleDefaultsMap()
{
    static std::unordered_map<std::string, double> map {
        { "contentInset.top", 0 },
        { "deviceScaleFactor", 1 },
        { "viewHeight", 600 },
        { "viewWidth", 800 },
    };
    return map;
}

static const std::unordered_map<std::string, std::string>& stringDefaultsMap()
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

static const std::unordered_map<std::string, std::vector<std::string>>& stringVectorDefaultsMap()
{
    static std::unordered_map<std::string, std::vector<std::string>> map {
        { "language", { } },
    };
    return map;
}

const std::unordered_map<std::string, TestHeaderKeyType>& TestOptions::keyTypeMapping()
{
    static std::unordered_map<std::string, TestHeaderKeyType> map {
        { "allowCrossOriginSubresourcesToAskForCredentials", TestHeaderKeyType::Bool },
        { "allowTopNavigationToDataURLs", TestHeaderKeyType::Bool },
        { "allowsLinkPreview", TestHeaderKeyType::Bool },
        { "domPasteAllowed", TestHeaderKeyType::Bool },
        { "dumpJSConsoleLogInStdErr", TestHeaderKeyType::Bool },
        { "editable", TestHeaderKeyType::Bool },
        { "enableAppNap", TestHeaderKeyType::Bool },
        { "enableAttachmentElement", TestHeaderKeyType::Bool },
        { "enableBackForwardCache", TestHeaderKeyType::Bool },
        { "enableCaptureAudioInGPUProcess", TestHeaderKeyType::Bool },
        { "enableCaptureAudioInUIProcess", TestHeaderKeyType::Bool },
        { "enableCaptureVideoInGPUProcess", TestHeaderKeyType::Bool },
        { "enableCaptureVideoInUIProcess", TestHeaderKeyType::Bool },
        { "enableColorFilter", TestHeaderKeyType::Bool },
        { "enableInAppBrowserPrivacy", TestHeaderKeyType::Bool },
        { "enableInspectorAdditions", TestHeaderKeyType::Bool },
        { "enableKeygenElement", TestHeaderKeyType::Bool },
        { "enableMenuItemElement", TestHeaderKeyType::Bool },
        { "enableModernMediaControls", TestHeaderKeyType::Bool },
        { "enableProcessSwapOnNavigation", TestHeaderKeyType::Bool },
        { "enableProcessSwapOnWindowOpen", TestHeaderKeyType::Bool },
        { "enableServiceControls", TestHeaderKeyType::Bool },
        { "enableWebAuthentication", TestHeaderKeyType::Bool },
        { "enableWebAuthenticationLocalAuthenticator", TestHeaderKeyType::Bool },
        { "ignoreSynchronousMessagingTimeouts", TestHeaderKeyType::Bool },
        { "ignoresViewportScaleLimits", TestHeaderKeyType::Bool },
        { "isAppBoundWebView", TestHeaderKeyType::Bool },
        { "needsSiteSpecificQuirks", TestHeaderKeyType::Bool },
        { "punchOutWhiteBackgroundsInDarkMode", TestHeaderKeyType::Bool },
        { "runSingly", TestHeaderKeyType::Bool },
        { "shouldHandleRunOpenPanel", TestHeaderKeyType::Bool },
        { "shouldIgnoreMetaViewport", TestHeaderKeyType::Bool },
        { "shouldPresentPopovers", TestHeaderKeyType::Bool },
        { "shouldShowTouches", TestHeaderKeyType::Bool },
        { "shouldShowWebView", TestHeaderKeyType::Bool },
        { "spellCheckingDots", TestHeaderKeyType::Bool },
        { "useAcceleratedDrawing", TestHeaderKeyType::Bool },
        { "useCharacterSelectionGranularity", TestHeaderKeyType::Bool },
        { "useDataDetection", TestHeaderKeyType::Bool },
        { "useEphemeralSession", TestHeaderKeyType::Bool },
        { "useFlexibleViewport", TestHeaderKeyType::Bool },
        { "useMockScrollbars", TestHeaderKeyType::Bool },
        { "useRemoteLayerTree", TestHeaderKeyType::Bool },
        { "useServiceWorkerShortTimeout", TestHeaderKeyType::Bool },
        { "useThreadedScrolling", TestHeaderKeyType::Bool },
    
        { "contentInset.top", TestHeaderKeyType::Double },
        { "deviceScaleFactor", TestHeaderKeyType::Double },
        { "viewHeight", TestHeaderKeyType::Double },
        { "viewWidth", TestHeaderKeyType::Double },

        { "additionalSupportedImageTypes", TestHeaderKeyType::String },
        { "applicationBundleIdentifier", TestHeaderKeyType::String },
        { "applicationManifest", TestHeaderKeyType::StringRelativePath },
        { "contentMode", TestHeaderKeyType::String },
        { "jscOptions", TestHeaderKeyType::String },
        { "standaloneWebApplicationURL", TestHeaderKeyType::StringURL },

        { "language", TestHeaderKeyType::StringVector },
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

    for (auto [key, keyType] : keyTypeMapping()) {
        switch (keyType) {
        case TestHeaderKeyType::Bool:
            if (boolFeatureValue(key) != options.boolFeatureValue(key))
                return false;
            break;
        case TestHeaderKeyType::Double:
            if (doubleFeatureValue(key) != options.doubleFeatureValue(key))
                return false;
            break;
        case TestHeaderKeyType::String:
        case TestHeaderKeyType::StringRelativePath:
        case TestHeaderKeyType::StringURL:
            if (stringFeatureValue(key) != options.stringFeatureValue(key))
                return false;
            break;
        case TestHeaderKeyType::StringVector:
            if (stringVectorFeatureValue(key) != options.stringVectorFeatureValue(key))
                return false;
            break;
        case TestHeaderKeyType::Unknown:
            ASSERT_NOT_REACHED();
        }
    }
    return true;
}

#define FOR_EACH_BOOL_WK_PREFERENCE(macro) \
    macro(enableCaptureVideoInUIProcess, CaptureVideoInUIProcessEnabled) \
    macro(enableCaptureVideoInGPUProcess, CaptureVideoInGPUProcessEnabled) \
    macro(enableCaptureAudioInUIProcess, CaptureAudioInUIProcessEnabled) \
    macro(enableCaptureAudioInGPUProcess, CaptureAudioInGPUProcessEnabled) \
    macro(useAcceleratedDrawing, AcceleratedDrawingEnabled) \
    macro(useMockScrollbars, MockScrollbarsEnabled) \
    macro(needsSiteSpecificQuirks, NeedsSiteSpecificQuirks) \
    macro(enableAttachmentElement, AttachmentElementEnabled) \
    macro(enableMenuItemElement, MenuItemElementEnabled) \
    macro(enableKeygenElement, KeygenElementEnabled) \
    macro(enableModernMediaControls, ModernMediaControlsEnabled) \
    macro(enableWebAuthentication, WebAuthenticationEnabled) \
    macro(enableWebAuthenticationLocalAuthenticator, WebAuthenticationLocalAuthenticatorEnabled) \
    macro(enableInspectorAdditions, InspectorAdditionsEnabled) \
    macro(allowCrossOriginSubresourcesToAskForCredentials, AllowCrossOriginSubresourcesToAskForCredentials) \
    macro(domPasteAllowed, DOMPasteAllowed) \
    macro(enableColorFilter, ColorFilterEnabled) \
    macro(punchOutWhiteBackgroundsInDarkMode, PunchOutWhiteBackgroundsInDarkMode) \
    macro(shouldIgnoreMetaViewport, ShouldIgnoreMetaViewport) \
    macro(enableAppNap, PageVisibilityBasedProcessSuppressionEnabled) \
    macro(enableBackForwardCache, UsesBackForwardCache) \
    macro(enableServiceControls, ServiceControlsEnabled) \
    macro(allowTopNavigationToDataURLs, AllowTopNavigationToDataURLs) \
    macro(useServiceWorkerShortTimeout, ShouldUseServiceWorkerShortTimeout) \
\

std::vector<std::pair<std::string, bool>> TestOptions::boolWKPreferences() const
{
    return {
#define ADD_VALUE(testOptionsKey, preferencesKey) { #preferencesKey, boolFeatureValue(#testOptionsKey) },

FOR_EACH_BOOL_WK_PREFERENCE(ADD_VALUE)

#undef ADD_VALUE
    };
}

template<typename T>
T featureValue(std::string key, const std::unordered_map<std::string, T>& map, const std::unordered_map<std::string, T>& defaultsMap)
{
    auto it = map.find(key);
    if (it != map.end())
        return it->second;
    
    auto defaultsMapIt = defaultsMap.find(key);
    ASSERT(defaultsMapIt != defaultsMap.end());
    return defaultsMapIt->second;
}

bool TestOptions::boolFeatureValue(std::string key) const
{
    return featureValue(key, m_features.boolFeatures, boolDefaultsMap());
}
double TestOptions::doubleFeatureValue(std::string key) const
{
    return featureValue(key, m_features.doubleFeatures, doubleDefaultsMap());
}
std::string TestOptions::stringFeatureValue(std::string key) const
{
    return featureValue(key, m_features.stringFeatures, stringDefaultsMap());
}
std::vector<std::string> TestOptions::stringVectorFeatureValue(std::string key) const
{
    return featureValue(key, m_features.stringVectorFeatures, stringVectorDefaultsMap());
}

}
