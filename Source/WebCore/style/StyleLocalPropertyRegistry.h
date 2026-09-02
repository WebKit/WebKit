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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSRegisteredCustomProperty.h"
#include <wtf/HashMap.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {
namespace Style {

// Lightweight property registration for custom function evaluation.
// Unlike CustomPropertyRegistry, this has no association with a Style::Scope
// and no invalidation or prototype style management.
class LocalPropertyRegistry {
public:
    explicit LocalPropertyRegistry(const LocalPropertyRegistry* enclosing = nullptr)
        : m_enclosing(enclosing)
    { }

    // The registration for a name, from this function or an enclosing one.
    const CSSRegisteredCustomProperty* get(const AtomString&) const;
    // Whether this function declares the name itself. A name declared by an enclosing function has its
    // value inherited from the calling context rather than resolving to a registered value here.
    bool declares(const AtomString&) const;
    bool isInherited(const AtomString&) const;

    void add(CSSRegisteredCustomProperty&&);

private:
    // The registry of the function this one was invoked from, so a typed parameter keeps its type across
    // frames. https://github.com/w3c/csswg-drafts/issues/12315
    const LocalPropertyRegistry* m_enclosing { nullptr };
    UncheckedKeyHashMap<AtomString, UniqueRef<CSSRegisteredCustomProperty>> m_properties;
};

} // namespace Style
} // namespace WebCore
