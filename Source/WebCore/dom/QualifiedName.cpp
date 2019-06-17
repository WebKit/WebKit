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

#include "QualifiedNameCache.h"
#include "ThreadGlobalData.h"
#include <wtf/Assertions.h>

namespace WebCore {

QualifiedName::QualifiedName(const AtomString& p, const AtomString& l, const AtomString& n)
    : m_impl(threadGlobalData().qualifiedNameCache().getOrCreate(QualifiedNameComponents { p.impl(), l.impl(), n.isEmpty() ? nullptr : n.impl() }))
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

    ASSERT_WITH_MESSAGE(WTF::nullAtomData.isConstructed(), "AtomString::init should have been called");
    anyName.construct(nullAtom(), starAtom(), starAtom());
    initialized = true;
}

const QualifiedName& nullQName()
{
    static NeverDestroyed<QualifiedName> nullName(nullAtom(), nullAtom(), nullAtom());
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
    QualifiedNameComponents components = { m_prefix.impl(), m_localName.impl(), m_namespace.impl() };
    return hashComponents(components);
}

}
