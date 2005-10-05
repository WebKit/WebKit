/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Peter Kelly (pmk@post.com)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2003 Apple Computer, Inc.

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include "DocumentImpl.h"
#include "XMLElementImpl.h"

using namespace KDOM;

XMLElementImpl::XMLElementImpl(DocumentPtr *doc, NodeImpl::Id id) : ElementImpl(doc), m_id(id)
{
}

XMLElementImpl::XMLElementImpl(DocumentPtr *doc, NodeImpl::Id id, DOMStringImpl *prefix, bool nullNSSpecified) : ElementImpl(doc, prefix, nullNSSpecified), m_id(id)
{
}

XMLElementImpl::~XMLElementImpl()
{
}

DOMStringImpl *XMLElementImpl::localName() const
{
    if(!m_dom2)
        return 0;

    return ownerDocument()->getName(NodeImpl::ElementId, id());
}

DOMStringImpl *XMLElementImpl::tagName() const
{
    DOMStringImpl *name = ownerDocument()->getName(NodeImpl::ElementId, id());
    if(!name)
        return 0;

    if(m_prefix && !m_prefix->isEmpty())
    {
        name->ref();
        DOMStringImpl *ret = new DOMStringImpl(m_prefix->string() + QString::fromLatin1(":") + name->string());

        name->deref();
        return ret;
    }

    return name;
}

// vim:ts=4:noet
