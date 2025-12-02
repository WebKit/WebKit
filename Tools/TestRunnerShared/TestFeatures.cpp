/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "TestFeatures.h"

#include "TestCommand.h"
#include "WPTFunctions.h"
#include <fstream>
#include <string>
#include <wtf/StdFilesystem.h>
#include <wtf/URL.h>

namespace WTR {

template<typename T> void merge(std::unordered_map<std::string, T>& base, const std::unordered_map<std::string, T>& additional)
{
    for (auto [key, value] : additional)
        base.insert_or_assign(key, value);
}

void merge(TestFeatures& base, TestFeatures additional)
{
    // FIXME: This should use std::unordered_map::merge when it is available for all ports.
    merge(base.boolWebPreferenceFeatures, additional.boolWebPreferenceFeatures);
    merge(base.doubleWebPreferenceFeatures, additional.doubleWebPreferenceFeatures);
    merge(base.uint32WebPreferenceFeatures, additional.uint32WebPreferenceFeatures);
    merge(base.stringWebPreferenceFeatures, additional.stringWebPreferenceFeatures);
    merge(base.boolTestRunnerFeatures, additional.boolTestRunnerFeatures);
    merge(base.doubleTestRunnerFeatures, additional.doubleTestRunnerFeatures);
    merge(base.uint16TestRunnerFeatures, additional.uint16TestRunnerFeatures);
    merge(base.stringTestRunnerFeatures, additional.stringTestRunnerFeatures);
    merge(base.stringVectorTestRunnerFeatures, additional.stringVectorTestRunnerFeatures);
}

bool operator==(const TestFeatures& a, const TestFeatures& b)
{
    if (a.boolWebPreferenceFeatures != b.boolWebPreferenceFeatures)
        return false;
    if (a.doubleWebPreferenceFeatures != b.doubleWebPreferenceFeatures)
        return false;
    if (a.uint32WebPreferenceFeatures != b.uint32WebPreferenceFeatures)
        return false;
    if (a.stringWebPreferenceFeatures != b.stringWebPreferenceFeatures)
        return false;
    if (a.boolTestRunnerFeatures != b.boolTestRunnerFeatures)
        return false;
    if (a.doubleTestRunnerFeatures != b.doubleTestRunnerFeatures)
        return false;
    if (a.uint16TestRunnerFeatures != b.uint16TestRunnerFeatures)
        return false;
    if (a.stringTestRunnerFeatures != b.stringTestRunnerFeatures)
        return false;
    if (a.stringVectorTestRunnerFeatures != b.stringVectorTestRunnerFeatures)
        return false;
    return true;
}

static bool pathContains(const std::string& pathOrURL, const char* substring)
{
    return pathOrURL.find(substring) != std::string::npos;
}

static bool shouldMakeViewportFlexible(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "viewport/") && !pathContains(pathOrURL, "visual-viewport/");
}

static bool shouldUseEphemeralSession(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "w3c/IndexedDB-private-browsing") || pathContains(pathOrURL, "w3c\\IndexedDB-private-browsing");
}

static std::optional<std::pair<double, double>> overrideViewWidthAndHeightForTest(const std::string& pathOrURL)
{
    if (pathContains(pathOrURL, "svg/W3C-SVG-1.1") || pathContains(pathOrURL, "svg\\W3C-SVG-1.1"))
        return { { 480, 360 } };
    return std::nullopt;
}

static std::optional<double> overrideDeviceScaleFactorForTest(const std::string& pathOrURL)
{
    if (pathContains(pathOrURL, "/hidpi-3x-"))
        return 3;
    if (pathContains(pathOrURL, "/hidpi-"))
        return 2;
    return std::nullopt;
}

static bool shouldDumpJSConsoleLogInStdErr(const std::string& pathOrURL)
{
    // Disable console logging for WPT tests as it frequently causes flakiness.
    if (auto url = URL { { }, String::fromUTF8(pathOrURL.c_str()) }; isWebPlatformTestURL(url))
        return true;

    return false;
}

static bool shouldSetDefaultPortsForWTR(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "localhost:8000/") || pathContains(pathOrURL, "localhost:8443/")
        || pathContains(pathOrURL, "127.0.0.1:8000/") || pathContains(pathOrURL, "127.0.0.1:8443/");
}

static bool shouldSetDefaultPortsForWPT(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "localhost:8800/") || pathContains(pathOrURL, "localhost:9443/")
        || pathContains(pathOrURL, "127.0.0.1:8800/") || pathContains(pathOrURL, "127.0.0.1:9443/");
}

static bool shouldEnableLockdownMode(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "lockdown-mode/");
}

static bool shouldEnableEnhancedSecurity(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "enhanced-security/");
}

static bool shouldUseBackForwardCache(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "navigation-api/");
}

