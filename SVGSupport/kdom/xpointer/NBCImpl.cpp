/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich     <frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "Namespace.h"
#include "DOMStringImpl.h"

#include "NBCImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

NBCImpl::NBCImpl(NBCImpl *parent) : /* XPathNSResolverImpl(), */ Shared(), m_parent(parent)
{
    /* Rigg the "initial namespace binding context". We can't use addMapping,
     * because it's ment for mere mortals who aren't allowed to re-write the world. */
    m_mapping.insert(QString::fromLatin1("xml"), NS_XML.string());

    if(m_parent)
        m_parent->ref();
}

NBCImpl::~NBCImpl()
{
    if(m_parent)
        m_parent->deref();
}

bool NBCImpl::addMapping(DOMStringImpl *prefixImpl, DOMStringImpl *nsImpl)
{
    DOMString prefix(prefixImpl);
    DOMString ns(nsImpl);

    if(prefix == "xml" || prefix == "xmlns" || ns == NS_XML || ns == NS_XMLNS)
        return false; /* FOO! */

    /* The Framework says nothing about validness of prefix and ns, so do nothing.
     * However, the xmlns scheme specs validness, and those checks are in XMLNSScheme. */
    m_mapping.insert(prefix.string(), ns.string());

    return true;
}

NBCImpl *NBCImpl::parentNBC() const
{
    return m_parent;
}

DOMStringImpl *NBCImpl::lookupNamespaceURI(DOMStringImpl *prefixImpl) const
{
    DOMString prefix(prefixImpl);
    if(m_mapping.contains(prefix.string()))
        return new DOMStringImpl(m_mapping[prefix.string()]);
    else if(parentNBC())
        return parentNBC()->lookupNamespaceURI(prefixImpl);
    
    return 0;
}

// vim:ts=4:noet
