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

#pragma once

#include "SecurityOrigin.h"

namespace WebCore {

// FIXME: Rename this to URLDecompositionFunctions.
template<typename T> class URLUtils {
public:
    URL href() const { return static_cast<const T*>(this)->href(); }
    void setHref(const String& url) { static_cast<T*>(this)->setHref(url); }

    String toString() const;
    String toJSON() const;

    String origin() const;

    String protocol() const;
    void setProtocol(StringView);

    String username() const;
    void setUsername(StringView);

    String password() const;
    void setPassword(StringView);

    String host() const;
    void setHost(StringView);

    String hostname() const;
    void setHostname(StringView);

    String port() const;
    void setPort(StringView);

    String pathname() const;
    void setPathname(StringView);

    String search() const;
    void setSearch(const String&);

    String hash() const;
    void setHash(StringView);
};

template<typename T> String URLUtils<T>::toString() const
{
    return href().string();
}

template<typename T> String URLUtils<T>::toJSON() const
{
    return href().string();
}

template<typename T> String URLUtils<T>::origin() const
{
    return SecurityOrigin::create(href())->toString();
}

template<typename T> String URLUtils<T>::protocol() const
{
    if (WTF::protocolIsJavaScript(href()))
        return "javascript:"_s;
    return makeString(href().protocol(), ':');
}

template<typename T> void URLUtils<T>::setProtocol(StringView value)
{
    URL url = href();
    url.setProtocol(value);
    setHref(url.string());
}

template<typename T> String URLUtils<T>::username() const
{
    return href().encodedUser().toString();
}

template<typename T> void URLUtils<T>::setUsername(StringView user)
{
    URL url = href();
    if (url.cannotBeABaseURL())
        return;
    url.setUser(user);
    setHref(url);
}

template<typename T> String URLUtils<T>::password() const
{
    return href().encodedPassword().toString();
}

template<typename T> void URLUtils<T>::setPassword(StringView password)
{
    URL url = href();
    if (url.cannotBeABaseURL())
        return;
    url.setPassword(password);
    setHref(url);
}

template<typename T> String URLUtils<T>::host() const
{
    return href().hostAndPort();
}

inline unsigned countASCIIDigits(StringView string)
{
    unsigned length = string.length();
    for (unsigned count = 0; count < length; ++count) {
        if (!isASCIIDigit(string[count]))
            return count;
    }
    return length;
}

template<typename T> void URLUtils<T>::setHost(StringView value)
{
    if (value.isEmpty())
        return;
    URL url = href();
    if (url.cannotBeABaseURL())
        return;
    if (!url.canSetHostOrPort())
        return;

    size_t separator = value.find(':');
    if (!separator)
        return;

    if (separator == notFound)
        url.setHostAndPort(value);
    else {
        unsigned portLength = countASCIIDigits(value.substring(separator + 1));
        if (!portLength) {
            // http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes
            // specifically goes against RFC 3986 (p3.2) and
            // requires setting the port to "0" if it is set to empty string.
            // FIXME: This seems like something that has since been changed and this rule and code may be obsolete.
            url.setHostAndPort(makeString(value.substring(0, separator + 1), '0'));
        } else {
            auto portNumber = parseUInt16(value.substring(separator + 1, portLength));
            if (portNumber && WTF::isDefaultPortForProtocol(*portNumber, url.protocol()))
                url.setHostAndPort(value.substring(0, separator));
            else
                url.setHostAndPort(value.substring(0, separator + 1 + portLength));
        }
    }
    setHref(url.string());
}

template<typename T> String URLUtils<T>::hostname() const
{
    return href().host().toString();
}

inline StringView removeAllLeadingSolidusCharacters(StringView string)
{
    unsigned i;
    unsigned length = string.length();
    for (i = 0; i < length; ++i) {
        if (string[i] != '/')
            break;
    }
    return string.substring(i);
}

template<typename T> void URLUtils<T>::setHostname(StringView value)
{
    auto host = removeAllLeadingSolidusCharacters(value);
    if (host.isEmpty())
        return;

    URL url = href();
    if (url.cannotBeABaseURL())
        return;
    if (!url.canSetHostOrPort())
        return;

    url.setHost(host);
    setHref(url.string());
}

template<typename T> String URLUtils<T>::port() const
{
    if (href().port())
        return String::number(href().port().value());

    return emptyString();
}

template<typename T> void URLUtils<T>::setPort(StringView value)
{
    URL url = href();
    if (url.cannotBeABaseURL() || url.protocolIs("file") || !url.canSetHostOrPort())
        return;

    auto digitsOnly = value.left(countASCIIDigits(value));
    Optional<uint16_t> port;
    if (!digitsOnly.isEmpty()) {
        port = parseUInt16(digitsOnly);
        if (!port)
            return;
        if (WTF::isDefaultPortForProtocol(*port, url.protocol()))
            port = WTF::nullopt;
    }
    url.setPort(port);

    setHref(url.string());
}

template<typename T> String URLUtils<T>::pathname() const
{
    return href().path().toString();
}

template<typename T> void URLUtils<T>::setPathname(StringView value)
{
    URL url = href();
    if (url.cannotBeABaseURL())
        return;
    if (!url.canSetPathname())
        return;

    if (value.startsWith('/'))
        url.setPath(value);
    else
        url.setPath(makeString('/', value));

    setHref(url.string());
}

template<typename T> String URLUtils<T>::search() const
{
    return href().query().isEmpty() ? emptyString() : href().queryWithLeadingQuestionMark().toString();
}

template<typename T> void URLUtils<T>::setSearch(const String& value)
{
    URL url = href();
    if (value.isEmpty()) {
        // If the given value is the empty string, set url's query to null.
        url.setQuery({ });
    } else {
        String newSearch = value.startsWith('?') ? value.substring(1) : value;
        // Make sure that '#' in the query does not leak to the hash.
        url.setQuery(newSearch.replaceWithLiteral('#', "%23"));
    }

    setHref(url.string());
}

template<typename T> String URLUtils<T>::hash() const
{
    // FIXME: Why convert this string to an atom here instead of just a string? Intentionally to save memory? An error?
    return href().fragmentIdentifier().isEmpty() ? emptyAtom() : href().fragmentIdentifierWithLeadingNumberSign().toAtomString();
}

template<typename T> void URLUtils<T>::setHash(StringView value)
{
    URL url = href();
    auto newFragment = value.startsWith('#') ? StringView(value).substring(1) : StringView(value);
    if (newFragment.isEmpty())
        url.removeFragmentIdentifier();
    else
        url.setFragmentIdentifier(newFragment);
    setHref(url.string());
}

} // namespace WebCore
