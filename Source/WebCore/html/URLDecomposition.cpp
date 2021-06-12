/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "URLDecomposition.h"

#include "SecurityOrigin.h"
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

String URLDecomposition::origin() const
{
    return SecurityOrigin::create(fullURL())->toString();
}

String URLDecomposition::protocol() const
{
    auto fullURL = this->fullURL();
    if (WTF::protocolIsJavaScript(fullURL.string()))
        return "javascript:"_s;
    return makeString(fullURL.protocol(), ':');
}

void URLDecomposition::setProtocol(StringView value)
{
    URL copy = fullURL();
    copy.setProtocol(value);
    setFullURL(copy);
}

String URLDecomposition::username() const
{
    return fullURL().encodedUser().toString();
}

void URLDecomposition::setUsername(StringView user)
{
    auto fullURL = this->fullURL();
    if (fullURL.host().isEmpty() || fullURL.cannotBeABaseURL() || fullURL.protocolIs("file"))
        return;
    fullURL.setUser(user);
    setFullURL(fullURL);
}

String URLDecomposition::password() const
{
    return fullURL().encodedPassword().toString();
}

void URLDecomposition::setPassword(StringView password)
{
    auto fullURL = this->fullURL();
    if (fullURL.host().isEmpty() || fullURL.cannotBeABaseURL() || fullURL.protocolIs("file"))
        return;
    fullURL.setPassword(password);
    setFullURL(fullURL);
}

String URLDecomposition::host() const
{
    return fullURL().hostAndPort();
}

static unsigned countASCIIDigits(StringView string)
{
    unsigned length = string.length();
    for (unsigned count = 0; count < length; ++count) {
        if (!isASCIIDigit(string[count]))
            return count;
    }
    return length;
}

void URLDecomposition::setHost(StringView value)
{
    auto fullURL = this->fullURL();
    if (value.isEmpty() && !fullURL.protocolIs("file"))
        return;

    size_t separator = value.reverseFind(':');
    if (!separator)
        return;

    if (fullURL.cannotBeABaseURL() || !fullURL.canSetHostOrPort())
        return;

    // No port if no colon or rightmost colon is within the IPv6 section.
    size_t ipv6Separator = value.reverseFind(']');
    if (separator == notFound || (ipv6Separator != notFound && ipv6Separator > separator))
        fullURL.setHost(value);
    else {
        // Multiple colons are acceptable only in case of IPv6.
        if (value.find(':') != separator && ipv6Separator == notFound)
            return;
        unsigned portLength = countASCIIDigits(value.substring(separator + 1));
        if (!portLength) {
            fullURL.setHost(value.substring(0, separator));
        } else {
            auto portNumber = parseInteger<uint16_t>(value.substring(separator + 1, portLength));
            if (portNumber && WTF::isDefaultPortForProtocol(*portNumber, fullURL.protocol()))
                fullURL.setHostAndPort(value.substring(0, separator));
            else
                fullURL.setHostAndPort(value.substring(0, separator + 1 + portLength));
        }
    }
    if (fullURL.isValid())
        setFullURL(fullURL);
}

String URLDecomposition::hostname() const
{
    return fullURL().host().toString();
}

static StringView removeAllLeadingSolidusCharacters(StringView string)
{
    unsigned i;
    unsigned length = string.length();
    for (i = 0; i < length; ++i) {
        if (string[i] != '/')
            break;
    }
    return string.substring(i);
}

void URLDecomposition::setHostname(StringView value)
{
    auto fullURL = this->fullURL();
    auto host = removeAllLeadingSolidusCharacters(value);
    if (host.isEmpty() && !fullURL.protocolIs("file"))
        return;
    if (fullURL.cannotBeABaseURL() || !fullURL.canSetHostOrPort())
        return;
    fullURL.setHost(host);
    if (fullURL.isValid())
        setFullURL(fullURL);
}

String URLDecomposition::port() const
{
    auto port = fullURL().port();
    if (!port)
        return emptyString();
    return String::number(*port);
}

// Outer optional is whether we could parse at all. Inner optional is "no port specified".
static std::optional<std::optional<uint16_t>> parsePort(StringView string, StringView protocol)
{
    auto digitsOnly = string.left(countASCIIDigits(string));
    if (digitsOnly.isEmpty())
        return std::optional<uint16_t> { std::nullopt };
    auto port = parseInteger<uint16_t>(digitsOnly);
    if (!port)
        return std::nullopt;
    if (WTF::isDefaultPortForProtocol(*port, protocol))
        return std::optional<uint16_t> { std::nullopt };
    return { { *port } };
}

void URLDecomposition::setPort(StringView value)
{
    auto fullURL = this->fullURL();
    if (fullURL.host().isEmpty() || fullURL.cannotBeABaseURL() || fullURL.protocolIs("file") || !fullURL.canSetHostOrPort())
        return;
    auto port = parsePort(value, fullURL.protocol());
    if (!port)
        return;
    fullURL.setPort(*port);
    setFullURL(fullURL);
}

String URLDecomposition::pathname() const
{
    return fullURL().path().toString();
}

void URLDecomposition::setPathname(StringView value)
{
    auto fullURL = this->fullURL();
    if (fullURL.cannotBeABaseURL() || !fullURL.canSetPathname())
        return;
    fullURL.setPath(value);
    setFullURL(fullURL);
}

String URLDecomposition::search() const
{
    auto fullURL = this->fullURL();
    return fullURL.query().isEmpty() ? emptyString() : fullURL.queryWithLeadingQuestionMark().toString();
}

void URLDecomposition::setSearch(const String& value)
{
    auto fullURL = this->fullURL();
    if (value.isEmpty()) {
        // If the given value is the empty string, set url's query to null.
        fullURL.setQuery({ });
    } else {
        String newSearch = value;
        // Make sure that '#' in the query does not leak to the hash.
        fullURL.setQuery(newSearch.replaceWithLiteral('#', "%23"));
    }
    setFullURL(fullURL);
}

String URLDecomposition::hash() const
{
    // FIXME: Why convert this string to an atom here instead of just a string? Intentionally to save memory? An error?
    auto fullURL = this->fullURL();
    return fullURL.fragmentIdentifier().isEmpty() ? emptyAtom() : fullURL.fragmentIdentifierWithLeadingNumberSign().toAtomString();
}

void URLDecomposition::setHash(StringView value)
{
    auto fullURL = this->fullURL();
    if (value.isEmpty())
        fullURL.removeFragmentIdentifier();
    else
        fullURL.setFragmentIdentifier(value.startsWith('#') ? value.substring(1) : value);
    setFullURL(fullURL);
}

}
