/*
 * Copyright (C) 2014 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "URL.h"

#include <libsoup/soup.h>
#include <wtf/text/CString.h>

namespace WebCore {

URL::URL(SoupURI* soupURI)
{
    if (!soupURI) {
        invalidate();
        return;
    }

    GUniquePtr<gchar> urlString(soup_uri_to_string(soupURI, FALSE));
    parse(String::fromUTF8(urlString.get()));
    if (!isValid())
        return;

    // Motivated by https://bugs.webkit.org/show_bug.cgi?id=38956. libsoup
    // does not add the password to the URL when calling
    // soup_uri_to_string, and thus the requests are not properly
    // built. Fixing soup_uri_to_string is a no-no as the maintainer does
    // not want to break compatibility with previous implementations
    if (soupURI->password)
        setPass(String::fromUTF8(soupURI->password));
}

GUniquePtr<SoupURI> URL::createSoupURI() const
{
    if (!isValid())
        return nullptr;

    // WebKit does not support fragment identifiers in data URLs, but soup does.
    // Before passing the URL to soup, we should make sure to urlencode any '#'
    // characters, so that soup does not interpret them as fragment identifiers.
    // See http://wkbug.com/68089
    if (protocolIsData()) {
        String urlString = string();
        urlString.replace("#", "%23");
        return GUniquePtr<SoupURI>(soup_uri_new(urlString.utf8().data()));
    }

    GUniquePtr<SoupURI> soupURI(soup_uri_new(string().utf8().data()));

    // Versions of libsoup prior to 2.42 have a soup_uri_new that will convert empty passwords that are not
    // prefixed by a colon into null. Some parts of soup like the SoupAuthenticationManager will only be active
    // when both the username and password are non-null. When we have credentials, empty usernames and passwords
    // should be empty strings instead of null.
    String urlUser = user();
    String urlPass = pass();
    if (!urlUser.isEmpty() || !urlPass.isEmpty()) {
        soup_uri_set_user(soupURI.get(), urlUser.utf8().data());
        soup_uri_set_password(soupURI.get(), urlPass.utf8().data());
    }

    return soupURI;
}

} // namespace WebCore
