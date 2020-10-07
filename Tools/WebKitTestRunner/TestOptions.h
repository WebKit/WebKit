/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace WTR {

struct TestCommand;

struct TestFeatures {
    std::unordered_map<std::string, bool> experimentalFeatures;
    std::unordered_map<std::string, bool> internalDebugFeatures;
    std::unordered_map<std::string, bool> boolFeatures;
    std::unordered_map<std::string, double> doubleFeatures;
    std::unordered_map<std::string, std::string> stringFeatures;
    std::unordered_map<std::string, std::vector<std::string>> stringVectorFeatures;
};

void merge(TestFeatures& base, TestFeatures additional);

TestFeatures hardcodedFeaturesBasedOnPathForTest(const TestCommand&);
TestFeatures featureDefaultsFromTestHeaderForTest(const TestCommand&);

struct ContextOptions {
    std::vector<std::string> overrideLanguages;
    bool ignoreSynchronousMessagingTimeouts;
    bool enableProcessSwapOnNavigation;
    bool enableProcessSwapOnWindowOpen;
    bool useServiceWorkerShortTimeout;

    bool hasSameInitializationOptions(const ContextOptions& options) const
    {
        if (ignoreSynchronousMessagingTimeouts != options.ignoreSynchronousMessagingTimeouts
            || overrideLanguages != options.overrideLanguages
            || enableProcessSwapOnNavigation != options.enableProcessSwapOnNavigation
            || enableProcessSwapOnWindowOpen != options.enableProcessSwapOnWindowOpen
            || useServiceWorkerShortTimeout != options.useServiceWorkerShortTimeout)
            return false;
        return true;
    }

    bool shouldEnableProcessSwapOnNavigation() const
    {
        return enableProcessSwapOnNavigation || enableProcessSwapOnWindowOpen;
    }
};

class TestOptions {
public:
    explicit TestOptions(TestFeatures);

    ContextOptions contextOptions() const
    {
        return {
            overrideLanguages(),
            ignoreSynchronousMessagingTimeouts(),
            enableProcessSwapOnNavigation(),
            enableProcessSwapOnWindowOpen(),
            useServiceWorkerShortTimeout()
        };
    }

    bool useThreadedScrolling() const { return boolFeatureValue("useThreadedScrolling", false); }
    bool useAcceleratedDrawing() const { return boolFeatureValue("useAcceleratedDrawing", false); }
    bool useRemoteLayerTree() const { return boolFeatureValue("useRemoteLayerTree", false); }
    bool shouldShowWebView() const { return boolFeatureValue("shouldShowWebView", false); }
    bool useFlexibleViewport() const { return boolFeatureValue("useFlexibleViewport", false); }
    bool useDataDetection() const { return boolFeatureValue("useDataDetection", false); }
    bool useMockScrollbars() const { return boolFeatureValue("useMockScrollbars", true); }
    bool needsSiteSpecificQuirks() const { return boolFeatureValue("needsSiteSpecificQuirks", false); }
    bool ignoresViewportScaleLimits() const { return boolFeatureValue("ignoresViewportScaleLimits", false); }
    bool useCharacterSelectionGranularity() const { return boolFeatureValue("useCharacterSelectionGranularity", false); }
    bool enableAttachmentElement() const { return boolFeatureValue("enableAttachmentElement", false); }
    bool enableIntersectionObserver() const { return boolFeatureValue("enableIntersectionObserver", false); }
    bool useEphemeralSession() const { return boolFeatureValue("useEphemeralSession", false); }
    bool enableMenuItemElement() const { return boolFeatureValue("enableMenuItemElement", false); }
    bool enableKeygenElement() const { return boolFeatureValue("enableKeygenElement", false); }
    bool enableModernMediaControls() const { return boolFeatureValue("enableModernMediaControls", true); }
    bool enablePointerLock() const { return boolFeatureValue("enablePointerLock", false); }
    bool enableWebAuthentication() const { return boolFeatureValue("enableWebAuthentication", true); }
    bool enableWebAuthenticationLocalAuthenticator() const { return boolFeatureValue("enableWebAuthenticationLocalAuthenticator", true); }
    bool enableInspectorAdditions() const { return boolFeatureValue("enableInspectorAdditions", false); }
    bool shouldShowTouches() const { return boolFeatureValue("shouldShowTouches", false); }
    bool dumpJSConsoleLogInStdErr() const { return boolFeatureValue("dumpJSConsoleLogInStdErr", false); }
    bool allowCrossOriginSubresourcesToAskForCredentials() const { return boolFeatureValue("allowCrossOriginSubresourcesToAskForCredentials", false); }
    bool domPasteAllowed() const { return boolFeatureValue("domPasteAllowed", true); }
    bool enableColorFilter() const { return boolFeatureValue("enableColorFilter", false); }
    bool punchOutWhiteBackgroundsInDarkMode() const { return boolFeatureValue("punchOutWhiteBackgroundsInDarkMode", false); }
    bool runSingly() const { return boolFeatureValue("runSingly", false); }
    bool checkForWorldLeaks() const { return boolFeatureValue("checkForWorldLeaks", false); }
    bool shouldIgnoreMetaViewport() const { return boolFeatureValue("shouldIgnoreMetaViewport", false); }
    bool shouldShowSpellCheckingDots() const { return boolFeatureValue("spellCheckingDots", false); }
    bool enableServiceControls() const { return boolFeatureValue("enableServiceControls", false); }
    bool editable() const { return boolFeatureValue("editable", false); }
    bool shouldHandleRunOpenPanel() const { return boolFeatureValue("shouldHandleRunOpenPanel", true); }
    bool shouldPresentPopovers() const { return boolFeatureValue("shouldPresentPopovers", true); }
    bool enableAppNap() const { return boolFeatureValue("enableAppNap", false); }
    bool enableBackForwardCache() const { return boolFeatureValue("enableBackForwardCache", false); }
    bool allowsLinkPreview() const { return boolFeatureValue("allowsLinkPreview", true); }
    bool enableCaptureVideoInUIProcess() const { return boolFeatureValue("enableCaptureVideoInUIProcess", false); }
    bool enableCaptureVideoInGPUProcess() const { return boolFeatureValue("enableCaptureVideoInGPUProcess", false); }
    bool enableCaptureAudioInUIProcess() const { return boolFeatureValue("enableCaptureAudioInUIProcess", false); }
    bool enableCaptureAudioInGPUProcess() const { return boolFeatureValue("enableCaptureAudioInGPUProcess", false); }
    bool allowTopNavigationToDataURLs() const { return boolFeatureValue("allowTopNavigationToDataURLs", true); }
    bool enableInAppBrowserPrivacy() const { return boolFeatureValue("enableInAppBrowserPrivacy", false); }
    bool isAppBoundWebView() const { return boolFeatureValue("isAppBoundWebView", false); }
    bool ignoreSynchronousMessagingTimeouts() const { return boolFeatureValue("ignoreSynchronousMessagingTimeouts", false); }
    bool enableProcessSwapOnNavigation() const { return boolFeatureValue("enableProcessSwapOnNavigation", true); }
    bool enableProcessSwapOnWindowOpen() const { return boolFeatureValue("enableProcessSwapOnWindowOpen", false); }
    bool useServiceWorkerShortTimeout() const { return boolFeatureValue("useServiceWorkerShortTimeout", false); }
    double contentInsetTop() const { return doubleFeatureValue("contentInset.top", 0); }
    double deviceScaleFactor() const { return doubleFeatureValue("deviceScaleFactor", 1); }
    double viewWidth() const { return doubleFeatureValue("viewWidth", 800); }
    double viewHeight() const { return doubleFeatureValue("viewHeight", 600); }
    std::string applicationManifest() const { return stringFeatureValue("applicationManifest", { }); }
    std::string jscOptions() const { return stringFeatureValue("jscOptions", { }); }
    std::string additionalSupportedImageTypes() const { return stringFeatureValue("additionalSupportedImageTypes", { }); }
    std::string standaloneWebApplicationURL() const { return stringFeatureValue("standaloneWebApplicationURL", { }); }
    std::string contentMode() const { return stringFeatureValue("contentMode", { }); }
    std::string applicationBundleIdentifier() const { return stringFeatureValue("applicationBundleIdentifier", { }); }
    std::vector<std::string> overrideLanguages() const { return stringVectorFeatureValue("language", { }); }

    const std::unordered_map<std::string, bool>& experimentalFeatures() const { return m_features.experimentalFeatures; }
    const std::unordered_map<std::string, bool>& internalDebugFeatures() const { return m_features.internalDebugFeatures; }

    bool hasSameInitializationOptions(const TestOptions& options) const
    {
        if (m_features.experimentalFeatures != options.m_features.experimentalFeatures)
            return false;
        if (m_features.internalDebugFeatures != options.m_features.internalDebugFeatures)
            return false;
        if (m_features.boolFeatures != options.m_features.boolFeatures)
            return false;
        if (m_features.doubleFeatures != options.m_features.doubleFeatures)
            return false;
        if (m_features.stringFeatures != options.m_features.stringFeatures)
            return false;
        if (m_features.stringVectorFeatures != options.m_features.stringVectorFeatures)
            return false;
        return true;
    }

private:
    template<typename T>
    T featureValue(std::string key, T defaultValue, const std::unordered_map<std::string, T>& map) const
    {
        auto it = map.find(key);
        if (it != map.end())
            return it->second;
        return defaultValue;
    }

    bool boolFeatureValue(std::string key, bool defaultValue) const
    {
        return featureValue(key, defaultValue, m_features.boolFeatures);
    }
    double doubleFeatureValue(std::string key, double defaultValue) const
    {
        return featureValue(key, defaultValue, m_features.doubleFeatures);
    }
    std::string stringFeatureValue(std::string key, std::string defaultValue) const
    {
        return featureValue(key, defaultValue, m_features.stringFeatures);
    }
    std::vector<std::string> stringVectorFeatureValue(std::string key, std::vector<std::string> defaultValue) const
    {
        return featureValue(key, defaultValue, m_features.stringVectorFeatures);
    }

    TestFeatures m_features;
};

}
