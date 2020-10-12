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

#include "TestFeatures.h"
#include <fstream>
#include <string>

namespace WTR {

const std::unordered_map<std::string, TestHeaderKeyType>& TestOptions::keyTypeMapping()
{
    static std::unordered_map<std::string, TestHeaderKeyType> map {
        { "allowCrossOriginSubresourcesToAskForCredentials", TestHeaderKeyType::Bool },
        { "allowTopNavigationToDataURLs", TestHeaderKeyType::Bool },
        { "dumpJSConsoleLogInStdErr", TestHeaderKeyType::Bool },
        { "enableAttachmentElement", TestHeaderKeyType::Bool },
        { "enableBackForwardCache", TestHeaderKeyType::Bool },
        { "enableColorFilter", TestHeaderKeyType::Bool },
        { "enableDragDestinationActionLoad", TestHeaderKeyType::Bool },
        { "enableInspectorAdditions", TestHeaderKeyType::Bool },
        { "enableIntersectionObserver", TestHeaderKeyType::Bool },
        { "enableKeygenElement", TestHeaderKeyType::Bool },
        { "enableMenuItemElement", TestHeaderKeyType::Bool },
        { "enableModernMediaControls", TestHeaderKeyType::Bool },
        { "enablePointerLock", TestHeaderKeyType::Bool },
        { "layerBackedWebView", TestHeaderKeyType::Bool },
        { "useAcceleratedDrawing", TestHeaderKeyType::Bool },
        { "useEphemeralSession", TestHeaderKeyType::Bool },

        { "additionalSupportedImageTypes", TestHeaderKeyType::String },
        { "jscOptions", TestHeaderKeyType::String },
    };

    return map;
}

template<typename T> static void setValueIfSetInMap(T& valueToSet, std::string key, const std::unordered_map<std::string, T>& map)
{
    auto it = map.find(key);
    if (it == map.end())
        return;
    valueToSet = it->second;
}

TestOptions::TestOptions(TestFeatures testFeatures)
{
    setValueIfSetInMap(allowCrossOriginSubresourcesToAskForCredentials, "allowCrossOriginSubresourcesToAskForCredentials", testFeatures.boolFeatures);
    setValueIfSetInMap(allowTopNavigationToDataURLs, "allowTopNavigationToDataURLs", testFeatures.boolFeatures);
    setValueIfSetInMap(dumpJSConsoleLogInStdErr, "dumpJSConsoleLogInStdErr", testFeatures.boolFeatures);
    setValueIfSetInMap(enableAttachmentElement, "enableAttachmentElement", testFeatures.boolFeatures);
    setValueIfSetInMap(enableBackForwardCache, "enableBackForwardCache", testFeatures.boolFeatures);
    setValueIfSetInMap(enableColorFilter, "enableColorFilter", testFeatures.boolFeatures);
    setValueIfSetInMap(enableDragDestinationActionLoad, "enableDragDestinationActionLoad", testFeatures.boolFeatures);
    setValueIfSetInMap(enableInspectorAdditions, "enableInspectorAdditions", testFeatures.boolFeatures);
    setValueIfSetInMap(enableIntersectionObserver, "enableIntersectionObserver", testFeatures.boolFeatures);
    setValueIfSetInMap(enableKeygenElement, "enableKeygenElement", testFeatures.boolFeatures);
    setValueIfSetInMap(enableMenuItemElement, "enableMenuItemElement", testFeatures.boolFeatures);
    setValueIfSetInMap(enableModernMediaControls, "enableModernMediaControls", testFeatures.boolFeatures);
    setValueIfSetInMap(enablePointerLock, "enablePointerLock", testFeatures.boolFeatures);
    setValueIfSetInMap(layerBackedWebView, "layerBackedWebView", testFeatures.boolFeatures);
    setValueIfSetInMap(useAcceleratedDrawing, "useAcceleratedDrawing", testFeatures.boolFeatures);
    setValueIfSetInMap(useEphemeralSession, "useEphemeralSession", testFeatures.boolFeatures);

    setValueIfSetInMap(additionalSupportedImageTypes, "additionalSupportedImageTypes", testFeatures.stringFeatures);
    setValueIfSetInMap(jscOptions, "jscOptions", testFeatures.stringFeatures);

    setValueIfSetInMap(enableCSSLogical, "CSSLogicalEnabled", testFeatures.internalDebugFeatures);
    setValueIfSetInMap(enableLineHeightUnits, "LineHeightUnitsEnabled", testFeatures.internalDebugFeatures);
    setValueIfSetInMap(enableSelectionAcrossShadowBoundaries, "selectionAcrossShadowBoundariesEnabled", testFeatures.internalDebugFeatures);
    setValueIfSetInMap(layoutFormattingContextIntegrationEnabled, "LayoutFormattingContextIntegrationEnabled", testFeatures.internalDebugFeatures);

    setValueIfSetInMap(adClickAttributionEnabled, "AdClickAttributionEnabled", testFeatures.experimentalFeatures);
    setValueIfSetInMap(enableAspectRatioOfImgFromWidthAndHeight, "AspectRatioOfImgFromWidthAndHeightEnabled", testFeatures.experimentalFeatures);
    setValueIfSetInMap(enableAsyncClipboardAPI, "AsyncClipboardAPIEnabled", testFeatures.experimentalFeatures);
    setValueIfSetInMap(enableCSSOMViewSmoothScrolling, "CSSOMViewSmoothScrollingEnabled", testFeatures.experimentalFeatures);
    setValueIfSetInMap(enableContactPickerAPI, "ContactPickerAPIEnabled", testFeatures.experimentalFeatures);
    setValueIfSetInMap(enableCoreMathML, "CoreMathMLEnabled", testFeatures.experimentalFeatures);
    setValueIfSetInMap(enableRequestIdleCallback, "RequestIdleCallbackEnabled", testFeatures.experimentalFeatures);
    setValueIfSetInMap(enableResizeObserver, "ResizeObserverEnabled", testFeatures.experimentalFeatures);
    setValueIfSetInMap(enableWebGPU, "WebGPUEnabled", testFeatures.experimentalFeatures);
}

bool TestOptions::webViewIsCompatibleWithOptions(const TestOptions& other) const
{
    return other.layerBackedWebView == layerBackedWebView
        && other.jscOptions == jscOptions;
}

}
