/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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

#include "CSSCounterStyle.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class StyleRuleCounterStyle;
using CounterStyleMap = HashMap<AtomString, RefPtr<CSSCounterStyle>>;

class CSSCounterStyleRegistry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RefPtr<CSSCounterStyle> resolvedCounterStyle(const AtomString&);
    RefPtr<CSSCounterStyle> decimalCounter() const;
    void addCounterStyle(const CSSCounterStyleDescriptors&);
    void resolveReferencesIfNeeded();
    bool operator==(const CSSCounterStyleRegistry& other) const;
    CSSCounterStyleRegistry() = default;

private:
    static CounterStyleMap& userAgentCounterStyles();
    void resolveFallbackReference(CSSCounterStyle&);
    void resolveExtendsReference(CSSCounterStyle&);
    void resolveExtendsReference(CSSCounterStyle&, HashSet<CSSCounterStyle*>&);
    RefPtr<CSSCounterStyle> counterStyle(const AtomString&);

    CounterStyleMap m_authorCounterStyles;
    bool hasUnresolvedReferences { true };
};

} // namespace WebCore
