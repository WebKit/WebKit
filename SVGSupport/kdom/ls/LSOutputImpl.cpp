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

#include "DOMString.h"
#include "LSOutputImpl.h"
#include "DOMStringImpl.h"

using namespace KDOM;

LSOutputImpl::LSOutputImpl() : Shared(true), m_systemId(0), m_encoding(0)
{
}

LSOutputImpl::~LSOutputImpl()
{
	if(m_systemId)
		m_systemId->deref();
	if(m_encoding)
		m_encoding->deref();
}

DOMString LSOutputImpl::systemId() const
{
	return DOMString(m_systemId);
}

void LSOutputImpl::setSystemId(const DOMString &systemId)
{
	if(m_systemId)
		m_systemId->deref();

	m_systemId = systemId.implementation();

	if(m_systemId)
		m_systemId->ref();
}

DOMString LSOutputImpl::encoding() const
{
	return DOMString(m_encoding);
}

void LSOutputImpl::setEncoding(const DOMString &encoding)
{
	if(m_encoding)
		m_encoding->deref();

	m_encoding = encoding.implementation();

	if(m_encoding)
		m_encoding->ref();
}

// vim:ts=4:noet
