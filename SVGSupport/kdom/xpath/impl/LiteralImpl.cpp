/*
    Copyright(C) 2004 Richard Moore <rich@kde.org>
    Copyright(C) 2004 Zack Rusin <zack@kde.org>
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

#include "DOMString.h"

#include "LiteralImpl.h"
#include "NumberImpl.h"
#include "StringImpl.h"
#include "ValueImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

LiteralImpl::LiteralImpl(ValueImpl *v) : m_val(v)
{
	if(m_val)
		m_val->ref();
}

LiteralImpl::~LiteralImpl()
{
	if(m_val)
		m_val->deref();
}

void LiteralImpl::refLiteral()
{
    m_val->ref();
}

void LiteralImpl::derefLiteral()
{
    m_val->deref();
}

ValueImpl *LiteralImpl::evaluate(ContextImpl * /*context*/) const
{
    return m_val;
}

QString LiteralImpl::dump() const
{
	switch(m_val->type())
	{
	case ValueImpl::ValueNumber:
		return QString::fromLatin1("LiteralImpl: type=Number value='%1'")
			.arg(m_val->toNumber()->value());
	case ValueImpl::ValueString:
		return QString::fromLatin1("LiteralImpl: type=String value='%1'")
			.arg(m_val->toString()->value().string());
	default:
		return QString::fromLatin1("LiteralImpl: type=%1 value='%2'")
			.arg(m_val->type()).arg(m_val->toString()->value().string());
	}
}

// vim:ts=4:noet
