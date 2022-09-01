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
#include "QualifiedName.h"

#include "CommonAtomStrings.h"
#include "ElementName.h"
#include "Namespace.h"
#include "QualifiedNameCache.h"
#include "ThreadGlobalData.h"
#include <wtf/Assertions.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(QualifiedName);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(QualifiedNameQualifiedNameImpl);

QualifiedName::QualifiedNameImpl::QualifiedNameImpl(const AtomString& prefix, const AtomString& localName, const AtomString& namespaceURI)
    : m_namespace(Namespace::Unknown)
    , m_elementName(ElementName::Unknown)
    , m_prefix(prefix)
    , m_localName(localName)
    , m_namespaceURI(namespaceURI)
{
    ASSERT(!namespaceURI.isEmpty() || namespaceURI.isNull());
}

static QualifiedNameComponents makeComponents(const AtomString& prefix, const AtomString& localName, const AtomString& namespaceURI)
{
    return { prefix.impl(), localName.impl(), namespaceURI.isEmpty() ? nullptr : namespaceURI.impl() };
}

QualifiedName::QualifiedName(const AtomString& prefix, const AtomString& localName, const AtomString& namespaceURI)
    : m_impl(threadGlobalData().qualifiedNameCache().getOrCreate(makeComponents(prefix, localName, namespaceURI)))
{
}

QualifiedName::QualifiedName(const AtomString& prefix, const AtomString& localName, const AtomString& namespaceURI, Namespace nodeNamespace, ElementName elementName)
    : m_impl(threadGlobalData().qualifiedNameCache().getOrCreate(makeComponents(prefix, localName, namespaceURI), nodeNamespace, elementName))
{
}

QualifiedName::QualifiedNameImpl::~QualifiedNameImpl()
{
    threadGlobalData().qualifiedNameCache().remove(*this);
}

// Global init routines
LazyNeverDestroyed<const QualifiedName> anyName;

void QualifiedName::init()
{
    static bool initialized = false;
    if (initialized)
        return;

    anyName.construct(nullAtom(), starAtom(), starAtom(), Namespace::Unknown, ElementName::Unknown);
    initialized = true;
}

const QualifiedName& nullQName()
{
    static NeverDestroyed<QualifiedName> nullName(nullAtom(), nullAtom(), nullAtom(), Namespace::None, ElementName::Unknown);
    return nullName;
}

const AtomString& QualifiedName::localNameUpper() const
{
    if (!m_impl->m_localNameUpper)
        m_impl->m_localNameUpper = m_impl->m_localName.convertToASCIIUppercase();
    return m_impl->m_localNameUpper;
}

unsigned QualifiedName::QualifiedNameImpl::computeHash() const
{
    QualifiedNameComponents components = { m_prefix.impl(), m_localName.impl(), m_namespaceURI.impl() };
    return WTF::computeHash(components);
}

}
