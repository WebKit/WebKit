/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <WebCore/CSSCounterStyle.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

namespace Style {
struct CounterStyle;
}

class StyleRuleCounterStyle;
enum CSSValueID : uint16_t;

using CounterStyleMap = HashMap<AtomString, Ref<CSSCounterStyle>>;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSCounterStyleRegistry);
class CSSCounterStyleRegistry {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSCounterStyleRegistry, CSSCounterStyleRegistry);
public:
    CSSCounterStyleRegistry() = default;

    static Ref<CSSCounterStyle> decimalCounter();

    Ref<CSSCounterStyle> resolvedCounterStyle(const Style::CounterStyle&);
    void resolveReferencesIfNeeded();

    void addCounterStyle(const CSSCounterStyleDescriptors&);

    static void addUserAgentCounterStyle(const CSSCounterStyleDescriptors&);
    static void resolveUserAgentReferences();

    bool hasAuthorCounterStyles() const { return !m_authorCounterStyles.isEmpty(); }
    void clearAuthorCounterStyles();

    bool operator==(const CSSCounterStyleRegistry&) const;

private:
    static CounterStyleMap& userAgentCounterStyles();

    // If no map is passed on, user-agent counter styles map will be used
    static void resolveFallbackReference(CSSCounterStyle&, CounterStyleMap* = nullptr);
    static void resolveExtendsReference(CSSCounterStyle&, CounterStyleMap* = nullptr);
    static void resolveExtendsReference(CSSCounterStyle&, HashSet<CSSCounterStyle*>&, CounterStyleMap* = nullptr);

    static Ref<CSSCounterStyle> counterStyle(const AtomString&, CounterStyleMap* = nullptr);

    void invalidate();

    CounterStyleMap m_authorCounterStyles;
    bool m_hasUnresolvedReferences { true };
};

} // namespace WebCore
