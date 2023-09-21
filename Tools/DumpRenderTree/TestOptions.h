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

#pragma once

#include "TestFeatures.h"
#include <string>
#include <unordered_map>

namespace WTR {

class TestOptions {
public:
    static const TestFeatures& defaults();
    static const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMapping();

    explicit TestOptions(TestFeatures features)
        : m_features(std::move(features))
    {
    }

    bool webViewIsCompatibleWithOptions(const TestOptions&) const;

    // Test-Runner-Specific Features
    bool dumpJSConsoleLogInStdErr() const { return boolTestRunnerFeatureValue("dumpJSConsoleLogInStdErr", false); }
    bool enableDragDestinationActionLoad() const { return boolTestRunnerFeatureValue("enableDragDestinationActionLoad", false); }
    bool layerBackedWebView() const { return boolTestRunnerFeatureValue("layerBackedWebView", false); }
    bool useEphemeralSession() const { return boolTestRunnerFeatureValue("useEphemeralSession", false); }
    std::string additionalSupportedImageTypes() const { return stringTestRunnerFeatureValue("additionalSupportedImageTypes", { }); }
    std::string jscOptions() const { return stringTestRunnerFeatureValue("jscOptions", { }); }
    std::string captionDisplayMode() const { return stringTestRunnerFeatureValue("captionDisplayMode", { }); }

    const auto& boolWebPreferenceFeatures() const { return m_features.boolWebPreferenceFeatures; }
    const auto& doubleWebPreferenceFeatures() const { return m_features.doubleWebPreferenceFeatures; }
    const auto& uint32WebPreferenceFeatures() const { return m_features.uint32WebPreferenceFeatures; }
    const auto& stringWebPreferenceFeatures() const { return m_features.stringWebPreferenceFeatures; }

    // FIXME: Remove these once there is a viable mechanism for reseting WebPreferences between tests,
    // at which point, we will not need to manually reset every supported preference for each test.
    static const std::vector<std::string>& supportedBoolWebPreferenceFeatures();
    static const std::vector<std::string>& supportedUInt32WebPreferenceFeatures();

    static std::string toWebKitLegacyPreferenceKey(const std::string&);

private:
    bool boolTestRunnerFeatureValue(std::string key, bool defaultValue) const;
    std::string stringTestRunnerFeatureValue(std::string key, std::string defaultValue) const;

    TestFeatures m_features;
};

}

