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
    context.cssMathDepthEnabled = true;
    context.propertySettings.cssMathDepthEnabled = true;
#if HAVE(CORE_MATERIAL)
    context.propertySettings.useSystemAppearance = true;
#endif
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
    : CSSParserContext(document.settings())
{
    baseURL = sheetBaseURL.isNull() ? document.baseURL() : sheetBaseURL;
    this->charset = charset;
    mode = document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode;
    isHTMLDocument = document.isHTMLDocument();
    hasDocumentSecurityOrigin = sheetBaseURL.isNull() || protect(document.securityOrigin())->canRequest(baseURL, OriginAccessPatternsForWebProcess::singleton());
    webkitMediaTextTrackDisplayQuirkEnabled = document.quirks().needsWebKitMediaTextTrackDisplayQuirk();
}

CSSParserContext::CSSParserContext(const Settings& settings)
    : mode { HTMLStandardMode }
    , useSystemAppearance { settings.useSystemAppearance() }
    , counterStyleAtRuleImageSymbolsEnabled { settings.cssCounterStyleAtRuleImageSymbolsEnabled() }
    , springTimingFunctionEnabled { settings.springTimingFunctionEnabled() }
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    , cssTransformStyleSeparatedEnabled { settings.cssTransformStyleSeparatedEnabled() }
#endif
    , gridLanesEnabled { settings.gridLanesEnabled() }
    , cssAppearanceBaseEnabled { settings.cssAppearanceBaseEnabled() }
    , cssPaintingAPIEnabled { settings.cssPaintingAPIEnabled() }
    , cssTextDecorationLineErrorValues { settings.cssTextDecorationLineErrorValues() }
    , cssWordBreakAutoPhraseEnabled { settings.cssWordBreakAutoPhraseEnabled() }
    , popoverAttributeEnabled { settings.popoverAttributeEnabled() }
    , sidewaysWritingModesEnabled { settings.sidewaysWritingModesEnabled() }
    , cssTextWrapPrettyEnabled { settings.cssTextWrapPrettyEnabled() }
#if ENABLE(SERVICE_CONTROLS)
    , imageControlsEnabled { settings.imageControlsEnabled() }
#endif
    , colorLayersEnabled { settings.cssColorLayersEnabled() }
    , targetTextPseudoElementEnabled { settings.targetTextPseudoElementEnabled() }
    , htmlEnhancedSelectPseudoElementsEnabled { settings.htmlEnhancedSelectPseudoElementsEnabled() }
    , cssRandomFunctionEnabled { settings.cssRandomFunctionEnabled() }
    , cssTreeCountingFunctionsEnabled { settings.cssTreeCountingFunctionsEnabled() }
    , cssURLModifiersEnabled { settings.cssURLModifiersEnabled() }
    , cssURLIntegrityModifierEnabled { settings.cssURLIntegrityModifierEnabled() }
    , cssAxisRelativePositionKeywordsEnabled { settings.cssAxisRelativePositionKeywordsEnabled() }
    , cssDynamicRangeLimitMixEnabled { settings.cssDynamicRangeLimitMixEnabled() }
    , cssConstrainedDynamicRangeLimitEnabled { settings.cssConstrainedDynamicRangeLimitEnabled() }
    , cssTextTransformMathAutoEnabled { settings.cssTextTransformMathAutoEnabled() }
    , cssInternalAutoBaseParsingEnabled { settings.cssInternalAutoBaseParsingEnabled() }
    , cssMathDepthEnabled { settings.cssMathDepthEnabled() }
    , openPseudoClassEnabled { settings.openPseudoClassEnabled() }
    , propertySettings { CSSPropertySettings { settings } }
{
}

void add(Hasher& hasher, const CSSParserContext& context)
{
    auto bits = WTF::packBools(
        context.isHTMLDocument,
        context.hasDocumentSecurityOrigin,
        static_cast<bool>(context.loadedFromOpaqueSource),
        context.useSystemAppearance,
        context.springTimingFunctionEnabled,
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
        context.cssTransformStyleSeparatedEnabled,
#endif
        context.gridLanesEnabled,
        context.cssAppearanceBaseEnabled,
        context.cssPaintingAPIEnabled,
        context.cssWordBreakAutoPhraseEnabled,
        context.popoverAttributeEnabled,
        context.sidewaysWritingModesEnabled,
        context.cssTextWrapPrettyEnabled,
#if ENABLE(SERVICE_CONTROLS)
        context.imageControlsEnabled,
#endif
        context.colorLayersEnabled,
        context.targetTextPseudoElementEnabled,
        context.cssRandomFunctionEnabled,
        context.cssTreeCountingFunctionsEnabled,
        context.cssURLModifiersEnabled,
        context.cssURLIntegrityModifierEnabled,
        context.cssAxisRelativePositionKeywordsEnabled,
        context.cssDynamicRangeLimitMixEnabled,
        context.cssConstrainedDynamicRangeLimitEnabled,
        context.cssTextDecorationLineErrorValues,
        context.cssTextTransformMathAutoEnabled,
        context.cssInternalAutoBaseParsingEnabled,
        context.cssMathDepthEnabled,
        context.htmlEnhancedSelectPseudoElementsEnabled,
        context.openPseudoClassEnabled
    );
    add(hasher, context.baseURL, context.charset, context.propertySettings, context.mode, bits);
}

void CSSParserContext::setUASheetMode()
{
    mode = UASheetMode;
    applyUASheetBehaviorsToContext(*this);
}

} // namespace WebCore
