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

#include <kdom/ecma/Ecma.h>

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimationElement.h"
#include "SVGAnimationElementImpl.h"

#include "SVGConstants.h"
#include "SVGAnimationElement.lut.h"
using namespace KSVG;

/*
@begin SVGAnimationElement::s_hashTable 2
 targetElement		SVGAnimationElementConstants::TargetElement		DontDelete|ReadOnly
@end
@begin SVGAnimationElementProto::s_hashTable 5
 getStartTime		SVGAnimationElementConstants::GetStartTime		DontDelete|Function 0
 getCurrentTime		SVGAnimationElementConstants::GetCurrentTime	DontDelete|Function 0
 getSimpleDuration	SVGAnimationElementConstants::GetSimpleDuration	DontDelete|Function 0
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGAnimationElement", SVGAnimationElementProto, SVGAnimationElementProtoFunc)

Value SVGAnimationElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimationElementConstants::TargetElement:
			return KDOM::getDOMNode(exec, targetElement());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}


Value SVGAnimationElementProtoFunc::call(ExecState *exec, Object &thisObj, const List &)
{
	SVGAnimationElement obj(cast(exec, static_cast<KJS::ObjectImp *>(thisObj.imp())));
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGAnimationElementConstants::GetStartTime:
			return Number(obj.getStartTime());
		case SVGAnimationElementConstants::GetCurrentTime:
			return Number(obj.getCurrentTime());
		case SVGAnimationElementConstants::GetSimpleDuration:
			return Number(obj.getSimpleDuration());
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGAnimationElementImpl *>(d))

SVGAnimationElement SVGAnimationElement::null;

SVGAnimationElement::SVGAnimationElement() : SVGElement(), SVGTests(), SVGExternalResourcesRequired()
{
}

SVGAnimationElement::SVGAnimationElement(SVGAnimationElementImpl *i) : SVGElement(i), SVGTests(i), SVGExternalResourcesRequired(i)
{
}

SVGAnimationElement::SVGAnimationElement(const SVGAnimationElement &other) : SVGElement(), SVGTests(), SVGExternalResourcesRequired()
{
	(*this) = other;
}

SVGAnimationElement::SVGAnimationElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGExternalResourcesRequired()
{
	(*this) = other;
}

SVGAnimationElement::~SVGAnimationElement()
{
}

SVGAnimationElement &SVGAnimationElement::operator=(const SVGAnimationElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	return *this;
}

SVGAnimationElement &SVGAnimationElement::operator=(const KDOM::Node &other)
{
	SVGAnimationElementImpl *ohandle = static_cast<SVGAnimationElementImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
		{
			if(d)
				d->deref();
	
			d = 0;
		}
		else
		{
			SVGElement::operator=(other);
			SVGTests::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
		}
	}

	return *this;
}

SVGElement SVGAnimationElement::targetElement() const
{
	if(!d)
		return SVGElement::null;

	return SVGElement(const_cast<SVGElementImpl *>(impl->targetElement()));
}

float SVGAnimationElement::getStartTime() const
{
	if(!d)
		return 0.0;

	return float(impl->startTime() / 1000.0);
}

float SVGAnimationElement::getCurrentTime() const
{
	if(!d)
		return 0.0;

	return float(impl->currentTime() / 1000.0);
}

float SVGAnimationElement::getSimpleDuration() const
{
	if(!d)
		return 0.0;

	return float(impl->simpleDuration() / 1000.0);
}

// vim:ts=4:noet
