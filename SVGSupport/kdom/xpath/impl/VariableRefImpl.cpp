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

#include <qmap.h>

#include <kdebug.h>

#include "DOMString.h"

#include "ContextImpl.h"
#include "ScopeImpl.h"
#include "ValueImpl.h"
#include "VariableRefImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

VariableRefImpl::VariableRefImpl(const DOMString &name)
	: ExprNodeImpl(), m_id(name)
{
}

VariableRefImpl::~VariableRefImpl()
{
}

ValueImpl *VariableRefImpl::evaluate(ContextImpl *context) const
{
	ScopeImpl *s = context->scope();
	QString name = m_id.string();

	if(!s->hasVariable(name))
	{
		// TODO Exception should be thrown.
		kdDebug() <<  "No such variable " << name.latin1();
		return 0;
	}

	return s->variable(name);
}

QString VariableRefImpl::dump() const
{
	return QString::fromLatin1("VariableRefImpl: %1").arg(m_id.string());
}

// vim:ts=4:noet
