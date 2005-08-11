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

#include "NodeFilter.h"
#include "NodeFilterImpl.h"
#include "Node.h"
#include "NodeImpl.h"
#include "kdomtraversal.h"

using namespace KDOM;

NodeFilterCondition::NodeFilterCondition() : Shared(true)
{
}

short NodeFilterCondition::acceptNode(const Node &) const
{
	return FILTER_ACCEPT;
}

NodeFilter NodeFilter::null;

NodeFilter::NodeFilter() : d(0)
{
}

NodeFilter::NodeFilter(NodeFilterImpl *i) : d(i)
{
	if(d)
		d->ref();
}

NodeFilter::NodeFilter(const NodeFilter &other) : d(0)
{
	(*this) = other;
}

NodeFilter::~NodeFilter()
{
	if(d)
		d->deref();
}

NodeFilter &NodeFilter::operator=(const NodeFilter &other)
{
	if(d)
		d->deref();

	d = other.d;

	if(d)
		d->ref();

	return (*this);
}

bool NodeFilter::operator==(const NodeFilter &other) const
{
	return d == other.d;
}

bool NodeFilter::operator!=(const NodeFilter &other) const
{
	return !operator==(other);
}

short NodeFilter::acceptNode(const Node &n) const
{
	if(!d)
		return FILTER_ACCEPT;

	return d->acceptNode(static_cast<NodeImpl *>(n.handle()));
}

// vim:ts=4:noet
