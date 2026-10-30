/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(AX_CUSTOM_COLOR_MODE)

#include <wtf/Forward.h>

namespace WebCore {

using AXCustomColorModeBlocklistURLMap = HashMap<String, HashSet<String>>;

class AXCustomColorModeBlocklist {
    WTF_MAKE_TZONE_ALLOCATED(AXCustomColorModeBlocklist);

public:
    AXCustomColorModeBlocklist() = default;
    WEBCORE_EXPORT AXCustomColorModeBlocklist(AXCustomColorModeBlocklistURLMap urls, AXCustomColorModeBlocklistURLMap urlsIncludingSubdomains);

    const AXCustomColorModeBlocklistURLMap& urls() const LIFETIME_BOUND { return m_urls; }
    const AXCustomColorModeBlocklistURLMap& urlsIncludingSubdomains() const LIFETIME_BOUND { return m_urlsIncludingSubdomains; }

    bool matches(const URL&) const;

private:
    // List of URLs in the list. The key is the host and port (e.g: "localhost:8000"),
    // and the value is a set of paths. An URL matches this list if the domain matches
    // exactly, and at least one path in the set is a prefix of the URL's match.
    AXCustomColorModeBlocklistURLMap m_urls;

    // Identical to the above list, but the host matches if the URL's host
    // is a subdomain of domains in this map.
    AXCustomColorModeBlocklistURLMap m_urlsIncludingSubdomains;
};

} // namespace WebCore

namespace WTF {

TextStream& operator<<(TextStream&, const WebCore::AXCustomColorModeBlocklist&);

} // namespace WTF

#endif // ENABLE(AX_CUSTOM_COLOR_MODE)
