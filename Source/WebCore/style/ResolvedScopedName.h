/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleScopeIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class Element;

namespace Style {

class Scope;

struct ScopedName;

// A ScopedName that has been resolved to determine which specific scope it belongs to.
class ResolvedScopedName {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ResolvedScopedName);
public:
    ResolvedScopedName(AtomString, ScopeIdentifier);

    // Creates a ResolvedScopedName from a ScopedName by resolving the scope ordinal
    // into the concrete Style::Scope object.
    static ResolvedScopedName createFromScopedName(const Element&, const ScopedName&);

    AtomString name() const { return m_name; }
    ScopeIdentifier scopeIdentifier() const { return m_scopeIdentifier; }

    friend bool operator==(const ResolvedScopedName& lhs, const ResolvedScopedName& rhs) = default;

private:
    AtomString m_name;
    ScopeIdentifier m_scopeIdentifier;
};

inline void add(Hasher& hasher, const ResolvedScopedName& name)
{
    add(hasher, name.name(), name.scopeIdentifier());
}

} // namespace Style

} // namespace WebCore

namespace WTF {

template<>
struct HashTraits<WebCore::Style::ResolvedScopedName> : GenericHashTraits<WebCore::Style::ResolvedScopedName> {
    static const bool emptyValueIsZero = true;

    static WebCore::Style::ResolvedScopedName emptyValue()
    {
        return { WTF::nullAtom(), HashTraits<WebCore::Style::ScopeIdentifier>::emptyValue() };
    }

    static void constructDeletedValue(WebCore::Style::ResolvedScopedName& slot)
    {
        new (NotNull, std::addressof(slot)) WebCore::Style::ResolvedScopedName { HashTableDeletedValue, HashTableDeletedValue };
    }

    static bool isDeletedValue(const WebCore::Style::ResolvedScopedName& value)
    {
        return value.name().isHashTableDeletedValue() && value.scopeIdentifier().isHashTableDeletedValue();
    }
};

} // namespace WTF
