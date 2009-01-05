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

#ifdef SKIP_STATIC_CONSTRUCTORS_ON_GCC
#define WEBCORE_QUALIFIEDNAME_HIDE_GLOBALS 1
#else
#define QNAME_DEFAULT_CONSTRUCTOR
#endif

#include "QualifiedName.h"
#include "StaticConstructors.h"
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

namespace WebCore {

typedef HashSet<QualifiedName::QualifiedNameImpl*, QualifiedNameHash> QNameSet;

struct QNameComponentsTranslator {
    static unsigned hash(const QualifiedNameComponents& components)
    {
        return hashComponents(components); 
    }
    static bool equal(QualifiedName::QualifiedNameImpl* name, const QualifiedNameComponents& c)
    {
        return c.m_prefix == name->m_prefix.impl() && c.m_localName == name->m_localName.impl() && c.m_namespace == name->m_namespace.impl();
    }
    static void translate(QualifiedName::QualifiedNameImpl*& location, const QualifiedNameComponents& components, unsigned)
    {
        location = QualifiedName::QualifiedNameImpl::create(components.m_prefix, components.m_localName, components.m_namespace).releaseRef();
    }
};

static QNameSet* gNameCache;

QualifiedName::QualifiedName(const AtomicString& p, const AtomicString& l, const AtomicString& n)
    : m_impl(0)
{
    if (!gNameCache)
        gNameCache = new QNameSet;
    QualifiedNameComponents components = { p.impl(), l.impl(), n.impl() };
    pair<QNameSet::iterator, bool> addResult = gNameCache->add<QualifiedNameComponents, QNameComponentsTranslator>(components);
    m_impl = *addResult.first;    
    if (!addResult.second)
        m_impl->ref();
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
