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

    bool allowTopNavigationToDataURLs() const { return boolFeatureValue("allowTopNavigationToDataURLs"); }
    bool allowsLinkPreview() const { return boolFeatureValue("allowsLinkPreview"); }
    bool dumpJSConsoleLogInStdErr() const { return boolFeatureValue("dumpJSConsoleLogInStdErr"); }
    bool editable() const { return boolFeatureValue("editable"); }
    bool enableAttachmentElement() const { return boolFeatureValue("enableAttachmentElement"); }
    bool enableInAppBrowserPrivacy() const { return boolFeatureValue("enableInAppBrowserPrivacy"); }
    bool enableProcessSwapOnNavigation() const { return boolFeatureValue("enableProcessSwapOnNavigation"); }
    bool enableProcessSwapOnWindowOpen() const { return boolFeatureValue("enableProcessSwapOnWindowOpen"); }
    bool ignoreSynchronousMessagingTimeouts() const { return boolFeatureValue("ignoreSynchronousMessagingTimeouts"); }
    bool ignoresViewportScaleLimits() const { return boolFeatureValue("ignoresViewportScaleLimits"); }
    bool isAppBoundWebView() const { return boolFeatureValue("isAppBoundWebView"); }
    bool punchOutWhiteBackgroundsInDarkMode() const { return boolFeatureValue("punchOutWhiteBackgroundsInDarkMode"); }
    bool runSingly() const { return boolFeatureValue("runSingly"); }
    bool shouldHandleRunOpenPanel() const { return boolFeatureValue("shouldHandleRunOpenPanel"); }
    bool shouldPresentPopovers() const { return boolFeatureValue("shouldPresentPopovers"); }
    bool shouldShowSpellCheckingDots() const { return boolFeatureValue("spellCheckingDots"); }
    bool shouldShowTouches() const { return boolFeatureValue("shouldShowTouches"); }
    bool shouldShowWebView() const { return boolFeatureValue("shouldShowWebView"); }
    bool useCharacterSelectionGranularity() const { return boolFeatureValue("useCharacterSelectionGranularity"); }
    bool useDataDetection() const { return boolFeatureValue("useDataDetection"); }
    bool useEphemeralSession() const { return boolFeatureValue("useEphemeralSession"); }
    bool useFlexibleViewport() const { return boolFeatureValue("useFlexibleViewport"); }
    bool useRemoteLayerTree() const { return boolFeatureValue("useRemoteLayerTree"); }
    bool useServiceWorkerShortTimeout() const { return boolFeatureValue("useServiceWorkerShortTimeout"); }
    bool useThreadedScrolling() const { return boolFeatureValue("useThreadedScrolling"); }
    double contentInsetTop() const { return doubleFeatureValue("contentInset.top"); }
    double deviceScaleFactor() const { return doubleFeatureValue("deviceScaleFactor"); }
    double viewHeight() const { return doubleFeatureValue("viewHeight"); }
    double viewWidth() const { return doubleFeatureValue("viewWidth"); }
    std::string additionalSupportedImageTypes() const { return stringFeatureValue("additionalSupportedImageTypes"); }
    std::string applicationBundleIdentifier() const { return stringFeatureValue("applicationBundleIdentifier"); }
    std::string applicationManifest() const { return stringFeatureValue("applicationManifest"); }
    std::string contentMode() const { return stringFeatureValue("contentMode"); }
    std::string jscOptions() const { return stringFeatureValue("jscOptions"); }
    std::string standaloneWebApplicationURL() const { return stringFeatureValue("standaloneWebApplicationURL"); }
    std::vector<std::string> overrideLanguages() const { return stringVectorFeatureValue("language"); }

    const std::unordered_map<std::string, bool>& experimentalFeatures() const { return m_features.experimentalFeatures; }
    const std::unordered_map<std::string, bool>& internalDebugFeatures() const { return m_features.internalDebugFeatures; }

    std::vector<std::pair<std::string, bool>> boolWKPreferences() const;

    bool hasSameInitializationOptions(const TestOptions&) const;

private:
    bool boolFeatureValue(std::string key) const;
    double doubleFeatureValue(std::string key) const;
    std::string stringFeatureValue(std::string key) const;
    std::vector<std::string> stringVectorFeatureValue(std::string key) const;

    TestFeatures m_features;
};

}
