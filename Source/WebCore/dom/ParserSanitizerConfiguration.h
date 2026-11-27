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

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

struct SanitizerCanonicalConfig;

enum class SanitizerAction : uint8_t { Keep, Remove, ReplaceWithChildren };

// Hash-based view of a canonical config for per-token queries while parsing;
// the "parser sanitizer configuration".
// https://html.spec.whatwg.org/multipage/parsing.html#parser-sanitizer-configuration
class ParserSanitizerConfiguration : public RefCounted<ParserSanitizerConfiguration> {
    WTF_MAKE_TZONE_ALLOCATED(ParserSanitizerConfiguration);
public:
    static Ref<ParserSanitizerConfiguration> create(const SanitizerCanonicalConfig&);

    SanitizerAction actionForElement(const AtomString& localName, const AtomString& namespaceURI) const;
    bool allowsAttribute(const AtomString& localName, const AtomString& namespaceURI, const AtomString& value, const AtomString& elementLocalName, const AtomString& elementNamespaceURI) const;
    bool allowsComments() const { return m_comments; }
    bool allowsProcessingInstructionTarget(const String& target) const;

private:
    explicit ParserSanitizerConfiguration(const SanitizerCanonicalConfig&);

    struct LocalAttributeLists {
        std::optional<HashSet<String>> attributes;
        std::optional<HashSet<String>> removeAttributes;
    };

    std::optional<HashMap<String, LocalAttributeLists>> m_elements;
    HashSet<String> m_removeElements;
    HashSet<String> m_replaceWithChildrenElements;
    std::optional<HashSet<String>> m_attributes;
    HashSet<String> m_removeAttributes;
    std::optional<HashSet<String>> m_processingInstructions;
    HashSet<String> m_removeProcessingInstructions;
    bool m_comments { false };
    bool m_dataAttributes { false };
    bool m_javascriptURLs { false };
};

} // namespace WebCore
