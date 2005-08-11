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

#include <kdom/ecma/Ecma.h>
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "ksvg.h"
#include "SVGPoint.h"
#include "SVGMatrix.h"
#include "SVGHelper.h"
#include "SVGPointImpl.h"
#include "SVGException.h"
#include "SVGElementImpl.h"
#include "SVGExceptionImpl.h"

#include "SVGConstants.h"
#include "SVGPoint.lut.h"
using namespace KSVG;

/*
@begin SVGPoint::s_hashTable 3
 x	SVGPointConstants::X	DontDelete
 y	SVGPointConstants::Y	DontDelete
@end
@begin SVGPointProto::s_hashTable 3
 matrixTransform	SVGPointConstants::MatrixTransform	DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGPoint", SVGPointProto, SVGPointProtoFunc)

ValueImp *SVGPoint::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGPointConstants::X:
			return Number(x());
		case SVGPointConstants::Y:
			return Number(y());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

void SVGPoint::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGPointConstants::X:
			setX(value->toNumber(exec));
			break;
		case SVGPointConstants::Y:
			setY(value->toNumber(exec));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

ValueImp *SVGPointProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGPoint)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGPointConstants::MatrixTransform:
		{
			SVGMatrix matrix = KDOM::ecma_cast<SVGMatrix>(exec, args[0], &toSVGMatrix);
			return KDOM::safe_cache<SVGPoint>(exec, obj.matrixTransform(matrix));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

SVGPoint SVGPoint::null;

SVGPoint::SVGPoint() : impl(0)
{
}

SVGPoint::SVGPoint(SVGPointImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGPoint::SVGPoint(const SVGPoint &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGPoint)

float SVGPoint::x() const
{
	if(!impl)
		return -1;

	return impl->x();
}

float SVGPoint::y() const
{
	if(!impl)
		return -1;

	return impl->y();
}

void SVGPoint::setX(float x)
{
	if(impl)
		impl->setX(x);
}

void SVGPoint::setY(float y)
{
	if(impl)
		impl->setY(y);
}

SVGPoint SVGPoint::matrixTransform(const SVGMatrix &matrix)
{
	if(!impl)
		return SVGPoint::null;

	return SVGPoint(impl->matrixTransform(matrix.handle()));
}

// vim:ts=4:noet
