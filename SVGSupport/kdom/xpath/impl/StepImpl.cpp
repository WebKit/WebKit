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

#include <qstring.h>
#include <qptrlist.h>

#include "ContextImpl.h"
#include "StepImpl.h"
#include "NodeSetImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

StepImpl::StepImpl(unsigned short op) : ExprNodeImpl(),
					m_id(op), m_star(false)
{
}

StepImpl::~StepImpl()
{
}

ValueImpl *StepImpl::evaluate(ContextImpl * /*context*/) const
{
	return new NodeSetImpl();
}

QString StepImpl::dump() const
{
	QString s;

	switch(m_id)
	{
	case StepImpl::StepDot:
		s = ".";
		break;
	case StepImpl::StepDotDot:
		s = "..";
		break;
	case StepImpl::StepQName:
		s = "qname";
		break;
	case StepImpl::StepStar:
		s = "*";
		break;
	case StepImpl::StepNameTest:
		s = "nametest";
		break;
	case StepImpl::StepProcessingInstruction:
		s = "processinginstruction";
		break;
	case StepImpl::StepText:
		s = "text";
		break;
	case StepImpl::StepNode:
		s = "node";
		break;
	case StepImpl::StepComment:
		s = "comment";
		break;
	default:
		s = "Unknown Step";
		break;
	}

	return QString::fromLatin1("StepImpl: %4 name='%2', id='%1', isstar='%3'")
		.arg(m_id).arg(m_name).arg(m_star).arg(s);
}

void StepImpl::setName(const QString &test)
{
	m_name = test;
}

void StepImpl::setStar(const bool yes)
{
	m_star = yes;
}
