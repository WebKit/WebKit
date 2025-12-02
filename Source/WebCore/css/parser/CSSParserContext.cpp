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
#include "DocumentLoader.h"
#include "DocumentQuirks.h"
#include "DocumentSecurityOrigin.h"
#include "OriginAccessPatterns.h"
#include "Page.h"
#include "Settings.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

const CSSParserContext& strictCSSParserContext()
{
    static MainThreadNeverDestroyed<CSSParserContext> strictContext(HTMLStandardMode);
    return strictContext;
}

static void applyUASheetBehaviorsToContext(CSSParserContext& context)
{
    // FIXME: We should turn all of the features on from their WebCore Settings defaults.
    context.cssAppearanceBaseEnabled = true;
    context.cssTextTransformMathAutoEnabled = true;
    context.popoverAttributeEnabled = true;
    context.propertySettings.cssInputSecurityEnabled = true;
    context.propertySettings.supportHDRDisplayEnabled = true;
    context.propertySettings.viewTransitionsEnabled = true;
    context.propertySettings.cssFieldSizingEnabled = true;
    context.propertySettings.cssMathDepthEnabled = true;
#if HAVE(CORE_MATERIAL)
    context.propertySettings.useSystemAppearance = true;
#endif
    context.thumbAndTrackPseudoElementsEnabled = true;
    context.cssInternalAutoBaseParsingEnabled = true;
}

CSSParserContext::CSSParserContext(CSSParserMode mode, const URL& baseURL)
    : baseURL(baseURL)
    , mode(mode)
{
    if (isUASheetBehavior(mode))
        applyUASheetBehaviorsToContext(*this);

    StaticCSSValuePool::init();
}

CSSParserContext::CSSParserContext(const Document& document)
{
    *this = document.cssParserContext();
}

CSSParserContext::CSSParserContext(const Document& document, const URL& sheetBaseURL, ASCIILiteral charset)
    : baseURL { sheetBaseURL.isNull() ? document.baseURL() : sheetBaseURL }
    , charset { charset }
    , mode { document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode }
    , isHTMLDocument { document.isHTMLDocument() }
    , hasDocumentSecurityOrigin { sheetBaseURL.isNull() || document.protectedSecurityOrigin()->canRequest(baseURL, OriginAccessPatternsForWebProcess::singleton()) }
    , useSystemAppearance { document.settings().useSystemAppearance() }
    , counterStyleAtRuleImageSymbolsEnabled { document.settings().cssCounterStyleAtRuleImageSymbolsEnabled() }
    , springTimingFunctionEnabled { document.settings().springTimingFunctionEnabled() }
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    , cssTransformStyleSeparatedEnabled { document.settings().cssTransformStyleSeparatedEnabled() }
#endif
    , gridLanesEnabled { document.settings().gridLanesEnabled() }
    , cssAppearanceBaseEnabled { document.settings().cssAppearanceBaseEnabled() }
    , cssPaintingAPIEnabled { document.settings().cssPaintingAPIEnabled() }
    , cssShapeFunctionEnabled { document.settings().cssShapeFunctionEnabled() }
    , cssTextDecorationLineErrorValues { document.settings().cssTextDecorationLineErrorValues() }
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
    , contrastColorEnabled { document.settings().cssContrastColorEnabled() }
    , targetTextPseudoElementEnabled { document.settings().targetTextPseudoElementEnabled() }
    , cssProgressFunctionEnabled { document.settings().cssProgressFunctionEnabled() }
    , cssRandomFunctionEnabled { document.settings().cssRandomFunctionEnabled() }
    , cssTreeCountingFunctionsEnabled { document.settings().cssTreeCountingFunctionsEnabled() }
    , cssURLModifiersEnabled { document.settings().cssURLModifiersEnabled() }
    , cssURLIntegrityModifierEnabled { document.settings().cssURLIntegrityModifierEnabled() }
    , cssAxisRelativePositionKeywordsEnabled { document.settings().cssAxisRelativePositionKeywordsEnabled() }
    , cssDynamicRangeLimitMixEnabled { document.settings().cssDynamicRangeLimitMixEnabled() }
    , cssConstrainedDynamicRangeLimitEnabled { document.settings().cssConstrainedDynamicRangeLimitEnabled() }
    , cssTextTransformMathAutoEnabled { document.settings().cssTextTransformMathAutoEnabled() }
    , cssInternalAutoBaseParsingEnabled { document.settings().cssInternalAutoBaseParsingEnabled() }
    , webkitMediaTextTrackDisplayQuirkEnabled { document.quirks().needsWebKitMediaTextTrackDisplayQuirk() }
    , propertySettings { CSSPropertySettings { document.settings() } }
{
}

void add(Hasher& hasher, const CSSParserContext& context)
{
    uint32_t bits = context.isHTMLDocument                  << 0
        | context.hasDocumentSecurityOrigin                 << 1
        | static_cast<bool>(context.loadedFromOpaqueSource) << 2
        | context.useSystemAppearance                       << 3
        | context.springTimingFunctionEnabled               << 4
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
        | context.cssTransformStyleSeparatedEnabled         << 5
#endif
        | context.gridLanesEnabled                          << 6
        | context.cssAppearanceBaseEnabled                  << 7
        | context.cssPaintingAPIEnabled                     << 8
        | context.cssShapeFunctionEnabled                   << 9
        | context.cssBackgroundClipBorderAreaEnabled        << 10
        | context.cssWordBreakAutoPhraseEnabled             << 11
        | context.popoverAttributeEnabled                   << 12
        | context.sidewaysWritingModesEnabled               << 13
        | context.cssTextWrapPrettyEnabled                  << 14
        | context.thumbAndTrackPseudoElementsEnabled        << 15
#if ENABLE(SERVICE_CONTROLS)
        | context.imageControlsEnabled                      << 16
#endif
        | context.colorLayersEnabled                        << 17
        | context.contrastColorEnabled                      << 18
        | context.targetTextPseudoElementEnabled            << 19
        | context.cssProgressFunctionEnabled                << 20
        | context.cssRandomFunctionEnabled                  << 21
        | context.cssTreeCountingFunctionsEnabled           << 22
        | context.cssURLModifiersEnabled                    << 23
        | context.cssURLIntegrityModifierEnabled            << 24
        | context.cssAxisRelativePositionKeywordsEnabled    << 25
        | context.cssDynamicRangeLimitMixEnabled            << 26
        | context.cssConstrainedDynamicRangeLimitEnabled    << 27
        | context.cssTextDecorationLineErrorValues          << 28
        | context.cssTextTransformMathAutoEnabled           << 29
        | context.cssInternalAutoBaseParsingEnabled         << 30;
    add(hasher, context.baseURL, context.charset, context.propertySettings, context.mode, bits);
}

void CSSParserContext::setUASheetMode()
{
    mode = UASheetMode;
    applyUASheetBehaviorsToContext(*this);
}

} // namespace WebCore
