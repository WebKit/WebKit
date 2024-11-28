/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "URLPatternCanonical.h"

#include "URLPattern.h"
#include <wtf/URLParser.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static constexpr auto dummyURLCharacters { "https://www.webkit.org"_s };

static bool isInvalidIPv6HostCodePoint(auto codepoint)
{
    static constexpr std::array validSpecialCodepoints { '[', ']', ':' };
    return !isASCIIHexDigit(codepoint) && std::find(validSpecialCodepoints.begin(), validSpecialCodepoints.end(), codepoint) != validSpecialCodepoints.end();
}

// https://urlpattern.spec.whatwg.org/#is-an-absolute-pathname
bool isAbsolutePathname(StringView input, BaseURLStringType inputType)
{
    if (input.isEmpty())
        return false;

    if (input[0] == '/')
        return true;

    if (inputType == BaseURLStringType::URL)
        return false;

    if (input.length() < 2)
        return false;

    if (input.startsWith("\\/"_s))
        return true;

    if (input.startsWith("{/"_s))
        return true;

    return false;
}

// https://urlpattern.spec.whatwg.org/#canonicalize-a-protocol, combined with https://urlpattern.spec.whatwg.org/#process-protocol-for-init
ExceptionOr<String> canonicalizeProtocol(StringView value, BaseURLStringType valueType)
{
    if (value.isEmpty())
        return value.toString();

    auto strippedValue = value.endsWith(':') ? value.left(value.length() - 1) : value;

    if (valueType == BaseURLStringType::Pattern)
        return strippedValue.toString();

    URL dummyURL(makeString(strippedValue, "://webkit.test"_s));

    if (!dummyURL.isValid())
        return Exception { ExceptionCode::TypeError, "Invalid input to canonicalize a URL protocol string."_s };

    return dummyURL.protocol().toString();
}

// https://urlpattern.spec.whatwg.org/#canonicalize-a-username, combined with https://urlpattern.spec.whatwg.org/#process-username-for-init
String canonicalizeUsername(StringView value, BaseURLStringType valueType)
{
    if (value.isEmpty())
        return value.toString();

    if (valueType == BaseURLStringType::Pattern)
        return value.toString();

    URL dummyURL(dummyURLCharacters);
    dummyURL.setUser(value);

    return dummyURL.user();
}

// https://urlpattern.spec.whatwg.org/#canonicalize-a-password, combined with https://urlpattern.spec.whatwg.org/#process-password-for-init
String canonicalizePassword(StringView value, BaseURLStringType valueType)
{
    if (value.isEmpty())
        return value.toString();

    if (valueType == BaseURLStringType::Pattern)
        return value.toString();

    URL dummyURL(dummyURLCharacters);
    dummyURL.setPassword(value);

    return dummyURL.password();
}

// https://urlpattern.spec.whatwg.org/#canonicalize-a-hostname, combined with https://urlpattern.spec.whatwg.org/#process-hostname-for-init
ExceptionOr<String> canonicalizeHostname(StringView value, BaseURLStringType valueType)
{
    if (value.isEmpty())
        return value.toString();

    if (valueType == BaseURLStringType::Pattern)
        return value.toString();

    URL dummyURL(dummyURLCharacters);
    dummyURL.setHost(value);

    if (!dummyURL.isValid())
        return Exception { ExceptionCode::TypeError, "Invalid input to canonicalize a URL host string."_s };

    return value.toString();
}

// https://urlpattern.spec.whatwg.org/#canonicalize-an-ipv6-hostname
ExceptionOr<String> canonicalizeIPv6Hostname(StringView value, BaseURLStringType valueType)
{
    if (valueType == BaseURLStringType::Pattern)
        return value.toString();

    StringBuilder result;
    result.reserveCapacity(result.length());

    for (auto codepoint : value.codePoints()) {
        if (isInvalidIPv6HostCodePoint(codepoint))
            return Exception { ExceptionCode::TypeError, "Invalid input to canonicalize a URL IPv6 host string."_s };

        result.append(toASCIILower(codepoint));
    }

    return result.toString();
}

// https://urlpattern.spec.whatwg.org/#canonicalize-a-port, combined with https://urlpattern.spec.whatwg.org/#process-port-for-init
ExceptionOr<String> canonicalizePort(StringView portValue, const std::optional<StringView> protocolValue, BaseURLStringType portValueType)
{
    if (portValue.isEmpty())
        return portValue.toString();

    if (portValueType == BaseURLStringType::Pattern)
        return portValue.toString();

    URL dummyURL(dummyURLCharacters);

    if (protocolValue)
        dummyURL.setProtocol(*protocolValue);

    dummyURL.setPort(parseInteger<uint16_t>(portValue));

    if (!dummyURL.isValid())
        return Exception { ExceptionCode::TypeError, "Invalid input to canonicalize a URL port string."_s };

    return String::number(*dummyURL.port());
}

