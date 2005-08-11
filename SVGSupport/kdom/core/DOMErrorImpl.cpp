/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#include "DOMErrorImpl.h"
#include "DOMLocatorImpl.h"
#include "DOMString.h"
#include "DOMStringImpl.h"
#include "NodeImpl.h"

using namespace KDOM;

DOMErrorImpl::DOMErrorImpl() : Shared(true)
{
	m_severity = 0;
	m_location = 0;
	m_message = 0;
	m_type = 0;
	m_relatedData = 0;
}

DOMErrorImpl::~DOMErrorImpl()
{
	if(m_message)
		m_message->deref();
	if(m_type)
		m_type->deref();
	if(m_relatedData)
		m_relatedData->deref();
}

unsigned short DOMErrorImpl::severity() const
{
	return m_severity;
}

void DOMErrorImpl::setSeverity(unsigned short severity)
{
	m_severity = severity;
}

DOMStringImpl *DOMErrorImpl::message() const
{
	return m_message;
}

void DOMErrorImpl::setMessage(const DOMString &mess)
{
	if(m_message)
		m_message->deref();

	m_message = mess.implementation();

	if(m_message)
		m_message->ref();
}

DOMStringImpl *DOMErrorImpl::type() const
{
	return m_type;
}

void DOMErrorImpl::setType(const DOMString &type)
{
	if(m_type)
		m_type->deref();

	m_type = type.implementation();

	if(m_type)
		m_type->ref();
}

//DOMObject relatedException
NodeImpl *DOMErrorImpl::relatedData() const
{
	return m_relatedData;
}

void DOMErrorImpl::setRelatedData(NodeImpl *relatedData)
{
	if(m_relatedData)
		m_relatedData->deref();

	m_relatedData = relatedData;

	if(m_relatedData)
		m_relatedData->ref();
}

DOMLocatorImpl *DOMErrorImpl::location() const
{
	if(!m_location)
		m_location = new DOMLocatorImpl();

	return m_location;
}

// vim:ts=4:noet
