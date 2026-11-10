/*
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/HashSet.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

using HTTPHeaderSet = HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash>;

class ResourceResponse;
enum class HTTPHeaderName : uint16_t;

enum class XSSProtectionDisposition {
    Invalid,
    Disabled,
    Enabled,
    BlockEnabled,
};

enum class ContentTypeOptionsDisposition : bool {
    None,
    Nosniff
};

enum class XFrameOptionsDisposition : uint8_t {
    None,
    Deny,
    SameOrigin,
    AllowAll,
    Invalid,
    Conflict
};

enum class CrossOriginResourcePolicy : uint8_t {
    None,
    CrossOrigin,
    SameOrigin,
    SameSite,
    Invalid
};

enum class ClearSiteDataValue : uint8_t {
    Cache = 1 << 0,
    Cookies = 1 << 1,
    ExecutionContexts = 1 << 2,
    Storage = 1 << 3,
    PrefetchCache = 1 << 4,
};

enum class RangeAllowWhitespace : bool { No, Yes };

NODELETE bool isValidReasonPhrase(StringImpl*);
NODELETE WEBCORE_EXPORT bool isValidHTTPHeaderValue(StringImpl*);
NODELETE bool isValidAcceptHeaderValue(StringImpl*);
NODELETE bool isValidLanguageHeaderValue(StringImpl*);
#if USE(GLIB)
WEBCORE_EXPORT bool isValidUserAgentHeaderValue(StringImpl*);
#endif
NODELETE WEBCORE_EXPORT bool isValidHTTPToken(StringImpl*);
NODELETE bool isValidHTTPToken(StringView);
inline bool isValidHTTPToken(const RefPtr<StringImpl>& value) { return isValidHTTPToken(value.get()); }
std::optional<WallTime> parseHTTPDate(StringImpl*);
StringView filenameFromHTTPContentDisposition(StringView value LIFETIME_BOUND);
WEBCORE_EXPORT RefPtr<StringImpl> extractMIMETypeFromMediaType(StringImpl*);
WEBCORE_EXPORT StringView extractCharsetFromMediaType(StringView mediaType LIFETIME_BOUND);
XSSProtectionDisposition parseXSSProtectionHeader(StringImpl* header, RefPtr<StringImpl>& failureReason, unsigned& failurePosition, RefPtr<StringImpl>& reportURL);
RefPtr<AtomStringImpl> extractReasonPhraseFromHTTPStatusLine(StringImpl*);
WEBCORE_EXPORT XFrameOptionsDisposition parseXFrameOptionsHeader(StringView);
WEBCORE_EXPORT OptionSet<ClearSiteDataValue> parseClearSiteDataHeader(const ResourceResponse&);

// One of the HTTPRange bounds could be set to std::nullopt but not both of them.
struct HTTPRange {
    std::optional<uint64_t> start;
    std::optional<uint64_t> end;
};
WEBCORE_EXPORT std::optional<HTTPRange> parseRange(StringView, RangeAllowWhitespace);

ContentTypeOptionsDisposition parseContentTypeOptionsHeader(StringView header);

// Parsing Complete HTTP Messages.
size_t parseHTTPHeader(std::span<const uint8_t> data, RefPtr<StringImpl>& failureReason, StringView& nameStr, RefPtr<StringImpl>& valueStr, bool strict = true);
size_t parseHTTPRequestBody(std::span<const uint8_t> data, Vector<uint8_t>& body);

std::optional<uint64_t> parseContentLength(StringView);

// HTTP Header routine as per https://fetch.spec.whatwg.org/#terminology-headers
WEBCORE_EXPORT bool isForbiddenHeader(StringImpl* name, StringView value);
bool isForbiddenHeaderName(StringImpl*);
bool isNoCORSSafelistedRequestHeaderName(StringImpl*);
bool isPriviledgedNoCORSRequestHeaderName(StringImpl*);
WEBCORE_EXPORT bool isForbiddenResponseHeaderName(StringImpl*);
bool isForbiddenMethod(StringView);
bool isSimpleHeader(StringImpl* name, StringImpl* value);
bool isCrossOriginSafeHeader(HTTPHeaderName, const HTTPHeaderSet&);
bool isCrossOriginSafeHeader(StringImpl*, const HTTPHeaderSet&);
bool isCrossOriginSafeRequestHeader(HTTPHeaderName, StringImpl*);

RefPtr<StringImpl> normalizeHTTPMethod(StringImpl*);
bool isSafeMethod(StringImpl*);

WEBCORE_EXPORT CrossOriginResourcePolicy parseCrossOriginResourcePolicyHeader(StringView);

// The delimiter placed between values when appending to a request header, or nullopt if appending isn't supported.
WEBCORE_EXPORT std::optional<ASCIILiteral> requestHeaderAppendDelimiter(StringImpl* header);

// Request headers, and header values, that an extension can't set or append to, because the network stack manages them.
WEBCORE_EXPORT bool isRequestHeaderUnsafeToModify(StringImpl* header, StringView value);

template<class HashType>
bool addToAccessControlAllowList(StringImpl* string, unsigned start, unsigned end, HashSet<RefPtr<StringImpl>, HashType>& set)
{
    StringImpl* stringImpl = string;
    if (!stringImpl)
        return true;

    // Skip white space from start.
    while (start <= end && isASCIIWhitespaceWithoutFF((*stringImpl)[start]))
        ++start;

    // only white space
    if (start > end)
        return true;

    // Skip white space from end.
    while (end && isASCIIWhitespaceWithoutFF((*stringImpl)[end]))
        --end;

    auto token = substring(string, start, end - start + 1);
    if (!isValidHTTPToken(token.get()))
        return false;

    set.add(WTF::move(token));
    return true;
}

template<class HashType = DefaultHash<RefPtr<StringImpl>>>
std::optional<HashSet<RefPtr<StringImpl>, HashType>> parseAccessControlAllowList(StringImpl* string)
{
    HashSet<RefPtr<StringImpl>, HashType> set;
    unsigned start = 0;
    size_t end;
    while ((end = find(string, ',', start)) != notFound) {
        if (start != end) {
            if (!addToAccessControlAllowList(string, start, end - 1, set))
                return { };
        }
        start = end + 1;
    }
    if (start != length(string)) {
        if (!addToAccessControlAllowList(string, start, length(string) - 1, set))
            return { };
    }
    return set;
}

}
