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

#include "StringFunctions.h"
#include "TestController.h"
#include <fstream>
#include <iostream>
#include <string>
#include <wtf/Optional.h>
#include <wtf/StdFilesystem.h>
#include <wtf/text/WTFString.h>

#define DUMP_FEATURES 0

namespace WTR {

#if DUMP_FEATURES
static void dumpFeatures(const TestFeatures& features)
{
    if (features.experimentalFeatures.empty() && features.internalDebugFeatures.empty() && features.boolFeatures.empty() && features.doubleFeatures.empty() && features.stringFeatures.empty() && features.stringVectorFeatures.empty()) {
        std::cerr << "  [EMPTY]\n";
        return;
    }
    
    if (!features.experimentalFeatures.empty()) {
        std::cerr << "  Experimental Features: \n";
        for (auto [key, value] : features.experimentalFeatures)
            std::cerr << "    " << key << ": " << value << '\n';
    }
    if (!features.internalDebugFeatures.empty()) {
        std::cerr << "  Internal Features: \n";
        for (auto [key, value] : features.internalDebugFeatures)
            std::cerr << "    " << key << ": " << value << '\n';
    }
    if (!features.boolFeatures.empty()) {
        std::cerr << "  Bool Features: \n";
        for (auto [key, value] : features.boolFeatures)
            std::cerr << "    " << key << ": " << value << '\n';
    }
    if (!features.doubleFeatures.empty()) {
        std::cerr << "  Double Features: \n";
        for (auto [key, value] : features.doubleFeatures)
            std::cerr << "    " << key << ": " << value << '\n';
    }
    if (!features.stringFeatures.empty()) {
        std::cerr << "  String Features: \n";
        for (auto [key, value] : features.stringFeatures)
            std::cerr << "    " << key << ": " << value << '\n';
    }
    if (!features.stringVectorFeatures.empty()) {
        std::cerr << "  String Vector Features: \n";
        for (auto [key, value] : features.stringVectorFeatures)
            std::cerr << "    " << key << ": Number of strings " << value.size() << '\n';
    }
}
#endif

void merge(TestFeatures& base, TestFeatures additional)
{
    // FIXME: This should use std::unordered_map::merge when it is available for all ports.

    for (auto [key, value] : additional.experimentalFeatures)
        base.experimentalFeatures.insert_or_assign(key, value);
    for (auto [key, value] : additional.internalDebugFeatures)
        base.internalDebugFeatures.insert_or_assign(key, value);
    for (auto [key, value] : additional.boolFeatures)
        base.boolFeatures.insert_or_assign(key, value);
    for (auto [key, value] : additional.doubleFeatures)
        base.doubleFeatures.insert_or_assign(key, value);
    for (auto [key, value] : additional.stringFeatures)
        base.stringFeatures.insert_or_assign(key, value);
    for (auto [key, value] : additional.stringVectorFeatures)
        base.stringVectorFeatures.insert_or_assign(key, value);
}

static const std::unordered_map<std::string, bool>& boolDefaultsMap()
{
    static std::unordered_map<std::string, bool> map {
        { "useThreadedScrolling", false },
        { "useAcceleratedDrawing", false },
        { "useRemoteLayerTree", false },
        { "shouldShowWebView", false },
        { "useFlexibleViewport", false },
        { "useDataDetection", false },
        { "useMockScrollbars", true },
        { "needsSiteSpecificQuirks", false },
        { "ignoresViewportScaleLimits", false },
        { "useCharacterSelectionGranularity", false },
        { "enableAttachmentElement", false },
        { "enableIntersectionObserver", false },
        { "useEphemeralSession", false },
        { "enableMenuItemElement", false },
        { "enableKeygenElement", false },
        { "enableModernMediaControls", true },
        { "enablePointerLock", false },
        { "enableWebAuthentication", true },
        { "enableWebAuthenticationLocalAuthenticator", true },
        { "enableInspectorAdditions", false },
        { "shouldShowTouches", false },
        { "dumpJSConsoleLogInStdErr", false },
        { "allowCrossOriginSubresourcesToAskForCredentials", false },
        { "domPasteAllowed", true },
        { "enableColorFilter", false },
        { "punchOutWhiteBackgroundsInDarkMode", false },
        { "runSingly", false },
        { "checkForWorldLeaks", false },
        { "shouldIgnoreMetaViewport", false },
        { "spellCheckingDots", false },
        { "enableServiceControls", false },
        { "editable", false },
        { "shouldHandleRunOpenPanel", true },
        { "shouldPresentPopovers", true },
        { "enableAppNap", false },
        { "enableBackForwardCache", false },
        { "allowsLinkPreview", true },
        { "enableCaptureVideoInUIProcess", false },
        { "enableCaptureVideoInGPUProcess", false },
        { "enableCaptureAudioInUIProcess", false },
        { "enableCaptureAudioInGPUProcess", false },
        { "allowTopNavigationToDataURLs", true },
        { "enableInAppBrowserPrivacy", false },
        { "isAppBoundWebView", false },
        { "ignoreSynchronousMessagingTimeouts", false },
        { "enableProcessSwapOnNavigation", true },
        { "enableProcessSwapOnWindowOpen", false },
        { "useServiceWorkerShortTimeout", false }
    };
    return map;
}

static const std::unordered_map<std::string, double>& doubleDefaultsMap()
{
    static std::unordered_map<std::string, double> map {
        { "contentInset.top", 0 },
        { "deviceScaleFactor", 1 },
        { "viewWidth", 800 },
        { "viewHeight", 600 }
    };
    return map;
}

static const std::unordered_map<std::string, std::string>& stringDefaultsMap()
{
    static std::unordered_map<std::string, std::string> map {
        { "applicationManifest", { } },
        { "jscOptions", { } },
        { "additionalSupportedImageTypes", { } },
        { "standaloneWebApplicationURL", { } },
        { "contentMode", { } },
        { "applicationBundleIdentifier", { } }
    };
    return map;
}

static const std::unordered_map<std::string, std::vector<std::string>>& stringVectorDefaultsMap()
{
    static std::unordered_map<std::string, std::vector<std::string>> map {
        { "language", { } }
    };
    return map;
}

enum class KeyType : uint8_t {
    Bool,
    Double,
    String,
    StringRelativePath,
    StringURL,
    StringVector,
    Unknown
};

static const std::unordered_map<std::string, KeyType>& keyTypeMap()
{
    static std::unordered_map<std::string, KeyType> map {
        { "useThreadedScrolling", KeyType::Bool },
        { "useAcceleratedDrawing", KeyType::Bool },
        { "useRemoteLayerTree", KeyType::Bool },
        { "shouldShowWebView", KeyType::Bool },
        { "useFlexibleViewport", KeyType::Bool },
        { "useDataDetection", KeyType::Bool },
        { "useMockScrollbars", KeyType::Bool },
        { "needsSiteSpecificQuirks", KeyType::Bool },
        { "ignoresViewportScaleLimits", KeyType::Bool },
        { "useCharacterSelectionGranularity", KeyType::Bool },
        { "enableAttachmentElement", KeyType::Bool },
        { "enableIntersectionObserver", KeyType::Bool },
        { "useEphemeralSession", KeyType::Bool },
        { "enableMenuItemElement", KeyType::Bool },
        { "enableKeygenElement", KeyType::Bool },
        { "enableModernMediaControls", KeyType::Bool },
        { "enablePointerLock", KeyType::Bool },
        { "enableWebAuthentication", KeyType::Bool },
        { "enableWebAuthenticationLocalAuthenticator", KeyType::Bool },
        { "enableInspectorAdditions", KeyType::Bool },
        { "shouldShowTouches", KeyType::Bool },
        { "dumpJSConsoleLogInStdErr", KeyType::Bool },
        { "allowCrossOriginSubresourcesToAskForCredentials", KeyType::Bool },
        { "domPasteAllowed", KeyType::Bool },
        { "enableColorFilter", KeyType::Bool },
        { "punchOutWhiteBackgroundsInDarkMode", KeyType::Bool },
        { "runSingly", KeyType::Bool },
        { "checkForWorldLeaks", KeyType::Bool },
        { "shouldIgnoreMetaViewport", KeyType::Bool },
        { "spellCheckingDots", KeyType::Bool },
        { "enableServiceControls", KeyType::Bool },
        { "editable", KeyType::Bool },
        { "shouldHandleRunOpenPanel", KeyType::Bool },
        { "shouldPresentPopovers", KeyType::Bool },
        { "enableAppNap", KeyType::Bool },
        { "enableBackForwardCache", KeyType::Bool },
        { "allowsLinkPreview", KeyType::Bool },
        { "enableCaptureVideoInUIProcess", KeyType::Bool },
        { "enableCaptureVideoInGPUProcess", KeyType::Bool },
        { "enableCaptureAudioInUIProcess", KeyType::Bool },
        { "enableCaptureAudioInGPUProcess", KeyType::Bool },
        { "allowTopNavigationToDataURLs", KeyType::Bool },
        { "enableInAppBrowserPrivacy", KeyType::Bool },
        { "isAppBoundWebView", KeyType::Bool },
        { "ignoreSynchronousMessagingTimeouts", KeyType::Bool },
        { "enableProcessSwapOnNavigation", KeyType::Bool },
        { "enableProcessSwapOnWindowOpen", KeyType::Bool },
        { "useServiceWorkerShortTimeout", KeyType::Bool },
    
        { "contentInset.top", KeyType::Double },
        { "deviceScaleFactor", KeyType::Double },
        { "viewWidth", KeyType::Double },
        { "viewHeight", KeyType::Double },

        { "jscOptions", KeyType::String },
        { "additionalSupportedImageTypes", KeyType::String },
        { "contentMode", KeyType::String },
        { "applicationBundleIdentifier", KeyType::String },
        { "applicationManifest", KeyType::StringRelativePath },
        { "standaloneWebApplicationURL", KeyType::StringURL },

        { "language", KeyType::StringVector },
    };

    return map;
}

static KeyType keyType(std::string key)
{
    auto map = keyTypeMap();
    auto it = map.find(key);
    if (it == map.end())
        return KeyType::Unknown;
    return it->second;
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

static std::string parseStringTestHeaderValueAsRelativePath(const std::string& value, const std::string& pathOrURL)
{
    auto baseURL = adoptWK(TestController::createTestURL(pathOrURL.c_str()));
    auto relativeURL = adoptWK(WKURLCreateWithBaseURL(baseURL.get(), value.c_str()));
    return toSTD(adoptWK(WKURLCopyPath(relativeURL.get())));
}

static std::string parseStringTestHeaderValueAsURL(const std::string& value)
{
    return toSTD(adoptWK(WKURLCopyString(TestController::createTestURL(value.c_str()))));
}

static TestFeatures parseTestHeader(std::filesystem::path path, const std::string& pathOrURL)
{
    TestFeatures features;
    if (!std::filesystem::exists(path))
        return features;

    std::ifstream file(path);
    if (!file.good()) {
        LOG_ERROR("Could not open file to inspect test headers in %s", path.c_str());
        return features;
    }

    std::string options;
    getline(file, options);
    std::string beginString("webkit-test-runner [ ");
    std::string endString(" ]");
    size_t beginLocation = options.find(beginString);
    if (beginLocation == std::string::npos)
        return features;
    size_t endLocation = options.find(endString, beginLocation);
    if (endLocation == std::string::npos) {
        LOG_ERROR("Could not find end of test header in %s", path.c_str());
        return features;
    }
    std::string pairString = options.substr(beginLocation + beginString.size(), endLocation - (beginLocation + beginString.size()));
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

        if (key.rfind("experimental:") == 0) {
            key = key.substr(13);
            features.experimentalFeatures.insert({ key, parseBooleanTestHeaderValue(value) });
        } else if (key.rfind("internal:") == 0) {
            key = key.substr(9);
            features.internalDebugFeatures.insert({ key, parseBooleanTestHeaderValue(value) });
        }

        switch (keyType(key)) {
        case KeyType::Bool:
            features.boolFeatures.insert({ key, parseBooleanTestHeaderValue(value) });
            break;
        case KeyType::Double:
            features.doubleFeatures.insert({ key, std::stod(value) });
            break;
        case KeyType::String:
            features.stringFeatures.insert({ key, value });
            break;
        case KeyType::StringRelativePath:
            features.stringFeatures.insert({ key, parseStringTestHeaderValueAsRelativePath(value, pathOrURL) });
            break;
        case KeyType::StringURL:
            features.stringFeatures.insert({ key, parseStringTestHeaderValueAsURL(value) });
            break;
        case KeyType::StringVector:
            features.stringVectorFeatures.insert({ key, split(value, ',') });
            break;
        case KeyType::Unknown:
            LOG_ERROR("Unknown key, '%s, in test header in %s", key.c_str(), path.c_str());
            break;
        }

        pairStart = pairEnd + 1;
    }

    return features;
}

static bool pathContains(const std::string& pathOrURL, const char* substring)
{
    String path(pathOrURL.c_str());
    return path.contains(substring); // Case-insensitive.
}

static bool shouldMakeViewportFlexible(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "viewport/") && !pathContains(pathOrURL, "visual-viewport/");
}

