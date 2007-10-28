/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
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
 */

#include "config.h"

#ifdef AVOID_STATIC_CONSTRUCTORS
#define WEBCORE_QUALIFIEDNAME_HIDE_GLOBALS 1
#else
#define QNAME_DEFAULT_CONSTRUCTOR
#endif

#include "QualifiedName.h"
#include "StaticConstructors.h"
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

namespace WebCore {

struct QualifiedNameComponents {
    StringImpl* m_prefix;
    StringImpl* m_localName;
    StringImpl* m_namespace;
};

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
static const unsigned PHI = 0x9e3779b9U;
    
static inline unsigned hashComponents(const QualifiedNameComponents& buf)
{
    ASSERT(sizeof(QualifiedNameComponents) % (sizeof(uint16_t) * 2) == 0);

    unsigned l = sizeof(QualifiedNameComponents) / (sizeof(uint16_t) * 2);
    const uint16_t* s = reinterpret_cast<const uint16_t*>(&buf);
    uint32_t hash = PHI;

    // Main loop
    for (; l > 0; l--) {
        hash += s[0];
        uint32_t tmp = (s[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        s += 2;
        hash += hash >> 11;
    }
        
    // Force "avalanching" of final 127 bits
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    // this avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet", using a value that is likely to be
    // effectively the same as 0 when the low bits are masked
    if (hash == 0)
        hash = 0x80000000;

    return hash;
}

struct QNameHash {
    static unsigned hash(const QualifiedName::QualifiedNameImpl* name) {    
        QualifiedNameComponents c = { name->m_prefix.impl(), name->m_localName.impl(), name->m_namespace.impl() };
        return hashComponents(c);
    }

    static bool equal(const QualifiedName::QualifiedNameImpl* a, const QualifiedName::QualifiedNameImpl* b) { return a == b; }

    static const bool safeToCompareToEmptyOrDeleted = false;
};

typedef HashSet<QualifiedName::QualifiedNameImpl*, QNameHash> QNameSet;

struct QNameComponentsTranslator {
    static unsigned hash(const QualifiedNameComponents& components) { 
        return hashComponents(components); 
    }
    static bool equal(QualifiedName::QualifiedNameImpl* name, const QualifiedNameComponents& c) {
        return c.m_prefix == name->m_prefix.impl() && c.m_localName == name->m_localName.impl() && c.m_namespace == name->m_namespace.impl();
    }
    static void translate(QualifiedName::QualifiedNameImpl*& location, const QualifiedNameComponents& components, unsigned hash) {
        location = new QualifiedName::QualifiedNameImpl(components.m_prefix, components.m_localName, components.m_namespace);
    }
};

static QNameSet* gNameCache;

QualifiedName::QualifiedName(const AtomicString& p, const AtomicString& l, const AtomicString& n)
    : m_impl(0)
{
    if (!gNameCache)
        gNameCache = new QNameSet;
    QualifiedNameComponents components = { p.impl(), l.impl(), n.impl() };
    m_impl = *gNameCache->add<QualifiedNameComponents, QNameComponentsTranslator>(components).first;    
    ref();
}

QualifiedName::~QualifiedName()
{
    deref();
}

QualifiedName::QualifiedName(const QualifiedName& other)
{
    m_impl = other.m_impl;
    ref();
}

const QualifiedName& QualifiedName::operator=(const QualifiedName& other)
{
    if (m_impl != other.m_impl) {
        deref();
        m_impl = other.m_impl;
        ref();
    }
    
    return *this;
}

void QualifiedName::deref()
{
#ifdef QNAME_DEFAULT_CONSTRUCTOR
    if (!m_impl)
        return;
#endif

    if (m_impl->hasOneRef())
        gNameCache->remove(m_impl);
    m_impl->deref();
}

void QualifiedName::setPrefix(const AtomicString& prefix)
{
    QualifiedName other(prefix, localName(), namespaceURI());
    *this = other;
}

String QualifiedName::toString() const
{
    String local = localName();
    if (hasPrefix())
        return prefix() + ":" + local;
    return local;
}

// Global init routines
DEFINE_GLOBAL(QualifiedName, anyName, nullAtom, starAtom, starAtom)

void QualifiedName::init()
{
    static bool initialized;
    if (!initialized) {
        // Use placement new to initialize the globals.
        
        AtomicString::init();
        new ((void*)&anyName) QualifiedName(nullAtom, starAtom, starAtom);
        initialized = true;
    }
}

}
