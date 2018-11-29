/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "TextEncoding.h"
#include "URL.h"
#include "URLHash.h"
#include <wtf/HashFunctions.h>
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
    bool isHTMLDocument { false };
#if ENABLE(TEXT_AUTOSIZING)
    bool textAutosizingEnabled { false };
#endif
    bool needsSiteSpecificQuirks { false };
    bool enforcesCSSMIMETypeInNoQuirksMode { true };
    bool useLegacyBackgroundSizeShorthandBehavior { false };
    bool springTimingFunctionEnabled { false };
    bool constantPropertiesEnabled { false };
    bool colorFilterEnabled { false };
#if ENABLE(ATTACHMENT_ELEMENT)
    bool attachmentEnabled { false };
#endif
    bool deferredCSSParserEnabled { false };
    
    // This is only needed to support getMatchedCSSRules.
    bool hasDocumentSecurityOrigin { false };
    
    bool useSystemAppearance { false };

    URL completeURL(const String& url) const
    {
        if (url.isNull())
            return URL();
        if (charset.isEmpty())
            return URL(baseURL, url);
        TextEncoding encoding(charset);
        auto& encodingForURLParsing = encoding.encodingForFormSubmissionOrURLParsing();
        return URL(baseURL, url, encodingForURLParsing == UTF8Encoding() ? nullptr : &encodingForURLParsing);
    }

    bool isContentOpaque { false };
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
#if ENABLE(TEXT_AUTOSIZING)
            & key.textAutosizingEnabled                     << 1
#endif
            & key.needsSiteSpecificQuirks                   << 2
            & key.enforcesCSSMIMETypeInNoQuirksMode         << 3
            & key.useLegacyBackgroundSizeShorthandBehavior  << 4
            & key.springTimingFunctionEnabled               << 5
            & key.constantPropertiesEnabled                 << 6
            & key.colorFilterEnabled                        << 7
            & key.deferredCSSParserEnabled                  << 8
            & key.hasDocumentSecurityOrigin                 << 9
            & key.useSystemAppearance                       << 10
#if ENABLE(ATTACHMENT_ELEMENT)
            & key.attachmentEnabled                         << 11
#endif
            & key.mode                                      << 12; // Keep this last.
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
