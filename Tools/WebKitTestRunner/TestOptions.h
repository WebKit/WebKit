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

#include "TestFeatures.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace WTR {

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
    static const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMapping();

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

    bool allowTopNavigationToDataURLs() const { return boolWebPreferenceFeatureValue("AllowTopNavigationToDataURLs"); }
    bool enableAttachmentElement() const { return boolWebPreferenceFeatureValue("AttachmentElementEnabled"); }
    bool punchOutWhiteBackgroundsInDarkMode() const { return boolWebPreferenceFeatureValue("PunchOutWhiteBackgroundsInDarkMode"); }
    bool useServiceWorkerShortTimeout() const { return boolWebPreferenceFeatureValue("ShouldUseServiceWorkerShortTimeout"); }

    bool allowsLinkPreview() const { return boolTestRunnerFeatureValue("allowsLinkPreview"); }
    bool dumpJSConsoleLogInStdErr() const { return boolTestRunnerFeatureValue("dumpJSConsoleLogInStdErr"); }
    bool editable() const { return boolTestRunnerFeatureValue("editable"); }
    bool enableInAppBrowserPrivacy() const { return boolTestRunnerFeatureValue("enableInAppBrowserPrivacy"); }
    bool enableProcessSwapOnNavigation() const { return boolTestRunnerFeatureValue("enableProcessSwapOnNavigation"); }
    bool enableProcessSwapOnWindowOpen() const { return boolTestRunnerFeatureValue("enableProcessSwapOnWindowOpen"); }
    bool ignoreSynchronousMessagingTimeouts() const { return boolTestRunnerFeatureValue("ignoreSynchronousMessagingTimeouts"); }
    bool ignoresViewportScaleLimits() const { return boolTestRunnerFeatureValue("ignoresViewportScaleLimits"); }
    bool isAppBoundWebView() const { return boolTestRunnerFeatureValue("isAppBoundWebView"); }
    bool runSingly() const { return boolTestRunnerFeatureValue("runSingly"); }
    bool shouldHandleRunOpenPanel() const { return boolTestRunnerFeatureValue("shouldHandleRunOpenPanel"); }
    bool shouldPresentPopovers() const { return boolTestRunnerFeatureValue("shouldPresentPopovers"); }
    bool shouldShowSpellCheckingDots() const { return boolTestRunnerFeatureValue("spellCheckingDots"); }
    bool shouldShowTouches() const { return boolTestRunnerFeatureValue("shouldShowTouches"); }
    bool shouldShowWebView() const { return boolTestRunnerFeatureValue("shouldShowWebView"); }
    bool useCharacterSelectionGranularity() const { return boolTestRunnerFeatureValue("useCharacterSelectionGranularity"); }
    bool useDataDetection() const { return boolTestRunnerFeatureValue("useDataDetection"); }
    bool useEphemeralSession() const { return boolTestRunnerFeatureValue("useEphemeralSession"); }
    bool useFlexibleViewport() const { return boolTestRunnerFeatureValue("useFlexibleViewport"); }
    bool useRemoteLayerTree() const { return boolTestRunnerFeatureValue("useRemoteLayerTree"); }
    bool useThreadedScrolling() const { return boolTestRunnerFeatureValue("useThreadedScrolling"); }
    double contentInsetTop() const { return doubleTestRunnerFeatureValue("contentInset.top"); }
    double deviceScaleFactor() const { return doubleTestRunnerFeatureValue("deviceScaleFactor"); }
    double viewHeight() const { return doubleTestRunnerFeatureValue("viewHeight"); }
    double viewWidth() const { return doubleTestRunnerFeatureValue("viewWidth"); }
    std::string additionalSupportedImageTypes() const { return stringTestRunnerFeatureValue("additionalSupportedImageTypes"); }
    std::string applicationBundleIdentifier() const { return stringTestRunnerFeatureValue("applicationBundleIdentifier"); }
    std::string applicationManifest() const { return stringTestRunnerFeatureValue("applicationManifest"); }
    std::string contentMode() const { return stringTestRunnerFeatureValue("contentMode"); }
    std::string jscOptions() const { return stringTestRunnerFeatureValue("jscOptions"); }
    std::string standaloneWebApplicationURL() const { return stringTestRunnerFeatureValue("standaloneWebApplicationURL"); }
    std::vector<std::string> overrideLanguages() const { return stringVectorTestRunnerFeatureValue("language"); }

    const std::unordered_map<std::string, bool>& experimentalFeatures() const { return m_features.experimentalFeatures; }
    const std::unordered_map<std::string, bool>& internalDebugFeatures() const { return m_features.internalDebugFeatures; }

    std::vector<std::pair<std::string, bool>> boolWKPreferences() const;

    bool hasSameInitializationOptions(const TestOptions&) const;

private:
    bool boolWebPreferenceFeatureValue(std::string key) const;
    bool boolTestRunnerFeatureValue(std::string key) const;
    double doubleTestRunnerFeatureValue(std::string key) const;
    std::string stringTestRunnerFeatureValue(std::string key) const;
    std::vector<std::string> stringVectorTestRunnerFeatureValue(std::string key) const;

    TestFeatures m_features;
};

}
