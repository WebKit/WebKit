/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#include <wtf/text/WTFString.h>

namespace WebCore {

class ContentSecurityPolicy;
struct SecurityOriginData;

class ContentSecurityPolicySource {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ContentSecurityPolicySource(const ContentSecurityPolicy&, const String& scheme, const String& host, Optional<uint16_t> port, const String& path, bool hostHasWildcard, bool portHasWildcard);

    bool matches(const URL&, bool didReceiveRedirectResponse = false) const;

    operator SecurityOriginData() const;

private:
    bool schemeMatches(const URL&) const;
    bool hostMatches(const URL&) const;
    bool pathMatches(const URL&) const;
    bool portMatches(const URL&) const;
    bool isSchemeOnly() const;

    const ContentSecurityPolicy& m_policy;
    String m_scheme;
    String m_host;
    String m_path;
    Optional<uint16_t> m_port;

    bool m_hostHasWildcard;
    bool m_portHasWildcard;
};

} // namespace WebCore
