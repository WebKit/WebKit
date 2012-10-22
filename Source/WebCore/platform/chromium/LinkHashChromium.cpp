/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "KURL.h"
#include "LinkHash.h"

#if USE(GOOGLEURL)
#include <googleurl/src/url_util.h>
#endif

#include <public/Platform.h>

namespace WebCore {

// Visited Links --------------------------------------------------------------

LinkHash visitedLinkHash(const UChar* url, unsigned length)
{
    url_canon::RawCanonOutput<2048> buffer;
    url_parse::Parsed parsed;
    if (!url_util::Canonicalize(url, length, 0, &buffer, &parsed))
        return 0; // Invalid URLs are unvisited.
    return WebKit::Platform::current()->visitedLinkHash(buffer.data(), buffer.length());
}

LinkHash visitedLinkHash(const String& url)
{
    return visitedLinkHash(url.characters(), url.length());
}

LinkHash visitedLinkHash(const KURL& base, const AtomicString& attributeURL)
{
    // Resolve the relative URL using googleurl and pass the absolute URL up to
    // the embedder. We could create a GURL object from the base and resolve
    // the relative URL that way, but calling the lower-level functions
    // directly saves us the string allocation in most cases.
    url_canon::RawCanonOutput<2048> buffer;
    url_parse::Parsed parsed;

#if USE(GOOGLEURL)
    const CString& cstr = base.utf8String();
    const char* data = cstr.data();
    int length = cstr.length();
    const url_parse::Parsed& srcParsed = base.parsed();
#else
    // When we're not using GoogleURL, first canonicalize it so we can resolve it
    // below.
    url_canon::RawCanonOutput<2048> srcCanon;
    url_parse::Parsed srcParsed;
    String str = base.string();
    if (!url_util::Canonicalize(str.characters(), str.length(), 0, &srcCanon, &srcParsed))
        return 0;
    const char* data = srcCanon.data();
    int length = srcCanon.length();
#endif

    if (!url_util::ResolveRelative(data, length, srcParsed, attributeURL.characters(),
                                   attributeURL.length(), 0, &buffer, &parsed))
        return 0; // Invalid resolved URL.

    return WebKit::Platform::current()->visitedLinkHash(buffer.data(), buffer.length());
}

} // namespace WebCore
