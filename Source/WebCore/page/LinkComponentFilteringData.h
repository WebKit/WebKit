/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/RegistrableDomain.h>
#include <optional>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class LinkComponent : uint8_t {
    Query,
    Path,
    Subdomain,
    Fragment
};

// A rule describing a tracking identifier that lives in a part of the URL other than the query string.
// When a rule matches, the whole component is hidden from script: the path becomes "/", all subdomains
// are dropped, or the fragment is removed. Query rules hide only the matching parameter.
struct LinkComponentFilteringData {
    LinkComponent component { LinkComponent::Query };
    String queryParameter; // Query only.
    unsigned index { 0 }; // 1-based. Path and Subdomain only.
    std::optional<RegistrableDomain> domain; // std::nullopt matches any domain. Only query rules may omit it.
    String subdomain; // Empty matches any subdomain.
    String path; // Empty matches any path.
    String pattern; // Empty matches any value.
};

}
