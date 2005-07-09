/*
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

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

#include "QNameImpl.h"

using namespace KDOM;

QNameImpl::QNameImpl(const DOMString &namespaceURI, const DOMString &localPart,
		  			 const DOMString &prefix) : Shared(true), m_namespace(namespaceURI),
									 			m_local(localPart), m_prefix(prefix)
{
}

QNameImpl::~QNameImpl()
{
}

bool QNameImpl::operator==(const QNameImpl &impl) const
{
	if(impl.localPart() == localPart() &&
	   impl.namespaceURI() == namespaceURI())
		return true;

	return false;
}

DOMString QNameImpl::prefix() const
{
	return m_prefix;
}

DOMString QNameImpl::namespaceURI() const
{
	return m_namespace;
}

DOMString QNameImpl::localPart() const
{
	return m_local;
}

// vim:ts=4:noet
