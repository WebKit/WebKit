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

#include "ksvg.h"
#include "SVGPaint.h"
#include "SVGHelper.h"
#include "SVGPaintImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"

#include "SVGConstants.h"
#include "SVGPaint.lut.h"
using namespace KSVG;

/*
@begin SVGPaint::s_hashTable 3
 paintType	SVGPaintConstants::PaintType	DontDelete|ReadOnly
@end
@begin SVGPaintProto::s_hashTable 5
 setUri		SVGPaintConstants::SetUri		DontDelete|Function 1
 setPaint	SVGPaintConstants::SetPaint		DontDelete|Function 4
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGPaint", SVGPaintProto, SVGPaintProtoFunc)

Value SVGPaint::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGPaintConstants::PaintType:
			return Number(paintType());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

Value SVGPaintProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGPaint)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGPaintConstants::SetUri:
		{
			KDOM::DOMString uri = KDOM::toDOMString(exec, args[0]);
			obj.setUri(uri);
			return Undefined();
		}
		case SVGPaintConstants::SetPaint:
		{
			unsigned short paintType = args[0].toUInt16(exec);
			KDOM::DOMString uri = KDOM::toDOMString(exec, args[1]);
			KDOM::DOMString rgbPaint = KDOM::toDOMString(exec, args[2]);
			KDOM::DOMString iccPaint = KDOM::toDOMString(exec, args[3]);
			obj.setPaint(paintType, uri, rgbPaint, iccPaint);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGPaintImpl *>(SVGColor::d))

SVGPaint SVGPaint::null;

SVGPaint::SVGPaint() : SVGColor()
{
}

SVGPaint::SVGPaint(SVGPaintImpl *i) : SVGColor(i)
{
}

SVGPaint::SVGPaint(const SVGPaint &other) : SVGColor(other)
{
}

SVGPaint::SVGPaint(const KDOM::CSSValue &other) : SVGColor()
{
	(*this) = other;
}

SVGPaint::~SVGPaint()
{
}

SVGPaint &SVGPaint::operator=(const SVGPaint &other)
{
	SVGColor::operator=(other);
	return *this;
}

SVGPaint &SVGPaint::operator=(const KDOM::CSSValue &other)
{
	SVGColorImpl *ohandle = static_cast<SVGColorImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle)
		{
			if(d)
				d->deref();
			
			d = 0;
		}
		else
			SVGColor::operator=(other);
	}

	return *this;
}


unsigned short SVGPaint::paintType() const
{
	if(!d)
		return SVG_PAINTTYPE_UNKNOWN;

	return impl->paintType();
}

KDOM::DOMString SVGPaint::uri() const
{
	if(!d)
		return KDOM::DOMString();

	return KDOM::DOMString(impl->uri());
}

void SVGPaint::setUri(const KDOM::DOMString &uri)
{
	if(d)
		impl->setUri(uri.implementation());
}

void SVGPaint::setPaint(unsigned short paintType, const KDOM::DOMString &uri, const KDOM::DOMString &rgbPaint, const KDOM::DOMString &iccPaint)
{
	if(d)
		impl->setPaint(paintType, uri.implementation(), rgbPaint.implementation(), iccPaint.implementation());
}

// vim:ts=4:noet
