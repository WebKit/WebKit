/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#include "CSSParserMode.h"
#include "CSSPropertyNames.h"
#include "StyleRuleType.h"
#include <pal/text/TextEncoding.h>
#include <wtf/HashFunctions.h>
#include <wtf/Hasher.h>
#include <wtf/URL.h>

namespace WebCore {

class Document;

struct ResolvedURL;

struct CSSParserContext {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    URL baseURL;
    String charset;
    CSSParserMode mode { HTMLStandardMode };
    std::optional<StyleRuleType> enclosingRuleType;
    bool isHTMLDocument { false };

    // This is only needed to support getMatchedCSSRules.
    bool hasDocumentSecurityOrigin { false };

    bool isContentOpaque { false };
    bool useSystemAppearance { false };
    bool shouldIgnoreImportRules { false };

    // Settings, excluding those affecting properties.
    bool colorContrastEnabled { false };
    bool colorMixEnabled { false };
    bool constantPropertiesEnabled { false };
    bool counterStyleAtRuleImageSymbolsEnabled { false };
    bool cssColor4 { false };
    bool relativeColorSyntaxEnabled { false };
    bool springTimingFunctionEnabled { false };
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
    bool transformStyleOptimized3DEnabled { false };
#endif
    bool useLegacyBackgroundSizeShorthandBehavior { false };
    bool focusVisibleEnabled { false };
    bool hasPseudoClassEnabled { false };
    bool cascadeLayersEnabled { false };
    bool overflowClipEnabled { false };
    bool gradientPremultipliedAlphaInterpolationEnabled { false };
    bool gradientInterpolationColorSpacesEnabled { false };
    bool subgridEnabled { false };

    // Settings, those affecting properties.
    CSSPropertySettings propertySettings;

    CSSParserContext(CSSParserMode, const URL& baseURL = URL());
    WEBCORE_EXPORT CSSParserContext(const Document&, const URL& baseURL = URL(), const String& charset = emptyString());
    ResolvedURL completeURL(const String&) const;
};

bool operator==(const CSSParserContext&, const CSSParserContext&);
inline bool operator!=(const CSSParserContext& a, const CSSParserContext& b) { return !(a == b); }

void add(Hasher&, const CSSParserContext&);

WEBCORE_EXPORT const CSSParserContext& strictCSSParserContext();

struct CSSParserContextHash {
    static unsigned hash(const CSSParserContext& context) { return computeHash(context); }
    static bool equal(const CSSParserContext& a, const CSSParserContext& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::CSSParserContext> : GenericHashTraits<WebCore::CSSParserContext> {
    static void constructDeletedValue(WebCore::CSSParserContext& slot) { new (NotNull, &slot.baseURL) URL(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::CSSParserContext& value) { return value.baseURL.isHashTableDeletedValue(); }
    static WebCore::CSSParserContext emptyValue() { return WebCore::CSSParserContext(WebCore::HTMLStandardMode); }
};

template<> struct DefaultHash<WebCore::CSSParserContext> : WebCore::CSSParserContextHash { };

} // namespace WTF
