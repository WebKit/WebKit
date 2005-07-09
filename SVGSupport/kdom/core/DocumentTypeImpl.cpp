/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include "NamedNodeMapImpl.h"
#include "DocumentTypeImpl.h"
#include "DocumentImpl.h"

using namespace KDOM;

DocumentTypeImpl::DocumentTypeImpl(DocumentImpl *doc, const DOMString &qualifiedName, const DOMString &publicId, const DOMString &systemId) : NodeImpl(doc), m_entities(0), m_notations(0)
{
	m_qualifiedName = qualifiedName;
	m_publicId = publicId;
	m_systemId = systemId;
}

DocumentTypeImpl::~DocumentTypeImpl()
{
	if(m_entities)
		m_entities->deref();
	if(m_notations)
		m_notations->deref();
}

DOMString DocumentTypeImpl::nodeName() const
{
	return m_qualifiedName;
}

unsigned short DocumentTypeImpl::nodeType() const
{
	return DOCUMENT_TYPE_NODE;
}

DOMString DocumentTypeImpl::textContent() const
{
	return DOMString();
}

DOMString DocumentTypeImpl::publicId() const
{
	return m_publicId;
}

DOMString DocumentTypeImpl::systemId() const
{
	return m_systemId;
}

NodeImpl *DocumentTypeImpl::cloneNode(bool, DocumentImpl *other) const
{
	DocumentTypeImpl *p = new DocumentTypeImpl(const_cast<DocumentImpl *>(other), nodeName(), publicId(), systemId());

	p->entities()->clone(entities());
	p->notations()->clone(notations());

	return p;
}

NamedNodeMapImpl *DocumentTypeImpl::entities() const
{
	if(!m_entities)
	{
		m_entities = new RONamedNodeMapImpl(document);
		m_entities->ref();
	}

	return m_entities;
}

NamedNodeMapImpl *DocumentTypeImpl::notations() const
{
	if(!m_notations)
	{
		m_notations = new RONamedNodeMapImpl(document);
		m_notations->ref();
	}

	return m_notations;
}

// vim:ts=4:noet
