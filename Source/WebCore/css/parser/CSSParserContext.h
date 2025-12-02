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

#pragma once

#include <WebCore/CSSParserMode.h>
#include <WebCore/CSSPropertyNames.h>
#include <WebCore/LoadedFromOpaqueSource.h>
#include <WebCore/StyleRuleType.h>
#include <pal/text/TextEncoding.h>
#include <wtf/HashFunctions.h>
#include <wtf/Hasher.h>
#include <wtf/URL.h>

namespace WebCore {

class Document;

struct CSSParserContext {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(CSSParserContext);

    URL baseURL;
    ASCIILiteral charset;
    CSSParserMode mode { HTMLStandardMode };
    std::optional<StyleRuleType> enclosingRuleType;
    bool isHTMLDocument : 1 { false };

    // This is only needed to support getMatchedCSSRules.
    bool hasDocumentSecurityOrigin : 1 { false };

    LoadedFromOpaqueSource loadedFromOpaqueSource : 1 { LoadedFromOpaqueSource::No };
    bool useSystemAppearance : 1 { false };
    bool shouldIgnoreImportRules : 1 { false };

    // Settings, excluding those affecting properties.
    bool counterStyleAtRuleImageSymbolsEnabled : 1 { false };
    bool springTimingFunctionEnabled : 1 { false };
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    bool cssTransformStyleSeparatedEnabled : 1 { false };
#endif
    bool gridLanesEnabled : 1 { false };
    bool cssAppearanceBaseEnabled : 1 { false };
    bool cssPaintingAPIEnabled : 1 { false };
    bool cssShapeFunctionEnabled : 1 { false };
    bool cssTextDecorationLineErrorValues : 1 { false };
    bool cssBackgroundClipBorderAreaEnabled : 1 { false };
    bool cssWordBreakAutoPhraseEnabled : 1 { false };
    bool popoverAttributeEnabled : 1 { false };
    bool sidewaysWritingModesEnabled : 1 { false };
    bool cssTextWrapPrettyEnabled : 1 { true };
    bool thumbAndTrackPseudoElementsEnabled : 1 { false };
#if ENABLE(SERVICE_CONTROLS)
    bool imageControlsEnabled : 1 { false };
#endif
    bool colorLayersEnabled : 1 { false };
    bool contrastColorEnabled : 1 { false };
    bool targetTextPseudoElementEnabled : 1 { false };
    bool cssProgressFunctionEnabled : 1 { false };
    bool cssRandomFunctionEnabled : 1 { false };
    bool cssTreeCountingFunctionsEnabled : 1 { false };
    bool cssURLModifiersEnabled : 1 { false };
    bool cssURLIntegrityModifierEnabled : 1 { false };
    bool cssAxisRelativePositionKeywordsEnabled : 1 { false };
    bool cssDynamicRangeLimitMixEnabled : 1 { false };
    bool cssConstrainedDynamicRangeLimitEnabled : 1 { false };
    bool cssTextTransformMathAutoEnabled : 1 { false };
    bool cssInternalAutoBaseParsingEnabled : 1 { false };
    bool webkitMediaTextTrackDisplayQuirkEnabled : 1 { false };

    // Settings, those affecting properties.
    CSSPropertySettings propertySettings;

    CSSParserContext(CSSParserMode, const URL& baseURL = URL());
    WEBCORE_EXPORT CSSParserContext(const Document&);
    CSSParserContext(const Document&, const URL& baseURL, ASCIILiteral charset = ""_s);

    void setUASheetMode();

    bool operator==(const CSSParserContext&) const = default;
};

void add(Hasher&, const CSSParserContext&);

WEBCORE_EXPORT const CSSParserContext& strictCSSParserContext();

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::CSSParserContext> : GenericHashTraits<WebCore::CSSParserContext> {
    static void constructDeletedValue(WebCore::CSSParserContext& slot) { new (NotNull, &slot.baseURL) URL(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::CSSParserContext& value) { return value.baseURL.isHashTableDeletedValue(); }
    static WebCore::CSSParserContext emptyValue() { return WebCore::CSSParserContext(WebCore::HTMLStandardMode); }
};

} // namespace WTF
