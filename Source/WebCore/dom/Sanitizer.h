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

#include "ExceptionOr.h"
#include "ParserSanitizerConfiguration.h"
#include "SanitizerConfig.h"
#include "SanitizerPresets.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

// Canonical form of a SanitizerConfig, as produced by the "canonicalize the
// configuration" algorithm: every name is a { name, namespace } pair where a
// null AtomString namespace means "no namespace".
// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizerconfig-canonicalize
struct SanitizerCanonicalName {
    AtomString name;
    AtomString namespaceURI;
    friend bool operator==(const SanitizerCanonicalName&, const SanitizerCanonicalName&) = default;
};

struct SanitizerCanonicalElement {
    SanitizerCanonicalName name;
    std::optional<Vector<SanitizerCanonicalName>> attributes;
    std::optional<Vector<SanitizerCanonicalName>> removeAttributes;
    friend bool operator==(const SanitizerCanonicalElement&, const SanitizerCanonicalElement&) = default;
};

struct SanitizerCanonicalConfig {
    std::optional<Vector<SanitizerCanonicalElement>> elements;
    std::optional<Vector<SanitizerCanonicalName>> removeElements;
    std::optional<Vector<SanitizerCanonicalName>> replaceWithChildrenElements;
    std::optional<Vector<AtomString>> processingInstructions;
    std::optional<Vector<AtomString>> removeProcessingInstructions;
    std::optional<Vector<SanitizerCanonicalName>> attributes;
    std::optional<Vector<SanitizerCanonicalName>> removeAttributes;
    bool comments { false };
    std::optional<bool> dataAttributes;
    bool javascriptURLs { false };
};

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#the-sanitizer-interface
class Sanitizer;
using SanitizerBinding = Variant<Ref<Sanitizer>, SanitizerConfig, SanitizerPresets>;

class Sanitizer : public RefCounted<Sanitizer> {
    WTF_MAKE_TZONE_ALLOCATED(Sanitizer);
public:
    static ExceptionOr<Ref<Sanitizer>> create(std::optional<Variant<SanitizerConfig, SanitizerPresets>>&&);
    static Ref<Sanitizer> create(SanitizerCanonicalConfig&&);

    SanitizerConfig get() const;

    bool allowElement(SanitizerElementWithAttributes&&);
    bool removeElement(SanitizerElement&&);
    bool replaceElementWithChildren(SanitizerElement&&);
    bool allowProcessingInstruction(SanitizerPI&&);
    bool removeProcessingInstruction(SanitizerPI&&);
    bool allowAttribute(SanitizerAttribute&&);
    bool removeAttribute(SanitizerAttribute&&);
    bool setComments(bool allow);
    bool setDataAttributes(bool allow);
    bool setJavascriptURLs(bool allow);
    bool removeUnsafe();

    const SanitizerCanonicalConfig& configuration() const { return m_configuration; }

    // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#get-a-sanitizer-config-from-options
    static ExceptionOr<RefPtr<ParserSanitizerConfiguration>> parserConfigurationFromOptions(std::optional<SanitizerBinding>&&, bool safe);

    // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#built-in-safe-default-configuration
    static SanitizerCanonicalConfig defaultConfiguration();
    static ExceptionOr<SanitizerCanonicalConfig> canonicalizeAndValidate(const SanitizerConfig&, bool permissiveDefaults);
    static bool removeUnsafe(SanitizerCanonicalConfig&);

private:
    explicit Sanitizer(SanitizerCanonicalConfig&&);

    SanitizerCanonicalConfig m_configuration;
};

} // namespace WebCore
