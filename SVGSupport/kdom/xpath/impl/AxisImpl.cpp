/*
    Copyright(C) 2004 Richard Moore <rich@kde.org>
    Copyright(C) 2005 Frans Englich <frans.englich@telia.com>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or(at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <qptrlist.h>

#include "AxisImpl.h"
#include "ContextImpl.h"
#include "ValueImpl.h"
#include "NodeSetImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

AxisImpl::AxisImpl(unsigned short op)
    : ExprNodeImpl(), m_id(op)
{
}

AxisImpl::~AxisImpl()
{
}

ValueImpl *AxisImpl::evaluate(ContextImpl * /*context*/) const
{
	switch(m_id)
	{
	case AxisImpl::AxisAncestor:
	case AxisImpl::AxisAncestorOrSelf:
	case AxisImpl::AxisAttribute:
	case AxisImpl::AxisChild:
	case AxisImpl::AxisDescendant:
	case AxisImpl::AxisDescendantOrSelf:
	case AxisImpl::AxisFollowing:
	case AxisImpl::AxisFollowingSibling:
	case AxisImpl::AxisNamespace:
	case AxisImpl::AxisParent:
	case AxisImpl::AxisPreceding:
	case AxisImpl::AxisPrecedingSibling:
	case AxisImpl::AxisSelf:
		break;
	}

	return new NodeSetImpl();
}

QString AxisImpl::dump() const
{
	QString axis;

	switch(m_id)
	{
	case AxisImpl::AxisAncestor:
		axis = "ancestor";
		break;
	case AxisImpl::AxisAncestorOrSelf:
		axis = "ancestor-or-self";
		break;
	case AxisImpl::AxisAttribute:
		axis = "attribute";
		break;
	case AxisImpl::AxisChild:
		axis = "child";
		break;
	case AxisImpl::AxisDescendant:
		axis = "descendant";
		break;
	case AxisImpl::AxisDescendantOrSelf:
		axis = "descendant-or-self";
		break;
	case AxisImpl::AxisFollowing:
		axis = "following";
		break;
	case AxisImpl::AxisFollowingSibling:
		axis = "following-sibling";
		break;
	case AxisImpl::AxisNamespace:
		axis = "namespace";
		break;
	case AxisImpl::AxisParent:
		axis = "parent";
		break;
	case AxisImpl::AxisPreceding:
		axis = "preceding";
		break;
	case AxisImpl::AxisPrecedingSibling:
		axis = "preceding-sibling";
		break;
	case AxisImpl::AxisSelf:
		axis = "self";
		break;
	default:
		axis = "Unknown Axis";
		break;
	}

	return QString::fromLatin1("AxisImpl: Axis='%1', id=%2").arg(axis).arg(m_id);
}

// vim:ts=4:noet
