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

struct TestOptions {
    // FIXME: Remove these and replace with access to TestFeatures set.
    // Web Preferences
    bool allowCrossOriginSubresourcesToAskForCredentials { false };
    bool allowTopNavigationToDataURLs { true };
    bool enableAcceleratedDrawing { false };
    bool enableAttachmentElement { false };
    bool enableBackForwardCache { false };
    bool enableColorFilter { false };
    bool enableInspectorAdditions { false };
    bool enableIntersectionObserver { false };
    bool enableKeygenElement { false };
    bool enableMenuItemElement { false };
    bool enableModernMediaControls { true };

    // FIXME: Remove these and replace with access to TestFeatures set.
    // Internal Features
    bool enableCSSLogical { false };
    bool enableLineHeightUnits { false };
    bool enableSelectionAcrossShadowBoundaries { true };
    bool layoutFormattingContextIntegrationEnabled { true };

    // FIXME: Remove these and replace with access to TestFeatures set.
    // Experimental Features
    bool adClickAttributionEnabled { false };
    bool enableAspectRatioOfImgFromWidthAndHeight { false };
    bool enableAsyncClipboardAPI { false };
    bool enableCSSOMViewSmoothScrolling { false };
    bool enableContactPickerAPI { false };
    bool enableCoreMathML { false };
    bool enableRequestIdleCallback { false };
    bool enableResizeObserver { false };
    bool enableWebGPU { false };

    // Test Runner Specific Features
    bool dumpJSConsoleLogInStdErr { false };
    bool enableDragDestinationActionLoad { false };
    bool enableWebSQL { true };
    bool layerBackedWebView { false };
    bool useEphemeralSession { false };
    std::string additionalSupportedImageTypes;
    std::string jscOptions;

    explicit TestOptions(TestFeatures);
    bool webViewIsCompatibleWithOptions(const TestOptions&) const;
    
    static const std::unordered_map<std::string, TestHeaderKeyType>& keyTypeMapping();
};

}

