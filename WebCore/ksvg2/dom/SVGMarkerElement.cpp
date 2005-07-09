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

#include "SVGAngle.h"
#include "SVGAnimatedAngle.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGMarkerElement.h"
#include "SVGMarkerElementImpl.h"

#include "SVGConstants.h"
#include "SVGMarkerElement.lut.h"
using namespace KSVG;

/*
@begin SVGMarkerElement::s_hashTable 9
 refX			SVGMarkerElementConstants::RefX			DontDelete|ReadOnly
 refY			SVGMarkerElementConstants::RefY			DontDelete|ReadOnly
 markerUnits	SVGMarkerElementConstants::MarkerUnits	DontDelete|ReadOnly
 markerWidth	SVGMarkerElementConstants::MarkerWidth	DontDelete|ReadOnly
 markerHeight	SVGMarkerElementConstants::MarkerHeight	DontDelete|ReadOnly
 orientType		SVGMarkerElementConstants::OrientType	DontDelete|ReadOnly
 orientAngle	SVGMarkerElementConstants::OrientAngle	DontDelete|ReadOnly
@end
@begin SVGMarkerElementProto::s_hashTable 3
 setOrientToAuto	SVGMarkerElementConstants::SetOrientToAuto		DontDelete|Function 0
 setOrientToAngle	SVGMarkerElementConstants::SetOrientToAngle		DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGMarkerElement", SVGMarkerElementProto, SVGMarkerElementProtoFunc)

Value SVGMarkerElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE
	
	switch(token)
	{
		case SVGMarkerElementConstants::RefX:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, refX());
		case SVGMarkerElementConstants::RefY:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, refY());
		case SVGMarkerElementConstants::MarkerWidth:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, markerWidth());
		case SVGMarkerElementConstants::MarkerHeight:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, markerHeight());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

Value SVGMarkerElementProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	SVGMarkerElement obj(cast(exec, static_cast<KJS::ObjectImp *>(thisObj.imp())));
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGMarkerElementConstants::SetOrientToAuto:
		{
			obj.setOrientToAuto();
			return Undefined();
		}
		case SVGMarkerElementConstants::SetOrientToAngle:
		{
			SVGAngle angle = KDOM::ecma_cast<SVGAngle>(exec, args[0], &toSVGAngle);
			obj.setOrientToAngle(angle);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGMarkerElementImpl *>(SVGElement::d))

SVGMarkerElement SVGMarkerElement::null;

SVGMarkerElement::SVGMarkerElement()
: SVGElement(), SVGLangSpace(), SVGExternalResourcesRequired(),
  SVGStylable(), SVGFitToViewBox()
{
}

SVGMarkerElement::SVGMarkerElement(SVGMarkerElementImpl *i)
: SVGElement(i), SVGLangSpace(i), SVGExternalResourcesRequired(i),
  SVGStylable(i), SVGFitToViewBox(i)
{
}

SVGMarkerElement::SVGMarkerElement(const SVGMarkerElement &other)
: SVGElement(), SVGLangSpace(), SVGExternalResourcesRequired(),
  SVGStylable(), SVGFitToViewBox()
{
	(*this) = other;
}

SVGMarkerElement::SVGMarkerElement(const KDOM::Node &other)
: SVGElement(), SVGLangSpace(), SVGExternalResourcesRequired(),
  SVGStylable(), SVGFitToViewBox()
{
	(*this) = other;
}

SVGMarkerElement::~SVGMarkerElement()
{
}

SVGMarkerElement &SVGMarkerElement::operator=(const SVGMarkerElement &other)
{
	SVGElement::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGFitToViewBox::operator=(other);
	return *this;
}

SVGMarkerElement &SVGMarkerElement::operator=(const KDOM::Node &other)
{
	SVGMarkerElementImpl *ohandle = static_cast<SVGMarkerElementImpl *>(other.handle());
	if(impl != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
		{
			if(impl)
				impl->deref();

			Node::d = 0;
		}
		else
		{
			SVGElement::operator=(other);
			SVGLangSpace::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
			SVGStylable::operator=(ohandle);
			SVGFitToViewBox::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedLength SVGMarkerElement::refX() const
{
	if(!impl)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->refX());
}

SVGAnimatedLength SVGMarkerElement::refY() const
{
	if(!impl)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->refY());
}

SVGAnimatedEnumeration SVGMarkerElement::markerUnits() const
{
	if(!impl)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->markerUnits());
}

SVGAnimatedLength SVGMarkerElement::markerWidth() const
{
	if(!impl)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->markerWidth());
}

SVGAnimatedLength SVGMarkerElement::markerHeight() const
{
	if(!impl)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->markerHeight());
}

SVGAnimatedEnumeration SVGMarkerElement::orientType() const
{
	if(!impl)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->orientType());
}

SVGAnimatedAngle SVGMarkerElement::orientAngle() const
{
	if(!impl)
		return SVGAnimatedAngle::null;

	return SVGAnimatedAngle(impl->orientAngle());
}

void SVGMarkerElement::setOrientToAuto()
{
	if(impl)
		impl->setOrientToAuto();
}

void SVGMarkerElement::setOrientToAngle(const SVGAngle &angle)
{
	if(impl)
		impl->setOrientToAngle(angle.handle());
}

// vim:ts=4:noet
