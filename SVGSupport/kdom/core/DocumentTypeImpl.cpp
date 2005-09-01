/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Computer, Inc.

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

#include "kdom.h"
#include "DocumentImpl.h"
#include "LSSerializerImpl.h"
#include "NamedNodeMapImpl.h"
#include "DocumentTypeImpl.h"

using namespace KDOM;

DocumentTypeImpl::DocumentTypeImpl(DocumentPtr *doc, DOMStringImpl *qualifiedName, DOMStringImpl *publicId, DOMStringImpl *systemId) : NodeImpl(doc), m_entities(0), m_notations(0)
{
    m_qualifiedName = qualifiedName;
    if(m_qualifiedName)
        m_qualifiedName->ref();

    m_publicId = publicId;
    if(m_publicId)
        m_publicId->ref();

    m_systemId = systemId;
    if(m_systemId)
        m_systemId->ref();
}

DocumentTypeImpl::~DocumentTypeImpl()
{
    if(m_entities)
        m_entities->deref();
    if(m_notations)
        m_notations->deref();
    if(m_qualifiedName)
        m_qualifiedName->deref();
    if(m_publicId)
        m_publicId->deref();
    if(m_systemId)
        m_systemId->deref();
}

DOMStringImpl *DocumentTypeImpl::nodeName() const
{
    return m_qualifiedName;
}

unsigned short DocumentTypeImpl::nodeType() const
{
    return DOCUMENT_TYPE_NODE;
}

DOMStringImpl *DocumentTypeImpl::textContent() const
{
    return 0;
}

DOMStringImpl *DocumentTypeImpl::name() const
{
    return m_qualifiedName;
}

DOMStringImpl *DocumentTypeImpl::publicId() const
{
    return m_publicId;
}

DOMStringImpl *DocumentTypeImpl::systemId() const
{
    return m_systemId;
}

DOMStringImpl *DocumentTypeImpl::internalSubset() const
{
    QString str;
    QTextOStream subset(&str);
    LSSerializerImpl::PrintInternalSubset(subset, const_cast<DocumentTypeImpl *>(this));
    return new DOMStringImpl(str);
}

NodeImpl *DocumentTypeImpl::cloneNode(bool, DocumentPtr *other) const
{
    DocumentTypeImpl *p = new DocumentTypeImpl(const_cast<DocumentPtr *>(other), nodeName(), publicId(), systemId());

    p->entities()->clone(entities());
    p->notations()->clone(notations());

    return p;
}

NamedNodeMapImpl *DocumentTypeImpl::entities() const
{
    if(!m_entities)
    {
        m_entities = new RONamedNodeMapImpl(docPtr());
        m_entities->ref();
    }

    return m_entities;
}

NamedNodeMapImpl *DocumentTypeImpl::notations() const
{
    if(!m_notations)
    {
        m_notations = new RONamedNodeMapImpl(docPtr());
        m_notations->ref();
    }

    return m_notations;
}

void DocumentTypeImpl::setName(DOMStringImpl *n)
{
    KDOM_SAFE_SET(m_qualifiedName, n);
}

void DocumentTypeImpl::setPublicId(DOMStringImpl *publicId)
{
    KDOM_SAFE_SET(m_publicId, publicId);
}

void DocumentTypeImpl::setSystemId(DOMStringImpl *systemId)
{
    KDOM_SAFE_SET(m_systemId, systemId);
}

// vim:ts=4:noet
