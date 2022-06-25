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

#include "ContentSecurityPolicy.h"
#include "ContentSecurityPolicyHash.h"
#include "ContentSecurityPolicySource.h"
#include <wtf/OptionSet.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ContentSecurityPolicy;

class ContentSecurityPolicySourceList {
public:
    ContentSecurityPolicySourceList(const ContentSecurityPolicy&, const String& directiveName);

    void parse(const String&);

    bool matches(const URL&, bool didReceiveRedirectResponse) const;
    bool matches(const Vector<ContentSecurityPolicyHash>&) const;
    bool matchesAll(const Vector<ContentSecurityPolicyHash>&) const;
    bool matches(const String& nonce) const;

    OptionSet<ContentSecurityPolicyHashAlgorithm> hashAlgorithmsUsed() const { return m_hashAlgorithmsUsed; }

    bool allowInline() const { return m_allowInline && m_hashes.isEmpty() && m_nonces.isEmpty(); }
    bool allowEval() const { return m_allowEval; }
    bool allowWasmEval() const { return m_allowWasmEval; }
    bool allowSelf() const { return m_allowSelf; }
    bool isNone() const { return m_isNone; }
    bool allowNonParserInsertedScripts() const { return m_allowNonParserInsertedScripts; }
    bool allowUnsafeHashes() const { return m_allowUnsafeHashes; }
    bool shouldReportSample() const { return m_reportSample; }

private:
    struct Host {
        StringView value;
        bool hasWildcard { false };
    };
    struct Port {
        std::optional<uint16_t> value;
        bool hasWildcard { false };
    };
    struct Source {
        StringView scheme;
        Host host;
        Port port;
        String path;
    };

    bool isProtocolAllowedByStar(const URL&) const;
    bool isValidSourceForExtensionMode(const ContentSecurityPolicySourceList::Source&);
    template<typename CharacterType> void parse(StringParsingBuffer<CharacterType>);
    template<typename CharacterType> std::optional<Source> parseSource(StringParsingBuffer<CharacterType>);
    template<typename CharacterType> StringView parseScheme(StringParsingBuffer<CharacterType>);
    template<typename CharacterType> std::optional<Host> parseHost(StringParsingBuffer<CharacterType>);
    template<typename CharacterType> std::optional<Port> parsePort(StringParsingBuffer<CharacterType>);
    template<typename CharacterType> String parsePath(StringParsingBuffer<CharacterType>);
    template<typename CharacterType> bool parseNonceSource(StringParsingBuffer<CharacterType>);
    template<typename CharacterType> bool parseHashSource(StringParsingBuffer<CharacterType>);

    const ContentSecurityPolicy& m_policy;
    Vector<ContentSecurityPolicySource> m_list;
    MemoryCompactLookupOnlyRobinHoodHashSet<String> m_nonces;
    HashSet<ContentSecurityPolicyHash> m_hashes;
    OptionSet<ContentSecurityPolicyHashAlgorithm> m_hashAlgorithmsUsed;
    String m_directiveName;
    ContentSecurityPolicyModeForExtension m_contentSecurityPolicyModeForExtension { ContentSecurityPolicyModeForExtension::None };
    bool m_allowSelf { false };
    bool m_allowStar { false };
    bool m_allowInline { false };
    bool m_allowEval { false };
    bool m_allowWasmEval { false };
    bool m_isNone { false };
    bool m_allowNonParserInsertedScripts { false };
    bool m_allowUnsafeHashes { false };
    bool m_reportSample { false };
};

} // namespace WebCore
