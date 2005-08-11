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

#include "DOMLookup.h"
#include "Ecma.h"
#include "DOMUserData.h"
#include "DOMUserDataImpl.h"
#include "DOMStringList.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"

#include "DOMConstants.h"
#include "DOMUserData.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DOMUserData::s_hashTable 2
 dummy	DOMUserDataConstants::Dummy	DontDelete|ReadOnly
@end
*/

ValueImp *DOMUserData::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

DOMUserData DOMUserData::null;

DOMUserData::DOMUserData() : d(0)
{
}

DOMUserData::DOMUserData(DOMUserDataImpl *i) : d(i)
{
}

DOMUserData::DOMUserData(const DOMUserData &other) : d(0)
{
	(*this) = other;
}

DOMUserData::~DOMUserData()
{
}

DOMUserData &DOMUserData::operator=(const DOMUserData &other)
{
	d = other.d;
	return *this;
}

bool DOMUserData::operator==(const DOMUserData &other) const
{
	return d == other.d;
}

bool DOMUserData::operator!=(const DOMUserData &other) const
{
	return !operator==(other);
}

// vim:ts=4:noet
