/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include <fstream>
#include <string>
#include <wtf/StdFilesystem.h>

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
    if (a.stringTestRunnerFeatures != b.stringTestRunnerFeatures)
        return false;
    if (a.stringVectorTestRunnerFeatures != b.stringVectorTestRunnerFeatures)
        return false;
    return true;
}

bool operator!=(const TestFeatures& a, const TestFeatures& b)
{
    return !(a == b);
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
    return pathContains(pathOrURL, "localhost:8800/beacon") || pathContains(pathOrURL, "localhost:9443/beacon")
        || pathContains(pathOrURL, "localhost:8800/cors") || pathContains(pathOrURL, "localhost:9443/cors")
        || pathContains(pathOrURL, "localhost:8800/fetch") || pathContains(pathOrURL, "localhost:9443/fetch")
        || pathContains(pathOrURL, "localhost:8800/service-workers") || pathContains(pathOrURL, "localhost:9443/service-workers")
        || pathContains(pathOrURL, "localhost:8800/streams/writable-streams") || pathContains(pathOrURL, "localhost:9443/streams/writable-streams")
        || pathContains(pathOrURL, "localhost:8800/streams/piping") || pathContains(pathOrURL, "localhost:9443/streams/piping")
        || pathContains(pathOrURL, "localhost:8800/xhr") || pathContains(pathOrURL, "localhost:9443/xhr")
        || pathContains(pathOrURL, "localhost:8800/webrtc") || pathContains(pathOrURL, "localhost:9443/webrtc")
        || pathContains(pathOrURL, "localhost:8800/websockets") || pathContains(pathOrURL, "localhost:9443/websockets");
}

static bool shouldEnableWebGPU(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "127.0.0.1:8000/webgpu");
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
    if (shouldEnableWebGPU(command.pathOrURL))
        features.boolWebPreferenceFeatures.insert({ "WebGPU", true });

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

bool parseTestHeaderFeature(TestFeatures& features, std::string key, std::string value, std::filesystem::path path, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
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

static TestFeatures parseTestHeaderString(const std::string& pairString, std::filesystem::path path, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    TestFeatures features;

    size_t pairStart = 0;
    while (pairStart < pairString.size()) {
        size_t pairEnd = pairString.find(" ", pairStart);
        if (pairEnd == std::string::npos)
            pairEnd = pairString.size();
        size_t equalsLocation = pairString.find("=", pairStart);
        if (equalsLocation == std::string::npos) {
            LOG_ERROR("Malformed option in test header (could not find '=' character) in %s", path.c_str());
            break;
        }
        auto key = pairString.substr(pairStart, equalsLocation - pairStart);
        auto value = pairString.substr(equalsLocation + 1, pairEnd - (equalsLocation + 1));
        
        if (!parseTestHeaderFeature(features, key, value, path, keyTypeMap))
            LOG_ERROR("Unknown key, '%s', in test header in %s", key.c_str(), path.c_str());
        
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
    return parseTestHeaderString(pairString, path, keyTypeMap);
}

TestFeatures featureDefaultsFromTestHeaderForTest(const TestCommand& command, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    return parseTestHeader(command.absolutePath, keyTypeMap);
}

TestFeatures featureDefaultsFromSelfComparisonHeader(const TestCommand& command, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    if (command.selfComparisonHeader.empty())
        return { };
    return parseTestHeaderString(command.selfComparisonHeader, command.absolutePath, keyTypeMap);
}

} // namespace WTF
