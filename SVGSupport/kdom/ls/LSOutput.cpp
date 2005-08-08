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
#include "LSOutput.h"
#include "DOMString.h"
#include "LSException.h"
#include "LSOutputImpl.h"
#include "LSExceptionImpl.h"

#include "LSConstants.h"
#include "LSOutput.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin LSOutput::s_hashTable 9
 systemId			LSOutputConstants::SystemId				DontDelete
 encoding			LSOutputConstants::Encoding				DontDelete
@end
*/

ValueImp *LSOutput::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case LSOutputConstants::SystemId:
			return getDOMString(systemId());
		case LSOutputConstants::Encoding:
			return getDOMString(encoding());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(LSException)
	return Undefined();
}

void LSOutput::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case LSOutputConstants::SystemId:
			setSystemId(toDOMString(exec, value));
			break;
		case LSOutputConstants::Encoding:
			setEncoding(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(LSException)
}

// The qdom way...
#define impl (static_cast<LSOutputImpl *>(d))

LSOutput LSOutput::null;

LSOutput::LSOutput() : d(0)
{
}

LSOutput::LSOutput(LSOutputImpl *i) : d(i)
{
	if(d)
		d->ref();
}

LSOutput::LSOutput(const LSOutput &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(LSOutput)

DOMString LSOutput::systemId() const
{
	if(!d)
		return DOMString();

	return DOMString(d->systemId());
}

void LSOutput::setSystemId(const DOMString &systemId)
{
	if(d)
		d->setSystemId(systemId);
}

DOMString LSOutput::encoding() const
{
	if(!d)
		return DOMString();

	return DOMString(d->encoding());
}

void LSOutput::setEncoding(const DOMString &encoding)
{
	if(d)
		d->setEncoding(encoding);
}

// vim:ts=4:noet
