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

#include "DOMLookup.h"
#include "Ecma.h"
#include "DOMConfiguration.h"
#include "DOMConfigurationImpl.h"
#include "DOMStringList.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"
#include "DOMUserData.h"

#include "DOMConstants.h"
#include "DOMConfiguration.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DOMConfiguration::s_hashTable 2
 parameterNames		DOMConfigurationConstants::ParameterNames	DontDelete|ReadOnly
@end
@begin DOMConfigurationProto::s_hashTable 5
 setParameter		DOMConfigurationConstants::SetParameter		DontDelete|Function 2
 getParameter		DOMConfigurationConstants::GetParameter		DontDelete|Function 1
 canSetParameter	DOMConfigurationConstants::CanSetParameter	DontDelete|Function 2
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("DOMConfiguration", DOMConfigurationProto, DOMConfigurationProtoFunc)

ValueImp *DOMConfiguration::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case DOMConfigurationConstants::ParameterNames:
			return safe_cache<DOMStringList>(exec, parameterNames());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

ValueImp *DOMConfigurationProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(DOMConfiguration)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case DOMConfigurationConstants::SetParameter:
		{
			DOMString name = toDOMString(exec, args[0]);
			bool value = args[1]->toBoolean(exec);
//			DOMUserData value = ecma_cast<DOMUserData>(exec, args[1], &toDOMUserData);
			obj.setParameter(name, value);
			return Undefined();
		}
		case DOMConfigurationConstants::GetParameter:
		{
			DOMString name = toDOMString(exec, args[0]);
			return safe_cache<DOMUserData>(exec, obj.getParameter(name));
		}
		case DOMConfigurationConstants::CanSetParameter:
		{
			DOMString name = toDOMString(exec, args[0]);
			DOMUserData value = ecma_cast<DOMUserData>(exec, args[1], &toDOMUserData);
			return KJS::Boolean(obj.canSetParameter(name, value));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

DOMConfiguration DOMConfiguration::null;

DOMConfiguration::DOMConfiguration() : d(0)
{
}

DOMConfiguration::DOMConfiguration(DOMConfigurationImpl *i) : d(i)
{
}

DOMConfiguration::DOMConfiguration(const DOMConfiguration &other) : d(0)
{
	(*this) = other;
}

DOMConfiguration::~DOMConfiguration()
{
}

DOMConfiguration &DOMConfiguration::operator=(const DOMConfiguration &other)
{
	d = other.d;
	return *this;
}

bool DOMConfiguration::operator==(const DOMConfiguration &other) const
{
	return d == other.d;
}

bool DOMConfiguration::operator!=(const DOMConfiguration &other) const
{
	return !operator==(other);
}

void DOMConfiguration::setParameter(const DOMString &name, DOMUserData value)
{
	kdDebug() << k_funcinfo << d << endl;
	if(d)
	{
		kdDebug() << "value.handle() : " << value.handle() << endl;
		d->setParameter(name.implementation(), value.handle());
	}
}

void DOMConfiguration::setParameter(const DOMString &name, bool value)
{
	kdDebug() << k_funcinfo << d << endl;
	if(d)
	{
		kdDebug() << "value : " << value << endl;
		d->setParameter(name.implementation(), value);
	}
}

DOMUserData DOMConfiguration::getParameter(const DOMString &name) const
{
	if(!d)
		return DOMUserData::null;

	return DOMUserData(d->getParameter(name.implementation()));
}

bool DOMConfiguration::canSetParameter(const DOMString &name, DOMUserData value) const
{
	kdDebug() << k_funcinfo << d << endl;
	if(!d)
		return false;

	return d->canSetParameter(name.implementation(), value.handle());
}

DOMStringList DOMConfiguration::parameterNames() const
{
	if(!d)
		return DOMStringList::null;

	return DOMStringList(d->parameterNames());
}

// vim:ts=4:noet
