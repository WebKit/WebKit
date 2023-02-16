/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIObject.h"
#include <WebCore/UserContentURLPattern.h>
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSError;
OBJC_CLASS NSSet;
OBJC_CLASS NSString;
OBJC_CLASS _WKWebExtensionMatchPattern;

namespace WebKit {

class WebExtensionMatchPattern : public API::ObjectImpl<API::Object::Type::WebExtensionMatchPattern> {
    WTF_MAKE_NONCOPYABLE(WebExtensionMatchPattern);

public:
    template<typename... Args>
    static RefPtr<WebExtensionMatchPattern> create(Args&&... args)
    {
        auto result = adoptRef(new WebExtensionMatchPattern(std::forward<Args>(args)...));
        return result && result->isValid() ? WTFMove(result) : nullptr;
    }

    static RefPtr<WebExtensionMatchPattern> getOrCreate(const String& pattern);
    static RefPtr<WebExtensionMatchPattern> getOrCreate(const String& scheme, const String& host, const String& path);

    static Ref<WebExtensionMatchPattern> allURLsMatchPattern();
    static Ref<WebExtensionMatchPattern> allHostsAndSchemesMatchPattern();

    static bool patternsMatchAllHosts(HashSet<Ref<WebExtensionMatchPattern>>&);

    explicit WebExtensionMatchPattern() { }
    explicit WebExtensionMatchPattern(const String& pattern, NSError **outError = nullptr);
    explicit WebExtensionMatchPattern(const String& scheme, const String& host, const String& path, NSError **outError = nullptr);

    ~WebExtensionMatchPattern() { }

    using URLSchemeSet = HashSet<String>;
    using MatchPatternSet = HashSet<Ref<WebExtensionMatchPattern>>;

    enum class Options : uint8_t {
        IgnoreSchemes        = 1 << 0, // Ignore the scheme component when matching.
        IgnorePaths          = 1 << 1, // Ignore the path component when matching.
        MatchBidirectionally = 1 << 2, // Match two patterns in either direction (A matches B, or B matches A). Invalid for matching URLs.
    };

    static URLSchemeSet& validSchemes();
    static URLSchemeSet& supportedSchemes();

    static void registerCustomURLScheme(String);

    bool operator==(const WebExtensionMatchPattern&) const;
    bool operator!=(const WebExtensionMatchPattern& other) const { return !(this == &other); }

    bool isValid() const { return m_valid; }
    bool isSupported() const;

    String scheme() const;
    String host() const;
    String path() const;

    bool matchesAllURLs() const { return m_matchesAllURLs; }
    bool matchesAllHosts() const;

    bool matchesURL(const URL&, OptionSet<Options> = { }) const;
    bool matchesPattern(const WebExtensionMatchPattern&, OptionSet<Options> = { }) const;

    String string() const { return stringWithScheme(nullString()); }
    NSArray *expandedStrings() const;

    const WebCore::UserContentURLPattern& pattern() const { return m_pattern; }

    unsigned hash() const { return m_hash; }

#ifdef __OBJC__
    _WKWebExtensionMatchPattern *wrapper() const { return (_WKWebExtensionMatchPattern *)API::ObjectImpl<API::Object::Type::WebExtensionMatchPattern>::wrapper(); }
#endif

private:
    String stringWithScheme(const String& differentScheme) const;

    static bool isValidScheme(String);

    bool schemeMatches(const WebExtensionMatchPattern&, OptionSet<Options> = { }) const;
    bool hostMatches(const WebExtensionMatchPattern&, OptionSet<Options> = { }) const;
    bool pathMatches(const WebExtensionMatchPattern&, OptionSet<Options> = { }) const;

    WebCore::UserContentURLPattern m_pattern;
    bool m_matchesAllURLs { false };
    bool m_valid { false };
    unsigned m_hash { 0 };
};

using MatchPatternSet = HashSet<Ref<WebExtensionMatchPattern>>;

NSSet *toAPI(MatchPatternSet&);
HashSet<String> toStrings(const MatchPatternSet&);
MatchPatternSet toPatterns(HashSet<String>&);

} // namespace WebKit

namespace WTF {

struct WebExtensionMatchPatternHash {
    static unsigned hash(const WebKit::WebExtensionMatchPattern& pattern) { return pattern.hash(); }
    static bool equal(const WebKit::WebExtensionMatchPattern& a, const WebKit::WebExtensionMatchPattern& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<WebKit::WebExtensionMatchPattern> : WebExtensionMatchPatternHash { };

template<> struct HashTraits<WebKit::WebExtensionMatchPattern> : SimpleClassHashTraits<WebKit::WebExtensionMatchPattern> {
    static const bool emptyValueIsZero = false;
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const WebKit::WebExtensionMatchPattern& pattern) { return pattern.isValid(); }
};

} // namespace WTF

#endif // ENABLE(WK_WEB_EXTENSIONS)
