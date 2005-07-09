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

#include "NotationImpl.h"
#include "DocumentImpl.h"

using namespace KDOM;

NotationImpl::NotationImpl(DocumentImpl *doc, const DOMString &name, const DOMString &publicId, const DOMString &systemId) : NodeBaseImpl(doc)
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
}

NotationImpl::NotationImpl(DocumentImpl *doc, const DOMString &publicId, const DOMString &systemId) : NodeBaseImpl(doc)
{
	m_name = 0;
	m_publicId = publicId.implementation();
	if(m_publicId)
	        m_publicId->ref();

	m_systemId = systemId.implementation();
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

DOMString NotationImpl::nodeName() const
{
	return DOMString(m_name);
}

unsigned short NotationImpl::nodeType() const
{
	return NOTATION_NODE;
}

DOMString NotationImpl::textContent() const
{
	return DOMString();
}

DOMString NotationImpl::publicId() const
{
	return DOMString(m_publicId);
}

DOMString NotationImpl::systemId() const
{
	return DOMString(m_systemId);
}

NodeImpl *NotationImpl::cloneNode(bool, DocumentImpl *doc) const
{
	return new NotationImpl(doc, DOMString(m_name), DOMString(m_publicId), DOMString(m_systemId));
}

// vim:ts=4:noet
