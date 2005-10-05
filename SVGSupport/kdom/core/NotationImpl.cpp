/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 2000 Peter Kelly (pmk@post.com)

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
#include "kdom.h"
#include "NotationImpl.h"
#include "DocumentImpl.h"

using namespace KDOM;

NotationImpl::NotationImpl(DocumentPtr *doc, DOMStringImpl *name, DOMStringImpl *publicId, DOMStringImpl *systemId) : NodeBaseImpl(doc)
{
    m_name = name;
    if(m_name)
        m_name->ref();

    m_publicId = publicId;
    if(m_publicId)
        m_publicId->ref();

    m_systemId = systemId;
    if(m_systemId)
        m_systemId->ref();
}

NotationImpl::NotationImpl(DocumentPtr *doc, DOMStringImpl *publicId, DOMStringImpl *systemId) : NodeBaseImpl(doc)
{
    m_name = 0;

    m_publicId = publicId;
    if(m_publicId)
        m_publicId->ref();

    m_systemId = systemId;
    if(m_systemId)
        m_systemId->ref();
}

NotationImpl::~NotationImpl()
{
    if(m_name)
        m_name->deref();
    if(m_publicId)
        m_publicId->deref();
    if(m_systemId)
        m_systemId->deref();
}

DOMStringImpl *NotationImpl::nodeName() const
{
    return m_name;
}

unsigned short NotationImpl::nodeType() const
{
    return NOTATION_NODE;
}

DOMStringImpl *NotationImpl::textContent() const
{
    return 0;
}

DOMStringImpl *NotationImpl::publicId() const
{
    return m_publicId;
}

DOMStringImpl *NotationImpl::systemId() const
{
    return m_systemId;
}

NodeImpl *NotationImpl::cloneNode(bool, DocumentPtr *doc) const
{
    return new NotationImpl(doc, nodeName(), publicId(), systemId());
}

// vim:ts=4:noet
