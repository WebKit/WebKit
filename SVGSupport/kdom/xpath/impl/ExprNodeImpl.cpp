/*
   	Copyright(C) 2004 Zack Rusin <zack@kde.org>
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

#include <qstring.h>

#include "DOMString.h"

#include "ExprNodeImpl.h"
#include "StringImpl.h"
#include "ValueImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

ExprNodeImpl::ExprNodeImpl() : Shared(true)
{
}

ExprNodeImpl::~ExprNodeImpl()
{
}

int ExprNodeImpl::argCount() const
{
	return m_args.count();
}

ValueImpl *ExprNodeImpl::evaluate(ContextImpl *) const
{
	return new StringImpl("Expression");
}

StringImpl  *ExprNodeImpl::argString(ContextImpl *context, uint idx) const
{
	if(idx >= m_args.count())
	{
		// TODO Throw missing argument
	}

	return m_args[idx]->evaluate(context)->toString();
}

NumberImpl  *ExprNodeImpl::argNumber(ContextImpl *context, uint idx) const
{
	if(idx >= m_args.count())
	{
		// TODO Throw missing argument
	}

	return m_args[idx]->evaluate(context)->toNumber();
}

BooleanImpl *ExprNodeImpl::argBoolean(ContextImpl *context, uint idx) const
{
	if(idx >= m_args.count())
	{
		// TODO Throw missing argument
	}

	return m_args[idx]->evaluate(context)->toBoolean();
}

void ExprNodeImpl::addArg(ExprNodeImpl* expr)
{
	if(!expr)
		qWarning("Attempt to add null argument to ExprNodeImpl");

	m_args.append(expr);
}

ExprNodeImpl *ExprNodeImpl::arg(uint idx) const
{
	if(idx < m_args.count())
		return m_args[idx];
	else
		return 0;
}

void ExprNodeImpl::refArgs()
{
	const_iterator it;
	const_iterator end(m_args.constEnd());

	for(it = m_args.constBegin(); it != end; ++it)
		(*it)->ref();
}

void ExprNodeImpl::derefArgs()
{
	const_iterator it;
	const_iterator end(m_args.constEnd());

	for(it = m_args.constBegin(); it != end; ++it)
		(*it)->deref();
}

QString ExprNodeImpl::dump() const
{
	return QString::fromLatin1("ExprNodeImpl");
}

// vim:ts=4:noet
