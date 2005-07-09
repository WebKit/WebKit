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

#include <kdom/DOMString.h>
#include <kdom/ecma/Ecma.h>

#include "SVGExternalResourcesRequired.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"
#include "SVGAnimatedBoolean.h"

#include "SVGConstants.h"
#include "SVGExternalResourcesRequired.lut.h"
using namespace KSVG;

/*
@begin SVGExternalResourcesRequired::s_hashTable 3
 externalResourcesRequired	SVGExternalResourcesRequiredConstants::ExternalResourcesRequired	DontDelete|ReadOnly
@end
*/

Value SVGExternalResourcesRequired::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGExternalResourcesRequiredConstants::ExternalResourcesRequired:
			return KDOM::safe_cache<SVGAnimatedBoolean>(exec, externalResourcesRequired());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

SVGExternalResourcesRequired::SVGExternalResourcesRequired() : impl(0)
{
}

SVGExternalResourcesRequired::SVGExternalResourcesRequired(SVGExternalResourcesRequiredImpl *i) : impl(i)
{
}

SVGExternalResourcesRequired::SVGExternalResourcesRequired(const SVGExternalResourcesRequired &other) : impl(0)
{
	(*this) = other;
}

SVGExternalResourcesRequired::~SVGExternalResourcesRequired()
{
}

SVGExternalResourcesRequired &SVGExternalResourcesRequired::operator=(const SVGExternalResourcesRequired &other)
{
	if(impl != other.impl)
		impl = other.impl;

	return *this;
}

SVGExternalResourcesRequired &SVGExternalResourcesRequired::operator=(SVGExternalResourcesRequiredImpl *other)
{
	if(impl != other)
		impl = other;

	return *this;
}

SVGAnimatedBoolean SVGExternalResourcesRequired::externalResourcesRequired() const
{
	if(!impl)
		return SVGAnimatedBoolean::null;

	return SVGAnimatedBoolean(impl->externalResourcesRequired());
}

// vim:ts=4:noet
