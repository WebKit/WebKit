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

#pragma once

#include "CSSParserMode.h"
#include "StyleRuleType.h"
#include "TextEncoding.h"
#include <wtf/HashFunctions.h>
#include <wtf/Optional.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class Document;

struct CSSParserContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CSSParserContext(CSSParserMode, const URL& baseURL = URL());
    WEBCORE_EXPORT CSSParserContext(const Document&, const URL& baseURL = URL(), const String& charset = emptyString());

    URL baseURL;
    String charset;
    CSSParserMode mode { HTMLStandardMode };
    Optional<StyleRuleType> enclosingRuleType;
    bool isHTMLDocument { false };

    // This is only needed to support getMatchedCSSRules.
    bool hasDocumentSecurityOrigin { false };

    bool isContentOpaque { false };
    bool useSystemAppearance { false };

    // Settings.
    bool aspectRatioEnabled { false };
    bool colorFilterEnabled { false };
    bool colorMixEnabled { false };
    bool constantPropertiesEnabled { false };
    bool deferredCSSParserEnabled { false };
    bool enforcesCSSMIMETypeInNoQuirksMode { true };
    bool individualTransformPropertiesEnabled { false };
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    bool legacyOverflowScrollingTouchEnabled { false };
#endif
    bool overscrollBehaviorEnabled { false };
    bool relativeColorSyntaxEnabled { false };
    bool scrollBehaviorEnabled { false };
    bool springTimingFunctionEnabled { false };
#if ENABLE(TEXT_AUTOSIZING)
    bool textAutosizingEnabled { false };
#endif
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
    bool transformStyleOptimized3DEnabled { false };
#endif
    bool useLegacyBackgroundSizeShorthandBehavior { false };
    bool focusVisibleEnabled { false };

    // RuntimeEnabledFeatures.
#if ENABLE(ATTACHMENT_ELEMENT)
    bool attachmentEnabled { false };
#endif

    URL completeURL(const String& url) const;
};

bool operator==(const CSSParserContext&, const CSSParserContext&);
inline bool operator!=(const CSSParserContext& a, const CSSParserContext& b) { return !(a == b); }

WEBCORE_EXPORT const CSSParserContext& strictCSSParserContext();

struct CSSParserContextHash {
    static unsigned hash(const CSSParserContext& key)
    {
        // FIXME: Convert this to use WTF::Hasher.

        unsigned hash = 0;
        if (!key.baseURL.isNull())
            hash ^= WTF::URLHash::hash(key.baseURL);
        if (!key.charset.isEmpty())
            hash ^= StringHash::hash(key.charset);
        
        unsigned bits = key.isHTMLDocument                  << 0
            & key.hasDocumentSecurityOrigin                 << 1
            & key.isContentOpaque                           << 2
            & key.useSystemAppearance                       << 3
            & key.aspectRatioEnabled                        << 4
            & key.colorFilterEnabled                        << 5
            & key.colorMixEnabled                           << 6
            & key.constantPropertiesEnabled                 << 7
            & key.deferredCSSParserEnabled                  << 8
            & key.enforcesCSSMIMETypeInNoQuirksMode         << 9
            & key.individualTransformPropertiesEnabled      << 10
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
            & key.legacyOverflowScrollingTouchEnabled       << 11
#endif
            & key.overscrollBehaviorEnabled                 << 12
            & key.relativeColorSyntaxEnabled                << 13
            & key.scrollBehaviorEnabled                     << 14
            & key.springTimingFunctionEnabled               << 15
#if ENABLE(TEXT_AUTOSIZING)
            & key.textAutosizingEnabled                     << 16
#endif
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
            & key.transformStyleOptimized3DEnabled          << 17
#endif
            & key.useLegacyBackgroundSizeShorthandBehavior  << 18
            & key.focusVisibleEnabled                       << 19
#if ENABLE(ATTACHMENT_ELEMENT)
            & key.attachmentEnabled                         << 20
#endif
            & key.mode                                      << 21; // Keep this last.
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
    static void constructDeletedValue(WebCore::CSSParserContext& slot) { new (NotNull, &slot.baseURL) URL(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::CSSParserContext& value) { return value.baseURL.isHashTableDeletedValue(); }
    static WebCore::CSSParserContext emptyValue() { return WebCore::CSSParserContext(WebCore::HTMLStandardMode); }
};

template<> struct DefaultHash<WebCore::CSSParserContext> : WebCore::CSSParserContextHash { };

} // namespace WTF
