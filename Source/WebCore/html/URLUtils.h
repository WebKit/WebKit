/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

template <typename T>
class URLUtils {
public:
    URL href() const { return static_cast<const T*>(this)->href(); }
    void setHref(const String& url) { static_cast<T*>(this)->setHref(url); }

    String toString() const;
    String toJSON() const;

    String origin() const;

    String protocol() const;
    void setProtocol(const String&);

    String username() const;
    void setUsername(const String&);

    String password() const;
    void setPassword(const String&);

    String host() const;
    void setHost(const String&);

    String hostname() const;
    void setHostname(const String&);

    String port() const;
    void setPort(const String&);

    String pathname() const;
    void setPathname(const String&);

    String search() const;
    void setSearch(const String&);

    String hash() const;
    void setHash(const String&);
};

template <typename T>
String URLUtils<T>::toString() const
{
    return href().string();
}

template <typename T>
String URLUtils<T>::toJSON() const
{
    return href().string();
}

template <typename T>
String URLUtils<T>::origin() const
{
    auto origin = SecurityOrigin::create(href());
    return origin->toString();
}

template <typename T>
String URLUtils<T>::protocol() const
{
    if (WTF::protocolIsJavaScript(href()))
        return "javascript:"_s;
    return makeString(href().protocol(), ':');
}

template <typename T>
void URLUtils<T>::setProtocol(const String& value)
{
    URL url = href();
    url.setProtocol(value);
    setHref(url.string());
}

template <typename T>
String URLUtils<T>::username() const
{
    return href().encodedUser();
}

template <typename T>
void URLUtils<T>::setUsername(const String& user)
{
    URL url = href();
    if (url.cannotBeABaseURL())
        return;
    url.setUser(user);
    setHref(url);
}

template <typename T>
String URLUtils<T>::password() const
{
    return href().encodedPass();
}

template <typename T>
void URLUtils<T>::setPassword(const String& pass)
{
    URL url = href();
    if (url.cannotBeABaseURL())
        return;
    url.setPass(pass);
    setHref(url);
}

template <typename T>
String URLUtils<T>::host() const
{
    return href().hostAndPort();
}

// This function does not allow leading spaces before the port number.
static inline unsigned parsePortFromStringPosition(const String& value, unsigned portStart, unsigned& portEnd)
{
    portEnd = portStart;
    while (isASCIIDigit(value[portEnd]))
        ++portEnd;
    return value.substring(portStart, portEnd - portStart).toUInt();
}

template <typename T>
void URLUtils<T>::setHost(const String& value)
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
        unsigned portEnd;
        unsigned port = parsePortFromStringPosition(value, separator + 1, portEnd);
        if (!port) {
            // http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes
            // specifically goes against RFC 3986 (p3.2) and
            // requires setting the port to "0" if it is set to empty string.
            url.setHostAndPort(value.substring(0, separator + 1) + '0');
        } else {
            if (WTF::isDefaultPortForProtocol(port, url.protocol()))
                url.setHostAndPort(value.substring(0, separator));
            else
                url.setHostAndPort(value.substring(0, portEnd));
        }
    }
    setHref(url.string());
}

template <typename T>
String URLUtils<T>::hostname() const
{
    return href().host().toString();
}

template <typename T>
void URLUtils<T>::setHostname(const String& value)
{
    // Before setting new value:
    // Remove all leading U+002F SOLIDUS ("/") characters.
    unsigned i = 0;
    unsigned hostLength = value.length();
    while (value[i] == '/')
        i++;

    if (i == hostLength)
        return;

    URL url = href();
    if (url.cannotBeABaseURL())
        return;
    if (!url.canSetHostOrPort())
        return;

    url.setHost(value.substring(i));
    setHref(url.string());
}

template <typename T>
String URLUtils<T>::port() const
{
    if (href().port())
        return String::number(href().port().value());

    return emptyString();
}

template <typename T>
void URLUtils<T>::setPort(const String& value)
{
    URL url = href();
    if (url.cannotBeABaseURL() || url.protocolIs("file"))
        return;
    if (!url.canSetHostOrPort())
        return;

    // http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes
    // specifically goes against RFC 3986 (p3.2) and
    // requires setting the port to "0" if it is set to empty string.
    // FIXME: http://url.spec.whatwg.org/ doesn't appear to require this; test what browsers do
    unsigned port = value.toUInt();
    if (WTF::isDefaultPortForProtocol(port, url.protocol()))
        url.removePort();
    else
        url.setPort(port);

    setHref(url.string());
}

template <typename T>
String URLUtils<T>::pathname() const
{
    return href().path();
}

template <typename T>
void URLUtils<T>::setPathname(const String& value)
{
    URL url = href();
    if (url.cannotBeABaseURL())
        return;
    if (!url.canSetPathname())
        return;

    if (value[0U] == '/')
        url.setPath(value);
    else
        url.setPath("/" + value);

    setHref(url.string());
}

template <typename T>
String URLUtils<T>::search() const
{
    String query = href().query();
    return query.isEmpty() ? emptyString() : "?" + query;
}

template <typename T>
void URLUtils<T>::setSearch(const String& value)
{
    URL url = href();
    if (value.isEmpty()) {
        // If the given value is the empty string, set url's query to null.
        url.setQuery({ });
    } else {
        String newSearch = (value[0U] == '?') ? value.substring(1) : value;
        // Make sure that '#' in the query does not leak to the hash.
        url.setQuery(newSearch.replaceWithLiteral('#', "%23"));
    }

    setHref(url.string());
}

template <typename T>
String URLUtils<T>::hash() const
{
    String fragmentIdentifier = href().fragmentIdentifier();
    if (fragmentIdentifier.isEmpty())
        return emptyString();
    return AtomicString(String("#" + fragmentIdentifier));
}

template <typename T>
void URLUtils<T>::setHash(const String& value)
{
    URL url = href();
    String newFragment = value[0U] == '#' ? value.substring(1) : value;
    if (newFragment.isEmpty())
        url.removeFragmentIdentifier();
    else
        url.setFragmentIdentifier(newFragment);
    setHref(url.string());
}

} // namespace WebCore
