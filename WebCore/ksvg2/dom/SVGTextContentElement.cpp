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

#include <kdom/kdom.h>
#include <kdom/Shared.h>
#include <kdom/DOMString.h>

#include <kdom/ecma/Ecma.h>

#include "SVGHelper.h"
#include "SVGDocument.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGTextContentElement.h"
#include "SVGTextContentElementImpl.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGRect.h"
#include "SVGPoint.h"
#include "SVGAnimatedEnumeration.h"

#include "SVGConstants.h"
#include "SVGTextContentElement.lut.h"
using namespace KSVG;

/*
@begin SVGTextContentElement::s_hashTable 3
 textLength		SVGTextContentElementConstants::TextLength			DontDelete|ReadOnly
 lengthAdjust	SVGTextContentElementConstants::LengthAdjust		DontDelete|ReadOnly
@end
@begin SVGTextContentElementProto::s_hashTable 11
 getNumberOfChars		SVGTextContentElementConstants::GetNumberOfChars		DontDelete|Function 0
 getComputedTextLength	SVGTextContentElementConstants::GetComputedTextLength	DontDelete|Function 0
 getSubStringLength		SVGTextContentElementConstants::GetSubStringLength		DontDelete|Function 2
 getStartPositionOfChar	SVGTextContentElementConstants::GetStartPositionOfChar	DontDelete|Function 1
 getEndPositionOfChar	SVGTextContentElementConstants::GetEndPositionOfChar	DontDelete|Function 1
 getExtentOfChar 		SVGTextContentElementConstants::GetExtentOfChar 		DontDelete|Function 1
 getRotationOfChar 		SVGTextContentElementConstants::GetRotationOfChar 		DontDelete|Function 1
 getCharNumAtPosition 	SVGTextContentElementConstants::GetCharNumAtPosition	DontDelete|Function 1
 selectSubString		SVGTextContentElementConstants::SelectSubString			DontDelete|Function 2
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGTextContentElement", SVGTextContentElementProto, SVGTextContentElementProtoFunc)

Value SVGTextContentElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGTextContentElementConstants::TextLength:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, textLength());
		case SVGTextContentElementConstants::LengthAdjust:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, lengthAdjust());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

Value SVGTextContentElementProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	SVGTextContentElement obj(cast(exec, static_cast<KJS::ObjectImp *>(thisObj.imp())));
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGTextContentElementConstants::GetNumberOfChars:
		{
			return Number(obj.getNumberOfChars());
		}
		case SVGTextContentElementConstants::GetComputedTextLength:
		{
			return Number(obj.getComputedTextLength());
		}
		case SVGTextContentElementConstants::GetSubStringLength:
		{
			return Number(obj.getSubStringLength(args[0].toUInt32(exec), args[1].toUInt32(exec)));
		}
		case SVGTextContentElementConstants::GetStartPositionOfChar:
			return KDOM::safe_cache<SVGPoint>(exec, obj.getStartPositionOfChar(args[0].toUInt32(exec)));
		case SVGTextContentElementConstants::GetEndPositionOfChar:
			return KDOM::safe_cache<SVGPoint>(exec, obj.getStartPositionOfChar(args[0].toUInt32(exec)));
		case SVGTextContentElementConstants::GetExtentOfChar:
			return KDOM::safe_cache<SVGRect>(exec, obj.getExtentOfChar(args[0].toUInt32(exec)));
		case SVGTextContentElementConstants::GetRotationOfChar:
		{
			return Number(obj.getRotationOfChar(args[0].toUInt32(exec)));
		}
		case SVGTextContentElementConstants::GetCharNumAtPosition:
		{
			SVGPoint point = KDOM::ecma_cast<SVGPoint>(exec, args[0], &toSVGPoint);
			return Number(obj.getCharNumAtPosition(point));
		}
		case SVGTextContentElementConstants::SelectSubString:
		{
			obj.selectSubString(args[0].toUInt32(exec), args[1].toUInt32(exec));
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGTextContentElementImpl *>(d))

SVGTextContentElement SVGTextContentElement::null;

SVGTextContentElement::SVGTextContentElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable()
{
}

SVGTextContentElement::SVGTextContentElement(SVGTextContentElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i)
{
}

SVGTextContentElement::SVGTextContentElement(const SVGTextContentElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable()
{
	(*this) = other;
}

SVGTextContentElement::SVGTextContentElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable()
{
	(*this) = other;
}

SVGTextContentElement::~SVGTextContentElement()
{
}

SVGTextContentElement &SVGTextContentElement::operator=(const SVGTextContentElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	return *this;
}

SVGTextContentElement &SVGTextContentElement::operator=(const KDOM::Node &other)
{
	SVGTextContentElementImpl *ohandle = static_cast<SVGTextContentElementImpl *>(other.handle());
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
			SVGLangSpace::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
			SVGStylable::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedLength SVGTextContentElement::textLength() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->textLength());
}

SVGAnimatedEnumeration SVGTextContentElement::lengthAdjust() const
{
	if(!d)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->lengthAdjust());
}

long SVGTextContentElement::getNumberOfChars() const
{
	if(!d)
		return 0;

	return impl->getNumberOfChars();
}

float SVGTextContentElement::getComputedTextLength() const
{
	if(!d)
		return 0.;

	return impl->getComputedTextLength();
}

float SVGTextContentElement::getSubStringLength(unsigned long charnum, unsigned long nchars) const
{
	if(!d)
		return 0.;

	return impl->getSubStringLength(charnum, nchars);
}

SVGPoint SVGTextContentElement::getStartPositionOfChar(unsigned long charnum) const
{
	if(!d)
		return SVGPoint::null;

	return SVGPoint(impl->getStartPositionOfChar(charnum));
}

SVGPoint SVGTextContentElement::getEndPositionOfChar(unsigned long charnum) const
{
	if(!d)
		return SVGPoint::null;

	return SVGPoint(impl->getEndPositionOfChar(charnum));
}

SVGRect SVGTextContentElement::getExtentOfChar(unsigned long charnum) const
{
	if(!d)
		return SVGRect::null;

	return SVGRect(impl->getExtentOfChar(charnum));
}

float SVGTextContentElement::getRotationOfChar(unsigned long charnum) const
{
	if(!d)
		return 0.;

	return impl->getRotationOfChar(charnum);
}

long SVGTextContentElement::getCharNumAtPosition(const SVGPoint &point) const
{
	if(!d)
		return 0;

	return impl->getCharNumAtPosition(point.handle());
}

void SVGTextContentElement::selectSubString(unsigned long charnum, unsigned long nchars) const
{
	if(d)
		return impl->selectSubString(charnum, nchars);
}

// vim:ts=4:noet
