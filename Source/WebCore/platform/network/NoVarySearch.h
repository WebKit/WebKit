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

#pragma once

#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ResourceResponse;

// https://httpwg.org/http-extensions/draft-ietf-httpbis-no-vary-search.html
// A "URL variation config", describing which parts of a URL's query do not affect the response.
class NoVarySearch {
public:
    // Returns the default config (query must match exactly) if the header is absent or invalid.
    WEBCORE_EXPORT static NoVarySearch parse(StringView headerValue);
    WEBCORE_EXPORT static NoVarySearch obtain(const ResourceResponse&);

    NoVarySearch() = default;

    bool isDefault() const { return m_varyOnKeyOrder && !m_noVaryParamsIsWildcard && m_noVaryParams.isEmpty(); }

    // https://httpwg.org/http-extensions/draft-ietf-httpbis-no-vary-search.html#name-comparing
    WEBCORE_EXPORT bool equivalent(const URL&, const URL&) const;

    // For testing.
    bool varyOnKeyOrder() const { return m_varyOnKeyOrder; }
    bool noVaryParamsIsWildcard() const { return m_noVaryParamsIsWildcard; }
    const Vector<String>& noVaryParams() const LIFETIME_BOUND { return m_noVaryParams; }
    const Vector<String>& varyParams() const LIFETIME_BOUND { return m_varyParams; }

private:
    // When no-vary params is a wildcard, only m_varyParams matter. Otherwise vary params is the wildcard and only m_noVaryParams matter.
    bool m_noVaryParamsIsWildcard { false };
    Vector<String> m_noVaryParams;
    Vector<String> m_varyParams;
    bool m_varyOnKeyOrder { true };
};

} // namespace WebCore
