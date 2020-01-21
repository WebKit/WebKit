/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <wtf/HashTraits.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

struct QualifiedNameComponents {
    StringImpl* m_prefix;
    StringImpl* m_localName;
    StringImpl* m_namespace;
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(QualifiedName);
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(QualifiedNameQualifiedNameImpl);

class QualifiedName {
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(QualifiedName);
public:
    class QualifiedNameImpl : public RefCounted<QualifiedNameImpl> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(QualifiedNameQualifiedNameImpl);
    public:
        static Ref<QualifiedNameImpl> create(const AtomString& prefix, const AtomString& localName, const AtomString& namespaceURI)
        {
            return adoptRef(*new QualifiedNameImpl(prefix, localName, namespaceURI));
        }

        WEBCORE_EXPORT ~QualifiedNameImpl();

        unsigned computeHash() const;

        mutable unsigned m_existingHash { 0 };
        const AtomString m_prefix;
        const AtomString m_localName;
        const AtomString m_namespace;
        mutable AtomString m_localNameUpper;

#if ENABLE(JIT)
        static ptrdiff_t localNameMemoryOffset() { return OBJECT_OFFSETOF(QualifiedNameImpl, m_localName); }
        static ptrdiff_t namespaceMemoryOffset() { return OBJECT_OFFSETOF(QualifiedNameImpl, m_namespace); }
#endif

    private:
        QualifiedNameImpl(const AtomString& prefix, const AtomString& localName, const AtomString& namespaceURI)
            : m_prefix(prefix)
            , m_localName(localName)
            , m_namespace(namespaceURI)
        {
            ASSERT(!namespaceURI.isEmpty() || namespaceURI.isNull());
        }        
    };

    WEBCORE_EXPORT QualifiedName(const AtomString& prefix, const AtomString& localName, const AtomString& namespaceURI);
    explicit QualifiedName(WTF::HashTableDeletedValueType) : m_impl(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_impl.isHashTableDeletedValue(); }
#ifdef QNAME_DEFAULT_CONSTRUCTOR
    QualifiedName() { }
#endif

    bool operator==(const QualifiedName& other) const { return m_impl == other.m_impl; }
    bool operator!=(const QualifiedName& other) const { return !(*this == other); }

    bool matches(const QualifiedName& other) const { return m_impl == other.m_impl || (localName() == other.localName() && namespaceURI() == other.namespaceURI()); }

    bool hasPrefix() const { return !m_impl->m_prefix.isNull(); }
    void setPrefix(const AtomString& prefix) { *this = QualifiedName(prefix, localName(), namespaceURI()); }

    const AtomString& prefix() const { return m_impl->m_prefix; }
    const AtomString& localName() const { return m_impl->m_localName; }
    const AtomString& namespaceURI() const { return m_impl->m_namespace; }

    // Uppercased localName, cached for efficiency
    const AtomString& localNameUpper() const;

    String toString() const;

    QualifiedNameImpl* impl() const { return m_impl.get(); }
#if ENABLE(JIT)
    static ptrdiff_t implMemoryOffset() { return OBJECT_OFFSETOF(QualifiedName, m_impl); }
#endif
    
    // Init routine for globals
    WEBCORE_EXPORT static void init();

private:
    static QualifiedNameImpl* hashTableDeletedValue() { return RefPtr<QualifiedNameImpl>::PtrTraits::hashTableDeletedValue(); }
    
    RefPtr<QualifiedNameImpl> m_impl;
};

extern LazyNeverDestroyed<const QualifiedName> anyName;
inline const QualifiedName& anyQName() { return anyName; }

const QualifiedName& nullQName();

inline bool operator==(const AtomString& a, const QualifiedName& q) { return a == q.localName(); }
inline bool operator!=(const AtomString& a, const QualifiedName& q) { return a != q.localName(); }
inline bool operator==(const QualifiedName& q, const AtomString& a) { return a == q.localName(); }
inline bool operator!=(const QualifiedName& q, const AtomString& a) { return a != q.localName(); }

inline unsigned hashComponents(const QualifiedNameComponents& buf)
{
    return StringHasher::hashMemory<sizeof(QualifiedNameComponents)>(&buf);
}

struct QualifiedNameHash {
    static unsigned hash(const QualifiedName& name) { return hash(name.impl()); }

    static unsigned hash(const QualifiedName::QualifiedNameImpl* name) 
    {
        if (!name->m_existingHash)
            name->m_existingHash = name->computeHash();
        return name->m_existingHash;
    }

    static bool equal(const QualifiedName& a, const QualifiedName& b) { return a == b; }
    static bool equal(const QualifiedName::QualifiedNameImpl* a, const QualifiedName::QualifiedNameImpl* b) { return a == b; }

    static const bool safeToCompareToEmptyOrDeleted = false;
};

inline String QualifiedName::toString() const
{
    if (!hasPrefix())
        return localName();

    return prefix().string() + ':' + localName().string();
}

} // namespace WebCore

namespace WTF {
    
    template<typename T> struct DefaultHash;

    template<> struct DefaultHash<WebCore::QualifiedName> {
        typedef WebCore::QualifiedNameHash Hash;
    };
    
    template<> struct HashTraits<WebCore::QualifiedName> : SimpleClassHashTraits<WebCore::QualifiedName> {
        static const bool emptyValueIsZero = false;
        static WebCore::QualifiedName emptyValue() { return WebCore::nullQName(); }
    };

} // namespace WTF
