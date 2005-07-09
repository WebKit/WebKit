/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include "kdom.h"
#include "Ecma.h"
#include "LSException.h"
#include "LSExceptionImpl.h"
#include "LSSerializerFilter.h"
#include "LSSerializerFilterImpl.h"

#include "LSConstants.h"
#include "LSSerializerFilter.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin LSSerializerFilter::s_hashTable 3
 whatToShow	LSSerializerFilterConstants::WhatToShow	DontDelete|ReadOnly
@end
*/

Value LSSerializerFilter::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case LSSerializerFilterConstants::WhatToShow:
			return Number(whatToShow());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(LSException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<LSSerializerFilterImpl *>(d))

LSSerializerFilter LSSerializerFilter::null;

LSSerializerFilter::LSSerializerFilter() : NodeFilter()
{
}

LSSerializerFilter::~LSSerializerFilter()
{
}

LSSerializerFilter::LSSerializerFilter(LSSerializerFilterImpl *i) : NodeFilter(i)
{
}

LSSerializerFilter::LSSerializerFilter(const LSSerializerFilter &other) : NodeFilter()
{
	(*this) = other;
}

LSSerializerFilter &LSSerializerFilter::operator=(const LSSerializerFilter &other)
{
	NodeFilter::operator=(other);
	return *this;
}

unsigned long LSSerializerFilter::whatToShow() const
{
	if(!d)
		return 0;

	return impl->whatToShow();
}

// vim:ts=4:noet
