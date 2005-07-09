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

#include "BooleanImpl.h"
#include "NumberImpl.h"
#include "OperatorImpl.h"
#include "ValueImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

OperatorImpl::OperatorImpl(const unsigned short op)
    : ExprNodeImpl(), m_id(op)
{
}

OperatorImpl::~OperatorImpl()
{
}

ValueImpl *OperatorImpl::evaluate(ContextImpl *context) const
{
	switch(m_id)
	{
	case OperatorImpl::OperatorOr:
	{
		bool left = argBoolean(context, 0)->value();
		bool right = argBoolean(context, 1)->value();
		return new BooleanImpl(left || right);
	}
	case OperatorImpl::OperatorAnd:
	{
		bool left = argBoolean(context, 0)->value();
		bool right = argBoolean(context, 1)->value();
		return new BooleanImpl(left && right);
	}
	case OperatorImpl::OperatorEqual:
	{
		return 0;
	}
	case OperatorImpl::OperatorNotEqual:
	{
		return 0;
	}
	case OperatorImpl::OperatorLessThan:
	{
		const double left = argNumber(context, 0)->value();
		const double right = argNumber(context, 1)->value();
		return new BooleanImpl(left < right);
	}
	case OperatorImpl::OperatorGreaterThan:
	{
		const double left = argNumber(context, 0)->value();
		const double right = argNumber(context, 1)->value();
		return new BooleanImpl(left > right);
	}
	case OperatorImpl::OperatorLessThanEquals:
	{
		const double left = argNumber(context, 0)->value();
		const double right = argNumber(context, 1)->value();
		return new BooleanImpl(left <= right);
	}
	case OperatorImpl::OperatorGreaterThanEquals:
	{
		const double left = argNumber(context, 0)->value();
		const double right = argNumber(context, 1)->value();
		return new BooleanImpl(left >= right);
	}
	case OperatorImpl::OperatorMinus:
	{
		const double left = argNumber(context, 0)->value();
		return new NumberImpl(-left);
	}
	default:
	{
		Q_ASSERT(0);
		return 0;
	}
	}

	Q_ASSERT(0);
	return 0;
}

QString OperatorImpl::dump() const
{
	return QString::fromLatin1("OperatorImpl: %1").arg(m_id);
}

// vim:ts=4:noet