static bool shouldUseEphemeralSession(const std::string& pathOrURL)
{
    return pathContains(pathOrURL, "w3c/IndexedDB-private-browsing") || pathContains(pathOrURL, "w3c\\IndexedDB-private-browsing");
}

static Optional<std::pair<double, double>> overrideViewWidthAndHeightForTest(const std::string& pathOrURL)
{
    if (pathContains(pathOrURL, "svg/W3C-SVG-1.1") || pathContains(pathOrURL, "svg\\W3C-SVG-1.1"))
        return { { 480, 360 } };
    return WTF::nullopt;
}

static Optional<double> overrideDeviceScaleFactorForTest(const std::string& pathOrURL)
{
    if (pathContains(pathOrURL, "/hidpi-3x-"))
        return 3;
    if (pathContains(pathOrURL, "/hidpi-"))
        return 2;
    return WTF::nullopt;
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

TestFeatures hardcodedFeaturesBasedOnPathForTest(const TestCommand& command)
{
    TestFeatures features;

    if (shouldMakeViewportFlexible(command.pathOrURL))
        features.boolFeatures.insert({ "useFlexibleViewport", true });
    if (shouldUseEphemeralSession(command.pathOrURL))
        features.boolFeatures.insert({ "useEphemeralSession", true });
    if (shouldDumpJSConsoleLogInStdErr(command.pathOrURL))
        features.boolFeatures.insert({ "dumpJSConsoleLogInStdErr", true });
    if (auto deviceScaleFactor = overrideDeviceScaleFactorForTest(command.pathOrURL); deviceScaleFactor != WTF::nullopt)
        features.doubleFeatures.insert({ "deviceScaleFactor", deviceScaleFactor.value() });
    if (auto viewWidthAndHeight = overrideViewWidthAndHeightForTest(command.pathOrURL); viewWidthAndHeight != WTF::nullopt) {
        features.doubleFeatures.insert({ "viewWidth", viewWidthAndHeight->first });
        features.doubleFeatures.insert({ "viewHeight", viewWidthAndHeight->second });
    }

    return features;
}

TestFeatures featureDefaultsFromTestHeaderForTest(const TestCommand& command)
{
    return parseTestHeader(command.absolutePath, command.pathOrURL);
}

TestOptions::TestOptions(TestFeatures features)
    : m_features { features }
{
#if DUMP_FEATURES
    std::cerr << "DUMPING FEATURES\n";
    dumpFeatures(m_features);
#endif
}

bool TestOptions::hasSameInitializationOptions(const TestOptions& options) const
{
    if (m_features.experimentalFeatures != options.m_features.experimentalFeatures)
        return false;
    if (m_features.internalDebugFeatures != options.m_features.internalDebugFeatures)
        return false;

    for (auto [key, keyType] : keyTypeMap()) {
        switch (keyType) {
        case KeyType::Bool:
            if (boolFeatureValue(key) != options.boolFeatureValue(key))
                return false;
            break;
        case KeyType::Double:
            if (doubleFeatureValue(key) != options.doubleFeatureValue(key))
                return false;
            break;
        case KeyType::String:
        case KeyType::StringRelativePath:
        case KeyType::StringURL:
            if (stringFeatureValue(key) != options.stringFeatureValue(key))
                return false;
            break;
        case KeyType::StringVector:
            if (stringVectorFeatureValue(key) != options.stringVectorFeatureValue(key))
                return false;
            break;
        case KeyType::Unknown:
            ASSERT_NOT_REACHED();
        }
    }
    return true;
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
