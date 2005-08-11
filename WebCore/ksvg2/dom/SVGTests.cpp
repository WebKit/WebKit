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

#include <kdom/DOMString.h>
#include <kdom/ecma/Ecma.h>

#include "SVGTests.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGTestsImpl.h"
#include "SVGStringList.h"

#include "SVGConstants.h"
#include "SVGTests.lut.h"
using namespace KSVG;

/*
@begin SVGTests::s_hashTable 5
 requiredFeatures	SVGTestsConstants::RequiredFeatures		DontDelete|ReadOnly
 requiredExtensions	SVGTestsConstants::RequiredExtensions	DontDelete|ReadOnly
 systemLanguage		SVGTestsConstants::SystemLanguage		DontDelete|ReadOnly
@end
@begin SVGTestsProto::s_hashTable 3
 hasExtension	SVGTestsConstants::HasExtension	DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGTests", SVGTestsProto, SVGTestsProtoFunc)

ValueImp *SVGTests::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGTestsConstants::RequiredFeatures:
			return KDOM::safe_cache<SVGStringList>(exec, requiredFeatures());
		case SVGTestsConstants::RequiredExtensions:
			return KDOM::safe_cache<SVGStringList>(exec, requiredExtensions());
		case SVGTestsConstants::SystemLanguage:
			return KDOM::safe_cache<SVGStringList>(exec, systemLanguage());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

ValueImp *SVGTestsProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	SVGTests obj(cast(exec, thisObj));
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGTestsConstants::HasExtension:
		{
			KDOM::DOMString extension = KDOM::toDOMString(exec, args[0]);
			return KJS::Boolean(obj.hasExtension(extension));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

SVGTests SVGTests::null;

SVGTests::SVGTests() : impl(0)
{
}

SVGTests::SVGTests(SVGTestsImpl *i) : impl(i)
{
}

SVGTests::SVGTests(const SVGTests &other) : impl(0)
{
	(*this) = other;
}

SVGTests::~SVGTests()
{
}

SVGTests &SVGTests::operator=(const SVGTests &other)
{
	if(impl != other.impl)
		impl = other.impl;

	return *this;
}

SVGTests &SVGTests::operator=(SVGTestsImpl *other)
{
	if(impl != other)
		impl = other;

	return *this;
}

SVGStringList SVGTests::requiredFeatures() const
{
	if(!impl)
		return SVGStringList::null;

	return SVGStringList(impl->requiredFeatures());
}

SVGStringList SVGTests::requiredExtensions() const
{
	if(!impl)
		return SVGStringList::null;

	return SVGStringList(impl->requiredExtensions());
}

SVGStringList SVGTests::systemLanguage() const
{
	if(!impl)
		return SVGStringList::null;

	return SVGStringList(impl->systemLanguage());
}

bool SVGTests::hasExtension(const KDOM::DOMString &extension) const
{
	if(!impl)
		return false;

	return impl->hasExtension(extension);
}

// vim:ts=4:noet