// https://urlpattern.spec.whatwg.org/#canonicalize-an-opaque-pathname
ExceptionOr<String> canonicalizeOpaquePathname(StringView value)
{
    URL dummyURL(dummyURLCharacters);
    dummyURL.setPath(value);

    if (!dummyURL.isValid())
        return Exception { ExceptionCode::TypeError, "Invalid input to canonicalize a URL opaque path string."_s };

    return dummyURL.path().toString();
}

// https://urlpattern.spec.whatwg.org/#canonicalize-a-pathname
ExceptionOr<String> canonicalizePathname(StringView pathnameValue)
{
    if (pathnameValue.isEmpty())
        return pathnameValue.toString();

    bool hasLeadingSlash = pathnameValue[0] == '/';
    auto maybeAddSlashPrefix = hasLeadingSlash ? pathnameValue : makeString("/-"_s, pathnameValue);

    URL dummyURL(dummyURLCharacters);
    dummyURL.setPath(maybeAddSlashPrefix);

    if (!dummyURL.isValid())
        return Exception { ExceptionCode::TypeError, "Invalid input to canonicalize a URL path string."_s };

    auto result = dummyURL.path();
    if (!hasLeadingSlash)
        result = result.substring(2);

    return result.toString();
}

// https://urlpattern.spec.whatwg.org/#process-pathname-for-init
ExceptionOr<String> processPathname(StringView pathnameValue, const StringView protocolValue, BaseURLStringType pathnameValueType)
{
    if (pathnameValue.isEmpty())
        return pathnameValue.toString();

    if (pathnameValueType == BaseURLStringType::Pattern)
        return pathnameValue.toString();

    if (WTF::URLParser::isSpecialScheme(protocolValue))
        return canonicalizePathname(pathnameValue);

    return canonicalizeOpaquePathname(pathnameValue);
}

// https://urlpattern.spec.whatwg.org/#canonicalize-a-search, combined with https://urlpattern.spec.whatwg.org/#process-search-for-init
ExceptionOr<String> canonicalizeSearch(StringView value, BaseURLStringType valueType)
{
    if (value.isEmpty())
        return value.toString();

    auto strippedValue = value[0] == '?' ? value.substring(1) : value;

    if (valueType == BaseURLStringType::Pattern)
        return strippedValue.toString();

    URL dummyURL(dummyURLCharacters);
    dummyURL.setQuery(strippedValue);

    if (!dummyURL.isValid())
        return Exception { ExceptionCode::TypeError, "Invalid input to canonicalize a URL search string."_s };

    return dummyURL.query().toString();
}

// https://urlpattern.spec.whatwg.org/#canonicalize-a-hash, combined with https://urlpattern.spec.whatwg.org/#process-hash-for-init
ExceptionOr<String> canonicalizeHash(StringView value, BaseURLStringType valueType)
{
    if (value.isEmpty())
        return value.toString();

    auto strippedValue = value[0] == '#' ? value.substring(1) : value;

    if (valueType == BaseURLStringType::Pattern)
        return strippedValue.toString();

    URL dummyURL(dummyURLCharacters);
    dummyURL.setFragmentIdentifier(strippedValue);

    if (!dummyURL.isValid())
        return Exception { ExceptionCode::TypeError, "Invalid input to canonicalize a URL hash string."_s };

    return dummyURL.fragmentIdentifier().toString();
}

ExceptionOr<String> callEncodingCallback(EncodingCallbackType type, StringView input)
{
    switch (type) {
    case EncodingCallbackType::Protocol:
        return canonicalizeProtocol(input, BaseURLStringType::URL);
    case EncodingCallbackType::Username:
        return canonicalizeUsername(input, BaseURLStringType::URL);
    case EncodingCallbackType::Password:
        return canonicalizePassword(input, BaseURLStringType::URL);
    case EncodingCallbackType::Host:
        return canonicalizeHostname(input, BaseURLStringType::URL);
    case EncodingCallbackType::IPv6Host:
        return canonicalizeIPv6Hostname(input, BaseURLStringType::URL);
    case EncodingCallbackType::Port:
        return canonicalizePort(input, { }, BaseURLStringType::URL);
    case EncodingCallbackType::Path:
        return canonicalizePathname(input);
    case EncodingCallbackType::OpaquePath:
        return canonicalizeOpaquePathname(input);
    case EncodingCallbackType::Search:
        return canonicalizeSearch(input, BaseURLStringType::URL);
    case EncodingCallbackType::Hash:
        return canonicalizeHash(input, BaseURLStringType::URL);
    default:
        return Exception { ExceptionCode::TypeError, "Invalid input type for encoding callback."_s };
    }
}

}
