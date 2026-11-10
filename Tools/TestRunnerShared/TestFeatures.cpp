/*
 * Copyright (C) 2020-2026 Apple Inc. All rights reserved.
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
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <wtf/ASCIICType.h>
#include <wtf/StdFilesystem.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/UTF8CStringView.h>

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

static bool shouldEnableSiteIsolation(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "site-isolation/inspector/") || pathContains(pathOrURL, "/document-isolation-policy/");
}

static bool shouldUseBackForwardCache(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "://web-platform.test");
}

static bool shouldEnableTouchEventRegions(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "touch-event-regions-layer-tree/");
}

static bool shouldEnableGlobalPrivacyControl(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "/gpc/");
}

static bool shouldEnableLocalNetworkAccess(const std::string& pathOrURL)
{
    // Enabled by path rather than as testable, because Local Network Access exempts loopback addresses from mixed content blocking, which changes the results of the mixed content tests. https://bugs.webkit.org/show_bug.cgi?id=326578
    return pathContains(pathOrURL, "/local-network-access/");
}

static bool shouldDumpCSSPageBoxes(const std::string& pathOrURL)
{
    if (!isWebPlatformTestURL(URL { { }, String::fromUTF8(pathOrURL.c_str()) }))
        return false;
    return pathContains(pathOrURL, "-print.") || pathContains(pathOrURL, "-print-expected") || pathContains(pathOrURL, "/print/");
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
    if (shouldEnableSiteIsolation(command.pathOrURL))
        features.boolWebPreferenceFeatures.insert({ "SiteIsolationEnabled", true });
    if (shouldUseBackForwardCache(command.pathOrURL))
        features.boolWebPreferenceFeatures.insert({ "UsesBackForwardCache", true });
    if (shouldEnableTouchEventRegions(command.pathOrURL))
        features.boolWebPreferenceFeatures.insert({ "AlwaysUseTouchEventRegions", true });
    if (shouldEnableGlobalPrivacyControl(command.pathOrURL))
        features.boolTestRunnerFeatures.insert({ "globalPrivacyControl", true });
    if (shouldEnableLocalNetworkAccess(command.pathOrURL))
        features.boolWebPreferenceFeatures.insert({ "LocalNetworkAccessEnabled", true });
    if (shouldDumpCSSPageBoxes(command.pathOrURL))
        features.boolTestRunnerFeatures.insert({ "dumpCSSPageBoxes", true });

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

static constexpr std::string_view skipIfDisabledHeaderKey { "skipIfDisabled" };

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

static std::optional<TestFeatures> parseTestHeaderString(const std::string& pairString, std::filesystem::path path, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap, std::vector<std::string>* skipIfDisabledKeys = nullptr)
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
                LOG_ERROR("Malformed option in test header (could not find '=' character) in %s", path.string().c_str());
            else
                LOG_ERROR("Malformed option in --additional-header option value (could not find '=' character)");
            break;
        }
        auto key = pairString.substr(pairStart, equalsLocation - pairStart);
        auto value = pairString.substr(equalsLocation + 1, pairEnd - (equalsLocation + 1));

        if (skipIfDisabledKeys && key == skipIfDisabledHeaderKey) {
            std::ranges::move(parseStringTestHeaderValueAsStringVector(value), std::back_inserter(*skipIfDisabledKeys));
        } else if (!parseTestHeaderFeature(features, key, value, path, keyTypeMap)) {
            if (!path.empty())
                LOG_ERROR("Unknown key, '%s', in test header in %s", key.c_str(), path.string().c_str());
            else
                LOG_ERROR("Unknown key, '%s', in --additional-header option value", key.c_str());
        }
        pairStart = pairEnd + 1;
    }

    return features;
}

static TestHeader parseTestHeader(std::filesystem::path path, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return { };

    std::ifstream file(path);
    if (!file.good()) {
        LOG_ERROR("Could not open file to inspect test headers in %s", path.string().c_str());
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
        LOG_ERROR("Could not find end of test header in %s", path.string().c_str());
        return { };
    }
    std::string pairString = options.substr(beginLocation + beginString.size(), endLocation - (beginLocation + beginString.size()));
    TestHeader header;
    header.features = parseTestHeaderString(pairString, path, keyTypeMap, &header.skipIfDisabledKeys).value_or(TestFeatures { });
    return header;
}

TestHeader testHeaderForTest(const TestCommand& command, const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMap)
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

std::set<std::string> parseDisabledKeys(const std::string& commaSeparatedKeys)
{
    std::set<std::string> keys;
    for (auto& item : parseStringTestHeaderValueAsStringVector(commaSeparatedKeys)) {
        auto begin = std::ranges::find_if_not(item, isASCIIWhitespace<char>);
        auto end = std::find_if_not(item.rbegin(), std::make_reverse_iterator(begin), isASCIIWhitespace<char>).base();
        std::string key { begin, end };
        if (key.empty())
            continue;
        if (std::ranges::none_of(key, [](char character) { return isASCIIWhitespace(character) || character == '"' || character == '\''; }))
            keys.insert(WTF::move(key));
        else
            SAFE_FPRINTF(stderr, "WARNING: Ignoring invalid --disabled-keys key '%s': keys must not contain whitespace or quotes\n", UTF8CStringView::unsafeFromUTF8(key.c_str()));
    }
    return keys;
}

std::optional<std::string> firstDisabledKeyForTest(const TestCommand& command, const TestHeader& testHeader, const std::set<std::string>& disabledKeys)
{
    if (command.isReference) {
        if (!testHeader.skipIfDisabledKeys.empty())
            SAFE_FPRINTF(stderr, "WARNING: Ignoring skipIfDisabled in reference file: %s\n", UTF8CStringView::unsafeFromUTF8(command.absolutePath.string().c_str()));
        return std::nullopt;
    }
    for (auto& key : testHeader.skipIfDisabledKeys) {
        if (disabledKeys.contains(key))
            return key;
    }
    for (auto& key : testHeader.skipIfDisabledKeys) {
        if (auto disabledKey = similarDisabledKey(key, disabledKeys))
            SAFE_FPRINTF(stderr, "WARNING: skipIfDisabled key '%s' is close to --disabled-keys key '%s' in test file: %s\n", UTF8CStringView::unsafeFromUTF8(key.c_str()), UTF8CStringView::unsafeFromUTF8(disabledKey->c_str()), UTF8CStringView::unsafeFromUTF8(command.absolutePath.string().c_str()));
    }
    return std::nullopt;
}

static std::string withDigitRunsMasked(const std::string& key)
{
    std::string result;
    bool previousWasDigit = false;
    for (char character : key) {
        bool isDigit = isASCIIDigit(character);
        if (!isDigit)
            result += character;
        else if (!previousWasDigit)
            result += '\0';
        previousWasDigit = isDigit;
    }
    return result;
}

static std::string lowercased(std::string string)
{
    std::ranges::transform(string, string.begin(), toASCIILower<char>);
    return string;
}

static size_t editDistance(const std::string& a, const std::string& b)
{
    std::vector<std::vector<size_t>> distance(a.size() + 1, std::vector<size_t>(b.size() + 1));
    for (size_t i = 0; i <= a.size(); ++i)
        distance[i][0] = i;
    for (size_t j = 0; j <= b.size(); ++j)
        distance[0][j] = j;
    for (size_t i = 1; i <= a.size(); ++i) {
        for (size_t j = 1; j <= b.size(); ++j) {
            size_t substitutionCost = a[i - 1] == b[j - 1] ? 0 : 1;
            distance[i][j] = std::min({ distance[i - 1][j] + 1, distance[i][j - 1] + 1, distance[i - 1][j - 1] + substitutionCost });
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
                distance[i][j] = std::min(distance[i][j], distance[i - 2][j - 2] + 1);
        }
    }
    return distance[a.size()][b.size()];
}

std::optional<std::string> similarDisabledKey(const std::string& headerKey, const std::set<std::string>& disabledKeys)
{
    auto maskedHeaderKey = withDigitRunsMasked(headerKey);
    auto lowercasedHeaderKey = lowercased(maskedHeaderKey);
    for (auto& disabledKey : disabledKeys) {
        auto maskedDisabledKey = withDigitRunsMasked(disabledKey);
        if (maskedHeaderKey == maskedDisabledKey)
            continue;
        size_t maximumDistance = std::min(maskedHeaderKey.size(), maskedDisabledKey.size()) >= 8 ? 2 : 1;
        if (editDistance(lowercasedHeaderKey, lowercased(maskedDisabledKey)) <= maximumDistance)
            return disabledKey;
    }
    return std::nullopt;
}

void dumpSkippedTestReply(const std::string& disabledKey)
{
    fputs("Content-Type: text/x-webkit-skipped\nskipIfDisabled: ", stdout);
    fputs(disabledKey.c_str(), stdout);
    fputs("\n#EOF\n#EOF\n", stdout);
    fputs("#EOF\n", stderr);
    fflush(stdout);
    fflush(stderr);
}

} // namespace WTF
