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

#include "config.h"
#include "TestOptions.h"

#include <fstream>
#include <string>
#include <wtf/text/WTFString.h>

static bool parseBooleanTestHeaderValue(const std::string& value)
{
    if (value == "true")
        return true;
    if (value == "false")
        return false;

    LOG_ERROR("Found unexpected value '%s' for boolean option. Expected 'true' or 'false'.", value.c_str());
    return false;
}

static bool pathContains(const std::string& pathOrURL, const char* substring)
{
    String path(pathOrURL.c_str());
    return path.contains(substring);
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

TestOptions::TestOptions(const std::string& pathOrURL, const std::string& absolutePath)
{
    const auto& path = absolutePath.empty() ? pathOrURL : absolutePath;
    if (path.empty())
        return;

    dumpJSConsoleLogInStdErr = shouldDumpJSConsoleLogInStdErr(pathOrURL);

    std::string options;
    std::ifstream testFile(path.data());
    if (!testFile.good())
        return;
    getline(testFile, options);
    std::string beginString("webkit-test-runner [ ");
    std::string endString(" ]");
    size_t beginLocation = options.find(beginString);
    if (beginLocation == std::string::npos)
        return;
    size_t endLocation = options.find(endString, beginLocation);
    if (endLocation == std::string::npos) {
        LOG_ERROR("Could not find end of test header in %s", path.c_str());
        return;
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
        if (key == "enableAttachmentElement")
            enableAttachmentElement = parseBooleanTestHeaderValue(value);
        if (key == "useAcceleratedDrawing")
            useAcceleratedDrawing = parseBooleanTestHeaderValue(value);
        else if (key == "enableIntersectionObserver")
            enableIntersectionObserver = parseBooleanTestHeaderValue(value);
        else if (key == "useEphemeralSession")
            useEphemeralSession = parseBooleanTestHeaderValue(value);
        else if (key == "enableBackForwardCache")
            enableBackForwardCache = parseBooleanTestHeaderValue(value);
        else if (key == "enableMenuItemElement")
            enableMenuItemElement = parseBooleanTestHeaderValue(value);
        else if (key == "enableKeygenElement")
            enableKeygenElement = parseBooleanTestHeaderValue(value);
        else if (key == "enableModernMediaControls")
            enableModernMediaControls = parseBooleanTestHeaderValue(value);
        else if (key == "enablePointerLock")
            enablePointerLock = parseBooleanTestHeaderValue(value);
        else if (key == "enableDragDestinationActionLoad")
            enableDragDestinationActionLoad = parseBooleanTestHeaderValue(value);
        else if (key == "layerBackedWebView")
            layerBackedWebView = parseBooleanTestHeaderValue(value);
        else if (key == "enableIsSecureContextAttribute")
            enableIsSecureContextAttribute = parseBooleanTestHeaderValue(value);
        else if (key == "enableInspectorAdditions")
            enableInspectorAdditions = parseBooleanTestHeaderValue(value);
        else if (key == "dumpJSConsoleLogInStdErr")
            dumpJSConsoleLogInStdErr = parseBooleanTestHeaderValue(value);
        else if (key == "allowCrossOriginSubresourcesToAskForCredentials")
            allowCrossOriginSubresourcesToAskForCredentials = parseBooleanTestHeaderValue(value);
        else if (key == "experimental:WebAnimationsCSSIntegrationEnabled")
            enableWebAnimationsCSSIntegration = parseBooleanTestHeaderValue(value);
        else if (key == "internal:selectionAcrossShadowBoundariesEnabled")
            enableSelectionAcrossShadowBoundaries = parseBooleanTestHeaderValue(value);
        else if (key == "enableColorFilter")
            enableColorFilter = parseBooleanTestHeaderValue(value);
        else if (key == "jscOptions")
            jscOptions = value;
        else if (key == "additionalSupportedImageTypes")
            additionalSupportedImageTypes = value;
        else if (key == "experimental:WebGPUEnabled")
            enableWebGPU = parseBooleanTestHeaderValue(value);
        else if (key == "internal:CSSLogicalEnabled")
            enableCSSLogical = parseBooleanTestHeaderValue(value);
        else if (key == "internal:LineHeightUnitsEnabled")
            enableLineHeightUnits = parseBooleanTestHeaderValue(value);
        else if (key == "experimental:AdClickAttributionEnabled")
            adClickAttributionEnabled = parseBooleanTestHeaderValue(value);
        else if (key == "experimental:ResizeObserverEnabled")
            enableResizeObserver = parseBooleanTestHeaderValue(value);
        else if (key == "experimental:CSSOMViewSmoothScrollingEnabled")
            enableCSSOMViewSmoothScrolling = parseBooleanTestHeaderValue(value);
        else if (key == "experimental:CoreMathMLEnabled")
            enableCoreMathML = parseBooleanTestHeaderValue(value);
        else if (key == "experimental:RequestIdleCallbackEnabled")
            enableRequestIdleCallback = parseBooleanTestHeaderValue(value);
        else if (key == "experimental:AsyncClipboardAPIEnabled")
            enableAsyncClipboardAPI = parseBooleanTestHeaderValue(value);
        else if (key == "internal:LayoutFormattingContextIntegrationEnabled")
            layoutFormattingContextIntegrationEnabled = parseBooleanTestHeaderValue(value);
        else if (key == "experimental:AspectRatioOfImgFromWidthAndHeightEnabled")
            enableAspectRatioOfImgFromWidthAndHeight = parseBooleanTestHeaderValue(value);
        else if (key == "allowTopNavigationToDataURLs")
            allowTopNavigationToDataURLs = parseBooleanTestHeaderValue(value);
        pairStart = pairEnd + 1;
    }
}

bool TestOptions::webViewIsCompatibleWithOptions(const TestOptions& other) const
{
    return other.layerBackedWebView == layerBackedWebView
        && other.jscOptions == jscOptions
        && other.enableWebAnimationsCSSIntegration == enableWebAnimationsCSSIntegration;
}
