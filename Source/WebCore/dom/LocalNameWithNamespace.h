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

#include <WebCore/QualifiedName.h>

namespace WebCore {

// An element or attribute name without a namespace prefix: a (namespace, local name) pair.
// Use this in HashMap/HashSet keys and in matching predicates where two names with the same
// expanded name should be treated as identical regardless of how they were prefixed.
class LocalNameWithNamespace {
public:
    LocalNameWithNamespace(const AtomString& namespaceURI, const AtomString& localName)
        : m_canonical(nullAtom(), localName, namespaceURI) { }
    LocalNameWithNamespace(const QualifiedName& qualifiedName)
        : m_canonical(qualifiedName.hasPrefix()
            ? QualifiedName(nullAtom(), qualifiedName.localName(), qualifiedName.namespaceURI())
            : qualifiedName) { }
    template<typename T>
    LocalNameWithNamespace(const LazyNeverDestroyed<T>& lazy)
        : LocalNameWithNamespace(lazy.get()) { }
    explicit LocalNameWithNamespace(WTF::HashTableDeletedValueType)
        : m_canonical(WTF::HashTableDeletedValue) { }

    const AtomString& namespaceURI() const LIFETIME_BOUND { return m_canonical.namespaceURI(); }
    const AtomString& localName() const LIFETIME_BOUND { return m_canonical.localName(); }

    bool isHashTableDeletedValue() const { return m_canonical.isHashTableDeletedValue(); }
    QualifiedName::QualifiedNameImpl* impl() const { return m_canonical.impl(); }

    friend bool operator==(const LocalNameWithNamespace&, const LocalNameWithNamespace&) = default;

private:
    QualifiedName m_canonical;
};

struct LocalNameWithNamespaceHash {
    static unsigned hash(const LocalNameWithNamespace& name) { return QualifiedNameHash::hash(name.impl()); }
    static bool equal(const LocalNameWithNamespace& a, const LocalNameWithNamespace& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
    static constexpr bool hasHashInValue = true;
};

} // namespace WebCore

namespace WTF {

template<typename T> struct DefaultHash;

template<> struct DefaultHash<WebCore::LocalNameWithNamespace> : WebCore::LocalNameWithNamespaceHash { };

template<> struct HashTraits<WebCore::LocalNameWithNamespace> : SimpleClassHashTraits<WebCore::LocalNameWithNamespace> {
    static const bool emptyValueIsZero = false;
    static WebCore::LocalNameWithNamespace emptyValue() { return { nullAtom(), nullAtom() }; }
};

} // namespace WTF
