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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

    return GUniquePtr<SoupURI>(soup_uri_new(string().utf8().data()));
}

} // namespace WebCore
