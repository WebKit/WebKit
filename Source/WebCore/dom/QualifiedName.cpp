/*
 * Copyright (C) 2005, 2006, 2009 Apple Inc. All rights reserved.
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
#include "QualifiedNameCache.h"
#include "ThreadGlobalData.h"
#include <wtf/Assertions.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StaticConstructors.h>

namespace WebCore {

QualifiedName::QualifiedName(const AtomicString& p, const AtomicString& l, const AtomicString& n)
    : m_impl(threadGlobalData().qualifiedNameCache().getOrCreate(QualifiedNameComponents { p.impl(), l.impl(), n.isEmpty() ? nullptr : n.impl() }))
{
}

QualifiedName::QualifiedNameImpl::~QualifiedNameImpl()
{
    threadGlobalData().qualifiedNameCache().remove(*this);
}

// Global init routines
DEFINE_GLOBAL(QualifiedName, anyName, nullAtom(), starAtom(), starAtom())

void QualifiedName::init()
{
    static bool initialized = false;
    if (initialized)
        return;

    // Use placement new to initialize the globals.
    AtomicString::init();
    new (NotNull, (void*)&anyName) QualifiedName(nullAtom(), starAtom(), starAtom());
    initialized = true;
}

const QualifiedName& nullQName()
{
    static NeverDestroyed<QualifiedName> nullName(nullAtom(), nullAtom(), nullAtom());
    return nullName;
}

const AtomicString& QualifiedName::localNameUpper() const
{
    if (!m_impl->m_localNameUpper)
        m_impl->m_localNameUpper = m_impl->m_localName.convertToASCIIUppercase();
    return m_impl->m_localNameUpper;
}

unsigned QualifiedName::QualifiedNameImpl::computeHash() const
{
    QualifiedNameComponents components = { m_prefix.impl(), m_localName.impl(), m_namespace.impl() };
    return hashComponents(components);
}

void createQualifiedName(void* targetAddress, StringImpl* name, const AtomicString& nameNamespace)
{
    new (NotNull, reinterpret_cast<void*>(targetAddress)) QualifiedName(nullAtom(), AtomicString(name), nameNamespace);
}

void createQualifiedName(void* targetAddress, StringImpl* name)
{
    new (NotNull, reinterpret_cast<void*>(targetAddress)) QualifiedName(nullAtom(), AtomicString(name), nullAtom());
}

}