static bool shouldEnableBlocksInInline(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "web-platform.test:8800/");
}

TestFeatures hardcodedFeaturesBasedOnPathForTest(const TestCommand& command)
{
    TestFeatures features;

    if (shouldMakeViewportFlexible(command.pathOrURL))
        features.boolTestRunnerFeatures.insert({ "useFlexibleViewport", true });
    if (shouldUseEphemeralSession(command.pathOrURL))
        features.boolTestRunnerFeatures.insert({ "useEphemeralSession", true });
    if (shouldDumpJSConsoleLogInStdErr(command.pathOrURL))
        features.boolTestRunnerFeatures.insert({ "dumpJSConsoleLogInStdErr", true });
    if (auto deviceScaleFactor = overrideDeviceScaleFactorForTest(command.pathOrURL); deviceScaleFactor != std::nullopt)
        features.doubleTestRunnerFeatures.insert({ "deviceScaleFactor", deviceScaleFactor.value() });
    if (auto viewWidthAndHeight = overrideViewWidthAndHeightForTest(command.pathOrURL); viewWidthAndHeight != std::nullopt) {
        features.doubleTestRunnerFeatures.insert({ "viewWidth", viewWidthAndHeight->first });
        features.doubleTestRunnerFeatures.insert({ "viewHeight", viewWidthAndHeight->second });
    }
    if (shouldSetDefaultPortsForWTR(command.pathOrURL)) {
        features.uint16TestRunnerFeatures.insert({ "insecureUpgradePort", 8000 });
        features.uint16TestRunnerFeatures.insert({ "secureUpgradePort", 8443 });
    } else if (shouldSetDefaultPortsForWPT(command.pathOrURL)) {
        features.uint16TestRunnerFeatures.insert({ "insecureUpgradePort", 8800 });
        features.uint16TestRunnerFeatures.insert({ "secureUpgradePort", 9443 });
    }
    if (shouldEnableLockdownMode(command.pathOrURL))
        features.boolWebPreferenceFeatures.insert({ "LockdownModeEnabled", true });
    if (shouldEnableEnhancedSecurity(command.pathOrURL))
        features.boolTestRunnerFeatures.insert({ "enhancedSecurityEnabled", true });
    if (shouldUseBackForwardCache(command.pathOrURL))
        features.boolWebPreferenceFeatures.insert({ "UsesBackForwardCache", true });
    // FIXME: Temporary. Enable WPT tests first as they don't use render tree dumps and so don't need rebaselining.
    // https://bugs.webkit.org/show_bug.cgi?id=303236
    if (shouldEnableBlocksInInline(command.pathOrURL))
        features.boolWebPreferenceFeatures.insert({ "BlocksInInlineLayoutEnabled", true });

    return features;
}

static bool parseBooleanTestHeaderValue(const std::string& value)
{
    if (value == "true")
        return true;
    if (value == "false")
        return false;

    LOG_ERROR("Found unexpected value '%s' for boolean option. Expected 'true' or 'false'.", value.c_str());
    return false;
}

static double parseDoubleTestHeaderValue(const std::string& value)
{
    return std::stod(value);
}

static uint32_t parseUInt32TestHeaderValue(const std::string& value)
{
    return std::stoi(value);
}

static uint16_t parseUInt16TestHeaderValue(const std::string& value)
{
    return static_cast<uint16_t>(std::stoul(value));
}

static std::string parseStringTestHeaderValueAsRelativePath(const std::string& value, const std::filesystem::path& testPath)
{
    auto basePath = testPath.parent_path();
    return (basePath / value).generic_string();
}

static std::string parseStringTestHeaderValueAsURL(const std::string& value)
{
    return testURLString(value);
}

static std::vector<std::string> parseStringTestHeaderValueAsStringVector(const std::string& string)
{
    std::vector<std::string> result;

    size_t i = 0;
    while (i < string.size()) {
        auto foundIndex = string.find_first_of(',', i);

        if (foundIndex != i)
            result.push_back(string.substr(i, foundIndex - i));

        if (foundIndex == std::string::npos)
            break;

        i = foundIndex + 1;
    }

    return result;
}

