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

#include "config.h"
#include "AXCustomColorModeBlocklist.h"

#if ENABLE(AX_CUSTOM_COLOR_MODE)

#include <wtf/HashMap.h>
#include <wtf/URL.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AXCustomColorModeBlocklist);

AXCustomColorModeBlocklist::AXCustomColorModeBlocklist(AXCustomColorModeBlocklistURLMap urls, AXCustomColorModeBlocklistURLMap urlsIncludingSubdomains)
    : m_urls(WTF::move(urls))
    , m_urlsIncludingSubdomains(WTF::move(urlsIncludingSubdomains))
{
    // FIXME: add '/' to end of path
    // FIXME: normalize URL by removing leading "www."
}

bool AXCustomColorModeBlocklist::matches(const URL& url) const
{
    auto hostAndPort = url.hostAndPort();
    if (hostAndPort.startsWith("www."_s))
        hostAndPort = hostAndPort.substring(4);

    auto urlPath = url.path().toString();
    if (!urlPath.endsWith('/'))
        urlPath = makeString(urlPath, '/');

    for (const auto& path : m_urls.get(hostAndPort)) {
        auto path2 = path;
        if (!path2.endsWith('/'))
            path2 = makeString(path2, '/');

        if (urlPath.startsWith(path2))
            return true;
    }

    while (true) {
        for (const auto& path : m_urlsIncludingSubdomains.get(hostAndPort)) {
            auto path2 = path;
            if (!path2.endsWith('/'))
                path2 = makeString(path2, '/');

            if (urlPath.startsWith(path2))
                return true;
        }

        auto nextDot = hostAndPort.find('.');
        if (nextDot == notFound)
            break;

        // FIXME: what if it's something like example.com.:1234
        hostAndPort = hostAndPort.substring(nextDot + 1);
    }

    return false;
}

} // namespace WebCore

namespace WTF {

TextStream& operator<<(TextStream& ts, const WebCore::AXCustomColorModeBlocklist& list)
{
    ts << "Exact host matches: ";
    ts.increaseIndent();

    for (const auto& [host, paths] : list.urls()) {
        ts.nextLine();
        ts << "- " << host;
        ts.increaseIndent();

        for (const auto& path : paths) {
            ts.nextLine();
            ts << "- path: " << path;
        }

        ts.decreaseIndent();
    }

    ts.decreaseIndent();

    ts << "Host matches subdomains: ";
    ts.increaseIndent();

    for (const auto& [host, paths] : list.urlsIncludingSubdomains()) {
        ts.nextLine();
        ts << "- " << host;
        ts.increaseIndent();

        for (const auto& path : paths) {
            ts.nextLine();
            ts << "- path: " << path;
        }

        ts.decreaseIndent();
    }

    ts.decreaseIndent();

    return ts;
}

} // namespace WTF

#endif // ENABLE(AX_CUSTOM_COLOR_MODE)
