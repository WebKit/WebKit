/*
 * Copyright (C) 2026 Helmut Januschka <helmut@januschka.com>
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
#include "ConnectionAllowlist.h"

#include "RFC8941.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include <wtf/Borrow.h>

namespace WebCore {

// https://w3c.github.io/connection-allowlists/#parse-a-connection-allowlist-header
std::unique_ptr<ConnectionAllowlist> parseConnectionAllowlist(const ResourceResponse& response, HTTPHeaderName headerName)
{
    auto header = response.httpHeaderField(headerName);
    if (header.isEmpty())
        return nullptr;

    auto parsedHeader = RFC8941::parseListStructuredFieldValue(header);
    if (!parsedHeader || parsedHeader->isEmpty())
        return nullptr;

    // Servers may deliver a list with an arbitrary number of items, but only the first is
    // used, and it has to be an inner list.
    auto borrowedHeader = borrow(*parsedHeader);
    const auto& [firstItem, parameters] = borrowedHeader[0];
    const auto* innerList = std::get_if<RFC8941::InnerList>(&firstItem);
    if (!innerList)
        return nullptr;

    auto allowlist = makeUnique<ConnectionAllowlist>();
    allowlist->disposition = headerName == HTTPHeaderName::ConnectionAllowlistReportOnly
        ? ConnectionAllowlist::Disposition::Report
        : ConnectionAllowlist::Disposition::Enforce;

    for (const auto& [item, itemParameters] : borrow(*innerList)) {
        if (const auto* token = std::get_if<RFC8941::Token>(&item)) {
            if (token->string() == "response-origin"_s)
                allowlist->patterns.append(SecurityOrigin::create(response.url())->toString());
            continue;
        }
        if (const auto* pattern = std::get_if<String>(&item)) {
            allowlist->patterns.append(*pattern);
            continue;
        }
        // Unexpected values are ignored.
    }

    // All parameters other than these three are ignored, and a parameter whose value is not
    // a token leaves the default in place.
    if (auto* reportTo = parameters.getIf<RFC8941::Token>("report-to"_s))
        allowlist->reportTo = reportTo->string();
    if (auto* redirects = parameters.getIf<RFC8941::Token>("redirects"_s))
        allowlist->redirects = redirects->string() == "block"_s ? ConnectionAllowlist::RedirectHandling::Block : ConnectionAllowlist::RedirectHandling::Allow;
    if (auto* webrtc = parameters.getIf<RFC8941::Token>("webrtc"_s))
        allowlist->webrtc = webrtc->string() == "block"_s ? ConnectionAllowlist::WebRTCHandling::Block : ConnectionAllowlist::WebRTCHandling::Allow;

    return allowlist;
}

} // namespace WebCore
