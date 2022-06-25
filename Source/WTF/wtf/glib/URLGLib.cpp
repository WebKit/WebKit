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

#if USE(GLIB)

#include <glib.h>
#include <wtf/URLParser.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace WTF {

#if HAVE(GURI)
URL::URL(GUri* uri)
{
    if (!uri) {
        invalidate();
        return;
    }

    GUniquePtr<char> uriString(g_uri_to_string(uri));
    URLParser parser(String::fromUTF8(uriString.get()));
    *this = parser.result();
}

GRefPtr<GUri> URL::createGUri() const
{
    if (isNull())
        return nullptr;

    return adoptGRef(g_uri_parse(m_string.utf8().data(),
        static_cast<GUriFlags>(G_URI_FLAGS_HAS_PASSWORD | G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED_QUERY | G_URI_FLAGS_ENCODED_FRAGMENT | G_URI_FLAGS_SCHEME_NORMALIZE | G_URI_FLAGS_PARSE_RELAXED),
        nullptr));
}
#endif

bool URL::hostIsIPAddress(StringView host)
{
    return !host.isEmpty() && g_hostname_is_ip_address(host.utf8().data());
}

} // namespace WTF

#endif // USE(GLIB)
