/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#include "CSSParserContext.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Page.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

const CSSParserContext& strictCSSParserContext()
{
    static MainThreadNeverDestroyed<CSSParserContext> strictContext(HTMLStandardMode);
    return strictContext;
}

CSSParserContext::CSSParserContext(CSSParserMode mode, const URL& baseURL)
    : baseURL(baseURL)
    , mode(mode)
{
}

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
static bool shouldEnableLegacyOverflowScrollingTouch(const Document& document)
{
    // The legacy -webkit-overflow-scrolling: touch behavior may have been disabled through the website policy,
    // in that case we want to disable the legacy behavior regardless of what the setting says.
    if (auto* loader = document.loader()) {
        if (loader->legacyOverflowScrollingTouchPolicy() == LegacyOverflowScrollingTouchPolicy::Disable)
            return false;
    }
    return document.settings().legacyOverflowScrollingTouchEnabled();
}
#endif

CSSParserContext::CSSParserContext(const Document& document, const URL& sheetBaseURL, const String& charset)
    : baseURL { sheetBaseURL.isNull() ? document.baseURL() : sheetBaseURL }
    , charset { charset }
    , mode { document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode }
    , isHTMLDocument { document.isHTMLDocument() }
    , hasDocumentSecurityOrigin { sheetBaseURL.isNull() || document.securityOrigin().canRequest(baseURL) }
    , useSystemAppearance { document.page() ? document.page()->useSystemAppearance() : false }
    , aspectRatioEnabled { document.settings().aspectRatioEnabled() }
    , colorFilterEnabled { document.settings().colorFilterEnabled() }
    , colorMixEnabled { document.settings().cssColorMixEnabled() }
    , constantPropertiesEnabled { document.settings().constantPropertiesEnabled() }
    , deferredCSSParserEnabled { document.settings().deferredCSSParserEnabled() }
    , enforcesCSSMIMETypeInNoQuirksMode { document.settings().enforceCSSMIMETypeInNoQuirksMode() }
    , individualTransformPropertiesEnabled { document.settings().cssIndividualTransformPropertiesEnabled() }
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    , legacyOverflowScrollingTouchEnabled { shouldEnableLegacyOverflowScrollingTouch(document) }
#endif
    , overscrollBehaviorEnabled { document.settings().overscrollBehaviorEnabled() }
    , relativeColorSyntaxEnabled { document.settings().cssRelativeColorSyntaxEnabled() }
    , scrollBehaviorEnabled { document.settings().CSSOMViewSmoothScrollingEnabled() }
    , springTimingFunctionEnabled { document.settings().springTimingFunctionEnabled() }
#if ENABLE(TEXT_AUTOSIZING)
    , textAutosizingEnabled { document.settings().textAutosizingEnabled() }
#endif
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
    , transformStyleOptimized3DEnabled { document.settings().cssTransformStyleOptimized3DEnabled() }
#endif
    , useLegacyBackgroundSizeShorthandBehavior { document.settings().useLegacyBackgroundSizeShorthandBehavior() }
    , focusVisibleEnabled { document.settings().focusVisibleEnabled() }
#if ENABLE(ATTACHMENT_ELEMENT)
    , attachmentEnabled { RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled() }
#endif
{
}

bool operator==(const CSSParserContext& a, const CSSParserContext& b)
{
    return a.baseURL == b.baseURL
        && a.charset == b.charset
        && a.mode == b.mode
        && a.enclosingRuleType == b.enclosingRuleType
        && a.isHTMLDocument == b.isHTMLDocument
        && a.hasDocumentSecurityOrigin == b.hasDocumentSecurityOrigin
        && a.isContentOpaque == b.isContentOpaque
        && a.useSystemAppearance == b.useSystemAppearance
        && a.aspectRatioEnabled == b.aspectRatioEnabled
        && a.colorFilterEnabled == b.colorFilterEnabled
        && a.colorMixEnabled == b.colorMixEnabled
        && a.constantPropertiesEnabled == b.constantPropertiesEnabled
        && a.deferredCSSParserEnabled == b.deferredCSSParserEnabled
        && a.enforcesCSSMIMETypeInNoQuirksMode == b.enforcesCSSMIMETypeInNoQuirksMode
        && a.individualTransformPropertiesEnabled == b.individualTransformPropertiesEnabled
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
        && a.legacyOverflowScrollingTouchEnabled == b.legacyOverflowScrollingTouchEnabled
#endif
        && a.overscrollBehaviorEnabled == b.overscrollBehaviorEnabled
        && a.relativeColorSyntaxEnabled == b.relativeColorSyntaxEnabled
        && a.scrollBehaviorEnabled == b.scrollBehaviorEnabled
        && a.springTimingFunctionEnabled == b.springTimingFunctionEnabled
#if ENABLE(TEXT_AUTOSIZING)
        && a.textAutosizingEnabled == b.textAutosizingEnabled
#endif
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
        && a.transformStyleOptimized3DEnabled == b.transformStyleOptimized3DEnabled
#endif
        && a.useLegacyBackgroundSizeShorthandBehavior == b.useLegacyBackgroundSizeShorthandBehavior
        && a.focusVisibleEnabled == b.focusVisibleEnabled
#if ENABLE(ATTACHMENT_ELEMENT)
        && a.attachmentEnabled == b.attachmentEnabled
#endif
    ;
}

URL CSSParserContext::completeURL(const String& url) const
{
    auto completedURL = [&] {
        if (url.isNull())
            return URL();
        if (charset.isEmpty())
            return URL(baseURL, url);
        TextEncoding encoding(charset);
        auto& encodingForURLParsing = encoding.encodingForFormSubmissionOrURLParsing();
        return URL(baseURL, url, encodingForURLParsing == UTF8Encoding() ? nullptr : &encodingForURLParsing);
    }();

    if (mode == WebVTTMode && !completedURL.protocolIsData())
        return URL();

    return completedURL;
}

}
