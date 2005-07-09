/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich 	<frans.englich@telia.com>
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

#include "DOMString.h"
#include "Namespace.h"

#include "NodeImpl.h"

#include "XPathNSResolverImpl.h"

using namespace KDOM;

XPathNSResolverImpl::XPathNSResolverImpl(): Shared(true)
{
}

XPathNSResolverImpl::XPathNSResolverImpl(NodeImpl *nodeResolver):
										 Shared(true), m_node(nodeResolver)
{
}

DOMString XPathNSResolverImpl::lookupNamespaceURI(const DOMString& prefix) const
{
	DOMString result(m_node->lookupNamespaceURI(prefix));

	if(result.isNull() && prefix == "xml")
		return NS_XML;

	return result;
}

// vim:ts=4:noet
