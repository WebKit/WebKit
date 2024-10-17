/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

#include "CSSPropertyNames.h"
#include "CSSValuePool.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "OriginAccessPatterns.h"
#include "Page.h"
#include "Settings.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// https://drafts.csswg.org/css-values/#url-local-url-flag
bool ResolvedURL::isLocalURL() const
{
    return specifiedURLString.startsWith('#');
}

const CSSParserContext& strictCSSParserContext()
{
    static MainThreadNeverDestroyed<CSSParserContext> strictContext(HTMLStandardMode);
    return strictContext;
}

CSSParserContext::CSSParserContext(CSSParserMode mode, const URL& baseURL)
    : baseURL(baseURL)
    , mode(mode)
{
    // FIXME: We should turn all of the features on from their WebCore Settings defaults.
    if (isUASheetBehavior(mode)) {
        cssAppearanceBaseEnabled = true;
        cssTextUnderlinePositionLeftRightEnabled = true;
        lightDarkEnabled = true;
        popoverAttributeEnabled = true;
        propertySettings.cssInputSecurityEnabled = true;
        propertySettings.cssCounterStyleAtRulesEnabled = true;
        propertySettings.viewTransitionsEnabled = true;
        thumbAndTrackPseudoElementsEnabled = true;
    }

    StaticCSSValuePool::init();
}

CSSParserContext::CSSParserContext(const Document& document)
{
    *this = document.cssParserContext();
}

CSSParserContext::CSSParserContext(const Document& document, const URL& sheetBaseURL, const String& charset)
    : baseURL { sheetBaseURL.isNull() ? document.baseURL() : sheetBaseURL }
    , charset { charset }
    , mode { document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode }
    , isHTMLDocument { document.isHTMLDocument() }
    , hasDocumentSecurityOrigin { sheetBaseURL.isNull() || document.protectedSecurityOrigin()->canRequest(baseURL, OriginAccessPatternsForWebProcess::singleton()) }
    , useSystemAppearance { document.page() ? document.page()->useSystemAppearance() : false }
    , counterStyleAtRuleImageSymbolsEnabled { document.settings().cssCounterStyleAtRuleImageSymbolsEnabled() }
    , springTimingFunctionEnabled { document.settings().springTimingFunctionEnabled() }
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    , cssTransformStyleSeparatedEnabled { document.settings().cssTransformStyleSeparatedEnabled() }
#endif
    , masonryEnabled { document.settings().masonryEnabled() }
    , cssAppearanceBaseEnabled { document.settings().cssAppearanceBaseEnabled() }
    , cssNestingEnabled { document.settings().cssNestingEnabled() }
    , cssPaintingAPIEnabled { document.settings().cssPaintingAPIEnabled() }
    , cssScopeAtRuleEnabled { document.settings().cssScopeAtRuleEnabled() }
    , cssShapeFunctionEnabled { document.settings().cssShapeFunctionEnabled() }
    , cssStartingStyleAtRuleEnabled { document.settings().cssStartingStyleAtRuleEnabled() }
    , cssStyleQueriesEnabled { document.settings().cssStyleQueriesEnabled() }
    , cssTextUnderlinePositionLeftRightEnabled { document.settings().cssTextUnderlinePositionLeftRightEnabled() }
    , cssBackgroundClipBorderAreaEnabled  { document.settings().cssBackgroundClipBorderAreaEnabled() }
    , cssWordBreakAutoPhraseEnabled { document.settings().cssWordBreakAutoPhraseEnabled() }
    , popoverAttributeEnabled { document.settings().popoverAttributeEnabled() }
    , sidewaysWritingModesEnabled { document.settings().sidewaysWritingModesEnabled() }
    , cssTextWrapPrettyEnabled { document.settings().cssTextWrapPrettyEnabled() }
    , thumbAndTrackPseudoElementsEnabled { document.settings().thumbAndTrackPseudoElementsEnabled() }
#if ENABLE(SERVICE_CONTROLS)
    , imageControlsEnabled { document.settings().imageControlsEnabled() }
#endif
    , colorLayersEnabled { document.settings().cssColorLayersEnabled() }
    , lightDarkEnabled { document.settings().cssLightDarkEnabled() }
    , contrastColorEnabled { document.settings().cssContrastColorEnabled() }
    , targetTextPseudoElementEnabled { document.settings().targetTextPseudoElementEnabled() }
    , viewTransitionTypesEnabled { document.settings().viewTransitionsEnabled() && document.settings().viewTransitionTypesEnabled() }
    , cssProgressFunctionEnabled { document.settings().cssProgressFunctionEnabled() }
    , propertySettings { CSSPropertySettings { document.settings() } }
{
}

void add(Hasher& hasher, const CSSParserContext& context)
{
    uint32_t bits = context.isHTMLDocument                  << 0
        | context.hasDocumentSecurityOrigin                 << 1
        | context.isContentOpaque                           << 2
        | context.useSystemAppearance                       << 3
        | context.springTimingFunctionEnabled               << 4
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
        | context.cssTransformStyleSeparatedEnabled         << 5
#endif
        | context.masonryEnabled                            << 6
        | context.cssAppearanceBaseEnabled                  << 7
        | context.cssNestingEnabled                         << 8
        | context.cssPaintingAPIEnabled                     << 9
        | context.cssScopeAtRuleEnabled                     << 10
        | context.cssShapeFunctionEnabled                   << 11
        | context.cssTextUnderlinePositionLeftRightEnabled  << 12
        | context.cssBackgroundClipBorderAreaEnabled        << 13
        | context.cssWordBreakAutoPhraseEnabled             << 14
        | context.popoverAttributeEnabled                   << 15
        | context.sidewaysWritingModesEnabled               << 16
        | context.cssTextWrapPrettyEnabled                  << 17
        | context.thumbAndTrackPseudoElementsEnabled        << 18
#if ENABLE(SERVICE_CONTROLS)
        | context.imageControlsEnabled                      << 19
#endif
        | context.colorLayersEnabled                        << 20
        | context.lightDarkEnabled                          << 21
        | context.contrastColorEnabled                      << 22
        | context.targetTextPseudoElementEnabled            << 23
        | context.viewTransitionTypesEnabled                << 24
        | context.cssProgressFunctionEnabled                << 25
        | (uint32_t)context.mode                            << 26; // This is multiple bits, so keep it last.
    add(hasher, context.baseURL, context.charset, context.propertySettings, bits);
}

ResolvedURL CSSParserContext::completeURL(const String& string) const
{
    auto result = [&] () -> ResolvedURL {
        // See also Document::completeURL(const String&), but note that CSS always uses UTF-8 for URLs
        if (string.isNull())
            return { };

        if (CSSValue::isCSSLocalURL(string))
            return { string, URL { string } };

        return { string, { baseURL, string } };
    }();

    if (mode == WebVTTMode && !result.resolvedURL.protocolIsData())
        return { };

    return result;
}

bool mayDependOnBaseURL(const ResolvedURL& resolved)
{
    if (resolved.specifiedURLString.isEmpty())
        return false;

    if (CSSValue::isCSSLocalURL(resolved.specifiedURLString))
        return false;

    if (protocolIs(resolved.specifiedURLString, "data"_s))
        return false;

    return true;
}

}
