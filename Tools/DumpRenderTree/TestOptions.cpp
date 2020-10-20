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

#include "TestOptionsGeneratedKeys.h"

namespace WTR {

const std::unordered_map<std::string, TestHeaderKeyType>& TestOptions::keyTypeMapping()
{
    static const std::unordered_map<std::string, TestHeaderKeyType> map {
        GENERATED_WEB_PREFERENCE_KEY_TYPE_MAPPINGS

        { "dumpJSConsoleLogInStdErr", TestHeaderKeyType::BoolTestRunner },
        { "enableDragDestinationActionLoad", TestHeaderKeyType::BoolTestRunner },
        { "layerBackedWebView", TestHeaderKeyType::BoolTestRunner },
        { "useEphemeralSession", TestHeaderKeyType::BoolTestRunner },

        { "additionalSupportedImageTypes", TestHeaderKeyType::StringTestRunner },
        { "jscOptions", TestHeaderKeyType::StringTestRunner },
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
    setValueIfSetInMap(allowCrossOriginSubresourcesToAskForCredentials, "AllowCrossOriginSubresourcesToAskForCredentials", testFeatures.boolWebPreferenceFeatures);
    setValueIfSetInMap(allowTopNavigationToDataURLs, "AllowTopNavigationToDataURLs", testFeatures.boolWebPreferenceFeatures);
    setValueIfSetInMap(enableAcceleratedDrawing, "AcceleratedDrawingEnabled", testFeatures.boolWebPreferenceFeatures);
    setValueIfSetInMap(enableAttachmentElement, "AttachmentElementEnabled", testFeatures.boolWebPreferenceFeatures);
    setValueIfSetInMap(enableBackForwardCache, "UsesBackForwardCache", testFeatures.boolWebPreferenceFeatures);
    setValueIfSetInMap(enableColorFilter, "ColorFilterEnabled", testFeatures.boolWebPreferenceFeatures);
    setValueIfSetInMap(enableInspectorAdditions, "InspectorAdditionsEnabled", testFeatures.boolWebPreferenceFeatures);
    setValueIfSetInMap(enableIntersectionObserver, "IntersectionObserverEnabled", testFeatures.boolWebPreferenceFeatures);
    setValueIfSetInMap(enableKeygenElement, "KeygenElementEnabled", testFeatures.boolWebPreferenceFeatures);
    setValueIfSetInMap(enableMenuItemElement, "MenuItemElementEnabled", testFeatures.boolWebPreferenceFeatures);
    setValueIfSetInMap(enableModernMediaControls, "ModernMediaControlsEnabled", testFeatures.boolWebPreferenceFeatures);

    setValueIfSetInMap(enableDragDestinationActionLoad, "enableDragDestinationActionLoad", testFeatures.boolTestRunnerFeatures);
    setValueIfSetInMap(dumpJSConsoleLogInStdErr, "dumpJSConsoleLogInStdErr", testFeatures.boolTestRunnerFeatures);
    setValueIfSetInMap(layerBackedWebView, "layerBackedWebView", testFeatures.boolTestRunnerFeatures);
    setValueIfSetInMap(useEphemeralSession, "useEphemeralSession", testFeatures.boolTestRunnerFeatures);

    setValueIfSetInMap(additionalSupportedImageTypes, "additionalSupportedImageTypes", testFeatures.stringTestRunnerFeatures);
    setValueIfSetInMap(jscOptions, "jscOptions", testFeatures.stringTestRunnerFeatures);

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