static bool parseTestHeaderFeature(TestFeatures& features, std::string key, std::string value, std::filesystem::path path, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    auto keyType = [&keyTypeMap](auto& key) {
        auto it = keyTypeMap.find(key);
        if (it == keyTypeMap.end())
            return TestHeaderKeyType::Unknown;
        return it->second;
    };

    switch (keyType(key)) {
    case TestHeaderKeyType::BoolWebPreference:
        features.boolWebPreferenceFeatures.insert_or_assign(key, parseBooleanTestHeaderValue(value));
        return true;
    case TestHeaderKeyType::DoubleWebPreference:
        features.doubleWebPreferenceFeatures.insert_or_assign(key, parseDoubleTestHeaderValue(value));
        return true;
    case TestHeaderKeyType::UInt32WebPreference:
        features.uint32WebPreferenceFeatures.insert_or_assign(key, parseUInt32TestHeaderValue(value));
        return true;
    case TestHeaderKeyType::StringWebPreference:
        features.stringWebPreferenceFeatures.insert_or_assign(key, value);
        return true;

    case TestHeaderKeyType::BoolTestRunner:
        features.boolTestRunnerFeatures.insert_or_assign(key, parseBooleanTestHeaderValue(value));
        return true;
    case TestHeaderKeyType::DoubleTestRunner:
        features.doubleTestRunnerFeatures.insert_or_assign(key, parseDoubleTestHeaderValue(value));
        return true;
    case TestHeaderKeyType::UInt16TestRunner:
        features.uint16TestRunnerFeatures.insert_or_assign(key, parseUInt16TestHeaderValue(value));
        return true;
    case TestHeaderKeyType::StringTestRunner:
        features.stringTestRunnerFeatures.insert_or_assign(key, value);
        return true;
    case TestHeaderKeyType::StringRelativePathTestRunner:
        features.stringTestRunnerFeatures.insert_or_assign(key, parseStringTestHeaderValueAsRelativePath(value, path));
        return true;
    case TestHeaderKeyType::StringURLTestRunner:
        features.stringTestRunnerFeatures.insert_or_assign(key, parseStringTestHeaderValueAsURL(value));
        return true;
    case TestHeaderKeyType::StringVectorTestRunner:
        features.stringVectorTestRunnerFeatures.insert_or_assign(key, parseStringTestHeaderValueAsStringVector(value));
        return true;

    case TestHeaderKeyType::Unknown:
        return false;
    }

    return false;
}

static std::optional<TestFeatures> parseTestHeaderString(const std::string& pairString, std::filesystem::path path, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    TestFeatures features;

    size_t pairStart = 0;
    while (pairStart < pairString.size()) {
        size_t pairEnd = pairString.find(" ", pairStart);
        if (pairEnd == std::string::npos)
            pairEnd = pairString.size();
        size_t equalsLocation = pairString.find("=", pairStart);
        if (equalsLocation == std::string::npos) {
            if (!path.empty())
                LOG_ERROR("Malformed option in test header (could not find '=' character) in %s", path.c_str());
            else
                LOG_ERROR("Malformed option in --additional-header option value (could not find '=' character)");
            break;
        }
        auto key = pairString.substr(pairStart, equalsLocation - pairStart);
        auto value = pairString.substr(equalsLocation + 1, pairEnd - (equalsLocation + 1));

        if (!parseTestHeaderFeature(features, key, value, path, keyTypeMap)) {
            if (!path.empty())
                LOG_ERROR("Unknown key, '%s', in test header in %s", key.c_str(), path.c_str());
            else
                LOG_ERROR("Unknown key, '%s', in --additional-header option value", key.c_str());
        }
        pairStart = pairEnd + 1;
    }

    return features;
}

static TestFeatures parseTestHeader(std::filesystem::path path, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return { };

    std::ifstream file(path);
    if (!file.good()) {
        LOG_ERROR("Could not open file to inspect test headers in %s", path.c_str());
        return { };
    }

    std::string options;
    getline(file, options);
    std::string beginString("webkit-test-runner [ ");
    std::string endString(" ]");
    size_t beginLocation = options.find(beginString);
    if (beginLocation == std::string::npos)
        return { };
    size_t endLocation = options.find(endString, beginLocation);
    if (endLocation == std::string::npos) {
        LOG_ERROR("Could not find end of test header in %s", path.c_str());
        return { };
    }
    std::string pairString = options.substr(beginLocation + beginString.size(), endLocation - (beginLocation + beginString.size()));
    return parseTestHeaderString(pairString, path, keyTypeMap).value_or(TestFeatures { });
}

TestFeatures featureDefaultsFromTestHeaderForTest(const TestCommand& command, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    return parseTestHeader(command.absolutePath, keyTypeMap);
}

TestFeatures featureDefaultsFromSelfComparisonHeader(const TestCommand& command, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    if (command.selfComparisonHeader.empty())
        return { };
    return parseTestHeaderString(command.selfComparisonHeader, command.absolutePath, keyTypeMap).value_or(TestFeatures { });
}

TestFeatures featureFromAdditionalHeaderOption(const TestCommand& command, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    if (command.additionalHeader.empty())
        return { };
    return parseTestHeaderString(command.additionalHeader, std::filesystem::path(), keyTypeMap).value_or(TestFeatures { });
}

std::optional<TestFeatures> parseAdditionalHeaderString(const std::string& additionalHeader, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    return parseTestHeaderString(additionalHeader, std::filesystem::path(), keyTypeMap);
}

} // namespace WTF
