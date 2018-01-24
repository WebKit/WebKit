/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "TextEncoding.h"
#include "URL.h"
#include "URLHash.h"
#include <wtf/HashFunctions.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class Document;

// Must not grow beyond 3 bits, due to packing in StyleProperties.
enum CSSParserMode {
    HTMLStandardMode,
    HTMLQuirksMode,
    // HTML attributes are parsed in quirks mode but also allows internal properties and values.
    HTMLAttributeMode,
    // SVG attributes are parsed in quirks mode but rules differ slightly.
    SVGAttributeMode,
    // @viewport rules are parsed in standards mode but CSSOM modifications (via StylePropertySet)
    // must call parseViewportProperties so needs a special mode.
    CSSViewportRuleMode,
    // User agent stylesheets are parsed in standards mode but also allows internal properties and values.
    UASheetMode
};

inline bool isQuirksModeBehavior(CSSParserMode mode)
{
    return mode == HTMLQuirksMode || mode == HTMLAttributeMode;
}

inline bool isUASheetBehavior(CSSParserMode mode)
{
    return mode == UASheetMode;
}

inline bool isUnitLessValueParsingEnabledForMode(CSSParserMode mode)
{
    return mode == HTMLAttributeMode || mode == SVGAttributeMode;
}

inline bool isCSSViewportParsingEnabledForMode(CSSParserMode mode)
{
    return mode == CSSViewportRuleMode;
}

// FIXME-NEWPARSER: Next two functions should be removed eventually.
inline CSSParserMode strictToCSSParserMode(bool inStrictMode)
{
    return inStrictMode ? HTMLStandardMode : HTMLQuirksMode;
}

inline bool isStrictParserMode(CSSParserMode cssParserMode)
{
    return cssParserMode == UASheetMode || cssParserMode == HTMLStandardMode || cssParserMode == SVGAttributeMode;
}

struct CSSParserContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CSSParserContext(CSSParserMode, const URL& baseURL = URL());
    WEBCORE_EXPORT CSSParserContext(Document&, const URL& baseURL = URL(), const String& charset = emptyString());

    URL baseURL;
    String charset;
    CSSParserMode mode { HTMLStandardMode };
    bool isHTMLDocument { false };
    bool cssGridLayoutEnabled { false };
#if ENABLE(TEXT_AUTOSIZING)
    bool textAutosizingEnabled { false };
#endif
    bool needsSiteSpecificQuirks { false };
    bool enforcesCSSMIMETypeInNoQuirksMode { true };
    bool useLegacyBackgroundSizeShorthandBehavior { false };
    bool springTimingFunctionEnabled { false };
    bool constantPropertiesEnabled { false };
    bool conicGradientsEnabled { false };
    bool deferredCSSParserEnabled { false };
    bool allowNewLinesClamp { false };
    
    // This is only needed to support getMatchedCSSRules.
    bool hasDocumentSecurityOrigin { false };

    URL completeURL(const String& url) const
    {
        if (url.isNull())
            return URL();
        if (charset.isEmpty())
            return URL(baseURL, url);
        return URL(baseURL, url, TextEncoding(charset));
    }
};

bool operator==(const CSSParserContext&, const CSSParserContext&);
inline bool operator!=(const CSSParserContext& a, const CSSParserContext& b) { return !(a == b); }

WEBCORE_EXPORT const CSSParserContext& strictCSSParserContext();

struct CSSParserContextHash {
    static unsigned hash(const CSSParserContext& key)
    {
        auto hash = URLHash::hash(key.baseURL);
        if (!key.charset.isEmpty())
            hash ^= StringHash::hash(key.charset);
        unsigned bits = key.isHTMLDocument                  << 0
            & key.isHTMLDocument                            << 1
            & key.cssGridLayoutEnabled                      << 2
#if ENABLE(TEXT_AUTOSIZING)
            & key.textAutosizingEnabled                     << 3
#endif
            & key.needsSiteSpecificQuirks                   << 4
            & key.enforcesCSSMIMETypeInNoQuirksMode         << 5
            & key.useLegacyBackgroundSizeShorthandBehavior  << 6
            & key.springTimingFunctionEnabled               << 7
            & key.conicGradientsEnabled                     << 8
            & key.deferredCSSParserEnabled                  << 9
            & key.hasDocumentSecurityOrigin                 << 10
            & key.mode                                      << 11
            & key.allowNewLinesClamp                        << 12;
        hash ^= WTF::intHash(bits);
        return hash;
    }
    static bool equal(const CSSParserContext& a, const CSSParserContext& b)
    {
        return a == b;
    }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace WebCore

namespace WTF {
template<> struct HashTraits<WebCore::CSSParserContext> : GenericHashTraits<WebCore::CSSParserContext> {
    static void constructDeletedValue(WebCore::CSSParserContext& slot) { new (NotNull, &slot.baseURL) WebCore::URL(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::CSSParserContext& value) { return value.baseURL.isHashTableDeletedValue(); }
    static WebCore::CSSParserContext emptyValue() { return WebCore::CSSParserContext(WebCore::HTMLStandardMode); }
};

template<> struct DefaultHash<WebCore::CSSParserContext> {
    typedef WebCore::CSSParserContextHash Hash;
};
} // namespace WTF
