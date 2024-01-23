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
#include "Document.h"
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
        colorMixEnabled = true;
        focusVisibleEnabled = true;
        lightDarkEnabled = true;
        popoverAttributeEnabled = true;
        propertySettings.cssContainmentEnabled = true;
        propertySettings.cssInputSecurityEnabled = true;
        propertySettings.cssCounterStyleAtRulesEnabled = true;
        propertySettings.viewTransitionsEnabled = true;
        thumbAndTrackPseudoElementsEnabled = true;
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
        transformStyleOptimized3DEnabled = true;
#endif
    }

    StaticCSSValuePool::init();
}

CSSParserContext::CSSParserContext(const Document& document, const URL& sheetBaseURL, const String& charset)
    : baseURL { sheetBaseURL.isNull() ? document.baseURL() : sheetBaseURL }
    , charset { charset }
    , mode { document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode }
    , isHTMLDocument { document.isHTMLDocument() }
    , hasDocumentSecurityOrigin { sheetBaseURL.isNull() || document.securityOrigin().canRequest(baseURL, OriginAccessPatternsForWebProcess::singleton()) }
    , useSystemAppearance { document.page() ? document.page()->useSystemAppearance() : false }
    , colorContrastEnabled { document.settings().cssColorContrastEnabled() }
    , colorMixEnabled { document.settings().cssColorMixEnabled() }
    , constantPropertiesEnabled { document.settings().constantPropertiesEnabled() }
    , counterStyleAtRuleImageSymbolsEnabled { document.settings().cssCounterStyleAtRuleImageSymbolsEnabled() }
    , relativeColorSyntaxEnabled { document.settings().cssRelativeColorSyntaxEnabled() }
    , springTimingFunctionEnabled { document.settings().springTimingFunctionEnabled() }
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
    , transformStyleOptimized3DEnabled { document.settings().cssTransformStyleOptimized3DEnabled() }
#endif
    , useLegacyBackgroundSizeShorthandBehavior { document.settings().useLegacyBackgroundSizeShorthandBehavior() }
    , focusVisibleEnabled { document.settings().focusVisibleEnabled() }
    , hasPseudoClassEnabled { document.settings().hasPseudoClassEnabled() }
    , cascadeLayersEnabled { document.settings().cssCascadeLayersEnabled() }
    , gradientPremultipliedAlphaInterpolationEnabled { document.settings().cssGradientPremultipliedAlphaInterpolationEnabled() }
    , gradientInterpolationColorSpacesEnabled { document.settings().cssGradientInterpolationColorSpacesEnabled() }
    , subgridEnabled { document.settings().subgridEnabled() }
    , masonryEnabled { document.settings().masonryEnabled() }
    , cssNestingEnabled { document.settings().cssNestingEnabled() }
#if ENABLE(CSS_PAINTING_API)
    , cssPaintingAPIEnabled { document.settings().cssPaintingAPIEnabled() }
#endif
    , cssScopeAtRuleEnabled { document.settings().cssScopeAtRuleEnabled() }
    , cssStartingStyleAtRuleEnabled { document.settings().cssStartingStyleAtRuleEnabled() }
    , cssTextUnderlinePositionLeftRightEnabled { document.settings().cssTextUnderlinePositionLeftRightEnabled() }
    , cssWordBreakAutoPhraseEnabled { document.settings().cssWordBreakAutoPhraseEnabled() }
    , popoverAttributeEnabled { document.settings().popoverAttributeEnabled() }
    , sidewaysWritingModesEnabled { document.settings().sidewaysWritingModesEnabled() }
    , cssTextWrapPrettyEnabled { document.settings().cssTextWrapPrettyEnabled() }
    , highlightAPIEnabled { document.settings().highlightAPIEnabled() }
    , grammarAndSpellingPseudoElementsEnabled { document.settings().grammarAndSpellingPseudoElementsEnabled() }
    , customStateSetEnabled { document.settings().customStateSetEnabled() }
    , thumbAndTrackPseudoElementsEnabled { document.settings().thumbAndTrackPseudoElementsEnabled() }
#if ENABLE(SERVICE_CONTROLS)
    , imageControlsEnabled { document.settings().imageControlsEnabled() }
#endif
    , lightDarkEnabled { document.settings().cssLightDarkEnabled() }
    , propertySettings { CSSPropertySettings { document.settings() } }
{
}

void add(Hasher& hasher, const CSSParserContext& context)
{
    uint64_t bits = context.isHTMLDocument                  << 0
        | context.hasDocumentSecurityOrigin                 << 1
        | context.isContentOpaque                           << 2
        | context.useSystemAppearance                       << 3
        | context.colorContrastEnabled                      << 4
        | context.colorMixEnabled                           << 5
        | context.constantPropertiesEnabled                 << 6
        | context.relativeColorSyntaxEnabled                << 7
        | context.springTimingFunctionEnabled               << 8
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
        | context.transformStyleOptimized3DEnabled          << 9
#endif
        | context.useLegacyBackgroundSizeShorthandBehavior  << 10
        | context.focusVisibleEnabled                       << 11
        | context.hasPseudoClassEnabled                     << 12
        | context.cascadeLayersEnabled                      << 13
        | context.gradientPremultipliedAlphaInterpolationEnabled << 14
        | context.gradientInterpolationColorSpacesEnabled   << 15
        | context.subgridEnabled                            << 16
        | context.masonryEnabled                            << 17
        | context.cssNestingEnabled                         << 18
        | context.cssPaintingAPIEnabled                     << 19
        | context.cssScopeAtRuleEnabled                     << 20
        | context.cssTextUnderlinePositionLeftRightEnabled  << 21
        | context.cssWordBreakAutoPhraseEnabled             << 22
        | context.popoverAttributeEnabled                   << 23
        | context.sidewaysWritingModesEnabled               << 24
        | context.cssTextWrapPrettyEnabled                  << 25
        | context.highlightAPIEnabled                       << 26
        | context.grammarAndSpellingPseudoElementsEnabled   << 27
        | context.customStateSetEnabled                     << 28
        | context.thumbAndTrackPseudoElementsEnabled        << 29
#if ENABLE(SERVICE_CONTROLS)
        | context.imageControlsEnabled                      << 30
#endif
        | context.lightDarkEnabled                          << 31
        | (uint64_t)context.mode                            << 32; // This is multiple bits, so keep it last.
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

}
