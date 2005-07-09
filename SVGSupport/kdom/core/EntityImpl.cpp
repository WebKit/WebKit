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

#include "EntityImpl.h"
#include "DocumentImpl.h"
#include "DocumentTypeImpl.h"
#include "DOMImplementationImpl.h"
#include "DOMStringImpl.h"

using namespace KDOM;

EntityImpl::EntityImpl(DocumentImpl *doc, const DOMString &name) : NodeBaseImpl(doc)
{
	m_name = name.implementation();
	if(m_name)
		m_name->ref();

	m_publicId = 0;
	m_systemId = 0;
	m_notationName = 0;
	m_inputEncoding = 0;
	m_xmlEncoding = 0;
	m_xmlVersion = 0;
}

EntityImpl::EntityImpl(DocumentImpl *doc, const DOMString &publicId, const DOMString &systemId, const DOMString &notationName) : NodeBaseImpl(doc)
{
	m_name = 0;
	m_publicId = publicId.implementation();
	if(m_publicId)
		m_publicId->ref();

	m_systemId = systemId.implementation();
	if(m_systemId)
		m_systemId->ref();

	m_notationName = notationName.implementation();
	if(m_notationName)
		m_notationName->ref();

	m_inputEncoding = 0;
	m_xmlEncoding = 0;
	m_xmlVersion = 0;
}

EntityImpl::EntityImpl(DocumentImpl *doc, const DOMString &name, const DOMString &publicId, const DOMString &systemId, const DOMString &notationName) : NodeBaseImpl(doc)
{
	m_name = name.implementation();
	if(m_name)
		m_name->ref();

	m_publicId = publicId.implementation();
	if(m_publicId)
		m_publicId->ref();

	m_systemId = systemId.implementation();
	if(m_systemId)
		m_systemId->ref();

	m_notationName = notationName.implementation();
	if(m_notationName)
		m_notationName->ref();

	m_inputEncoding = 0;
	m_xmlEncoding = 0;
	m_xmlVersion = 0;
}

EntityImpl::~EntityImpl()
{
	if(m_name)
		m_name->deref();
	if(m_publicId)
		m_publicId->deref();
	if(m_systemId)
		m_systemId->deref();
	if(m_notationName)
		m_notationName->deref();
	if(m_inputEncoding)
		m_inputEncoding->deref();
	if(m_xmlEncoding)
		m_xmlEncoding->deref();
	if(m_xmlVersion)
		m_xmlVersion->deref();
}

DOMString EntityImpl::publicId() const
{
	return DOMString(m_publicId);
}

DOMString EntityImpl::systemId() const
{
	return DOMString(m_systemId);
}

DOMString EntityImpl::notationName() const
{
	return DOMString(m_notationName);
}

DOMString EntityImpl::inputEncoding() const
{
	return DOMString(m_inputEncoding);
}

DOMString EntityImpl::xmlEncoding() const
{
	return DOMString(m_xmlEncoding);
}

DOMString EntityImpl::xmlVersion() const
{
	return DOMString(m_xmlVersion);
}

DOMString EntityImpl::nodeName() const
{
	return DOMString(m_name);
}

unsigned short EntityImpl::nodeType() const
{
	return ENTITY_NODE;
}

NodeImpl *EntityImpl::cloneNode(bool deep, DocumentImpl *doc) const
{
	EntityImpl *p = new EntityImpl(doc, nodeName(), DOMString(m_publicId), DOMString(m_systemId), DOMString(m_notationName));

	if(deep)
	{
		bool oldMode = doc->parsing();
		doc->setParsing(true);
		cloneChildNodes(p, doc);
		doc->setParsing(oldMode);
	}

	return p;
}

bool EntityImpl::childTypeAllowed(unsigned short type) const
{
	switch(type)
	{
		case ELEMENT_NODE:
		case TEXT_NODE:
		case COMMENT_NODE:
		case PROCESSING_INSTRUCTION_NODE:
		case CDATA_SECTION_NODE:
		case ENTITY_REFERENCE_NODE:
			return true;
		default:
			return false;
	}
}

// vim:ts=4:noet
