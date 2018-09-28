/*
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/Optional.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

typedef HashSet<String, ASCIICaseInsensitiveHash> HTTPHeaderSet;

enum class HTTPHeaderName;

enum class XSSProtectionDisposition {
    Invalid,
    Disabled,
    Enabled,
    BlockEnabled,
};

enum ContentTypeOptionsDisposition {
    ContentTypeOptionsNone,
    ContentTypeOptionsNosniff
};

enum XFrameOptionsDisposition {
    XFrameOptionsNone,
    XFrameOptionsDeny,
    XFrameOptionsSameOrigin,
    XFrameOptionsAllowAll,
    XFrameOptionsInvalid,
    XFrameOptionsConflict
};

enum class CrossOriginResourcePolicy {
    None,
    SameOrigin,
    SameSite,
    Invalid
};

bool isValidReasonPhrase(const String&);
bool isValidHTTPHeaderValue(const String&);
bool isValidAcceptHeaderValue(const String&);
bool isValidLanguageHeaderValue(const String&);
bool isValidHTTPToken(const String&);
bool parseHTTPRefresh(const String& refresh, double& delay, String& url);
std::optional<WallTime> parseHTTPDate(const String&);
String filenameFromHTTPContentDisposition(const String&);
String extractMIMETypeFromMediaType(const String&);
String extractCharsetFromMediaType(const String&);
void findCharsetInMediaType(const String& mediaType, unsigned int& charsetPos, unsigned int& charsetLen, unsigned int start = 0);
XSSProtectionDisposition parseXSSProtectionHeader(const String& header, String& failureReason, unsigned& failurePosition, String& reportURL);
AtomicString extractReasonPhraseFromHTTPStatusLine(const String&);
WEBCORE_EXPORT XFrameOptionsDisposition parseXFrameOptionsHeader(const String&);

// -1 could be set to one of the return parameters to indicate the value is not specified.
WEBCORE_EXPORT bool parseRange(const String&, long long& rangeOffset, long long& rangeEnd, long long& rangeSuffixLength);

ContentTypeOptionsDisposition parseContentTypeOptionsHeader(const String& header);

// Parsing Complete HTTP Messages.
enum HTTPVersion { Unknown, HTTP_1_0, HTTP_1_1 };
size_t parseHTTPRequestLine(const char* data, size_t length, String& failureReason, String& method, String& url, HTTPVersion&);
size_t parseHTTPHeader(const char* data, size_t length, String& failureReason, StringView& nameStr, String& valueStr, bool strict = true);
size_t parseHTTPRequestBody(const char* data, size_t length, Vector<unsigned char>& body);

void parseAccessControlExposeHeadersAllowList(const String& headerValue, HTTPHeaderSet&);

// HTTP Header routine as per https://fetch.spec.whatwg.org/#terminology-headers
bool isForbiddenHeaderName(const String&);
bool isForbiddenResponseHeaderName(const String&);
bool isForbiddenMethod(const String&);
bool isSimpleHeader(const String& name, const String& value);
bool isCrossOriginSafeHeader(HTTPHeaderName, const HTTPHeaderSet&);
bool isCrossOriginSafeHeader(const String&, const HTTPHeaderSet&);
bool isCrossOriginSafeRequestHeader(HTTPHeaderName, const String&);

String normalizeHTTPMethod(const String&);

WEBCORE_EXPORT CrossOriginResourcePolicy parseCrossOriginResourcePolicyHeader(StringView);

inline bool isHTTPSpace(UChar character)
{
    return character <= ' ' && (character == ' ' || character == '\n' || character == '\t' || character == '\r');
}

// Strip leading and trailing whitespace as defined in https://fetch.spec.whatwg.org/#concept-header-value-normalize.
inline String stripLeadingAndTrailingHTTPSpaces(const String& string)
{
    return string.stripLeadingAndTrailingCharacters(isHTTPSpace);
}

inline StringView stripLeadingAndTrailingHTTPSpaces(StringView string)
{
    return string.stripLeadingAndTrailingMatchedCharacters(isHTTPSpace);
}

template<class HashType>
void addToAccessControlAllowList(const String& string, unsigned start, unsigned end, HashSet<String, HashType>& set)
{
    StringImpl* stringImpl = string.impl();
    if (!stringImpl)
        return;

    // Skip white space from start.
    while (start <= end && isSpaceOrNewline((*stringImpl)[start]))
        ++start;

    // only white space
    if (start > end)
        return;

    // Skip white space from end.
    while (end && isSpaceOrNewline((*stringImpl)[end]))
        --end;

    set.add(string.substring(start, end - start + 1));
}

template<class HashType = DefaultHash<String>::Hash>
std::optional<HashSet<String, HashType>> parseAccessControlAllowList(const String& string)
{
    HashSet<String, HashType> set;
    unsigned start = 0;
    size_t end;
    while ((end = string.find(',', start)) != notFound) {
        if (start != end)
            addToAccessControlAllowList(string, start, end - 1, set);
        start = end + 1;
    }
    if (start != string.length())
        addToAccessControlAllowList(string, start, string.length() - 1, set);

    return set;
}

}
