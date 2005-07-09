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

#include "BooleanImpl.h"
#include "NumberImpl.h"
#include "StringImpl.h"
#include "ValueImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

ValueImpl::ValueImpl(unsigned short type): Shared(true), m_type(type)
{
}

ValueImpl::~ValueImpl()
{
}

unsigned short ValueImpl::type() const
{
	return m_type;
}

StringImpl  *ValueImpl::toString()
{
	if(m_type == ValueImpl::ValueString)
		return static_cast<StringImpl *>(this);

	return 0;
}

BooleanImpl *ValueImpl::toBoolean()
{
	if(m_type == ValueImpl::ValueBoolean)
		return static_cast<BooleanImpl *>(this);

	return 0;
}

NumberImpl  *ValueImpl::toNumber()
{
	if(m_type == ValueImpl::ValueNumber)
		return static_cast<NumberImpl *>(this);

	return 0;
}


// vim:ts=4:noet
