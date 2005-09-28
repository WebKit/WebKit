/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define KHTML_QNAME_HIDE_GLOBALS 1

#include "dom_qname.h"
#include <kxmlcore/HashSet.h>

namespace DOM {

struct QualifiedNameComponents {
    DOMStringImpl *m_prefix;
    DOMStringImpl *m_localName;
    DOMStringImpl *m_namespace;
};

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
static const unsigned PHI = 0x9e3779b9U;
    
inline unsigned hashComponents(const QualifiedNameComponents& buf)
{
    unsigned l = sizeof(QualifiedNameComponents) / sizeof(uint16_t);
    const uint16_t *s = reinterpret_cast<const uint16_t*>(&buf);
    uint32_t hash = PHI;
    uint32_t tmp;
        
    int rem = l & 1;
    l >>= 1;
        
    // Main loop
    for (; l > 0; l--) {
        hash += s[0];
        tmp = (s[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        s += 2;
        hash += hash >> 11;
    }
    
    // Handle end case
    if (rem) {
        hash += s[0];
        hash ^= hash << 11;
        hash += hash >> 17;
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
    static unsigned hash(const QualifiedName::QualifiedNameImpl *name) {    
        QualifiedNameComponents components = { name->m_prefix.impl(), 
                                               name->m_localName.impl(), 
                                               name->m_namespace.impl() };
        return hashComponents(components);
    }
    static bool equal(const QualifiedName::QualifiedNameImpl *a, const QualifiedName::QualifiedNameImpl *b) { return a == b; }
};

typedef HashSet<QualifiedName::QualifiedNameImpl*, QNameHash> QNameSet;

static QNameSet *gNameCache;

inline bool equalComponents(QualifiedName::QualifiedNameImpl* const& name, const QualifiedNameComponents &components)
{
    return components.m_localName == name->m_localName.impl() &&
           components.m_namespace == name->m_namespace.impl() &&
           components.m_prefix == name->m_prefix.impl();
}

inline QualifiedName::QualifiedNameImpl *convertComponents(const QualifiedNameComponents& components, unsigned hash)
{
    return new QualifiedName::QualifiedNameImpl(components.m_prefix, components.m_localName, components.m_namespace);
}


QualifiedName::QualifiedName(const AtomicString& p, const AtomicString& l, const AtomicString& n)
    : m_impl(0)
{
    if (!gNameCache)
        gNameCache = new QNameSet;
    QualifiedNameComponents components = { p.impl(), l.impl(), n.impl() };
    m_impl = *gNameCache->insert<QualifiedNameComponents, hashComponents, equalComponents, convertComponents>(components).first;    
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
    if (m_impl->hasOneRef())
        gNameCache->remove(m_impl);
        
    m_impl->deref();
}

void QualifiedName::setPrefix(const AtomicString& prefix)
{
    QualifiedName other(prefix, localName(), namespaceURI());
    *this = other;
}

DOMString QualifiedName::toString() const
{
    DOMString local = localName();
    if (hasPrefix())
        return DOMString(prefix()) + ":" + local;
    return local;
}

// Global init routines
void* anyName[(sizeof(QualifiedName) + sizeof(void*) - 1) / sizeof(void*)];

void QualifiedName::init()
{
    static bool initialized;
    if (!initialized) {
        // Use placement new to initialize the globals.
        new (&anyName) QualifiedName(nullAtom, starAtom, starAtom);
        initialized = true;
    }
}

}
