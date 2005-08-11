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

#include "SVGTransformImpl.h"
#include "SVGTransform.h"
#include "SVGMatrix.h"
#include "SVGElementImpl.h"
#include "ksvg.h"

#include "SVGConstants.h"
#include "SVGTransform.lut.h"
using namespace KSVG;

/*
@begin SVGTransform::s_hashTable 5
 type			SVGTransformConstants::Type			DontDelete|ReadOnly
 matrix			SVGTransformConstants::Matrix		DontDelete|ReadOnly
 angle			SVGTransformConstants::Angle		DontDelete|ReadOnly
@end
@begin SVGTransformProto::s_hashTable 7
 setMatrix		SVGTransformConstants::SetMatrix	DontDelete|Function 1
 setTranslate	SVGTransformConstants::SetTranslate	DontDelete|Function 2
 setScale		SVGTransformConstants::SetScale		DontDelete|Function 2
 setRotate		SVGTransformConstants::SetRotate	DontDelete|Function 3
 setSkewX		SVGTransformConstants::SetSkewX		DontDelete|Function 1
 setSkewY		SVGTransformConstants::SetSkewY		DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGTransform", SVGTransformProto, SVGTransformProtoFunc)

ValueImp *SVGTransform::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGTransformConstants::Type:
			return Number(type());
		case SVGTransformConstants::Matrix:
			return KDOM::safe_cache<SVGMatrix>(exec, matrix());
		case SVGTransformConstants::Angle:
			return Number(angle());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

ValueImp *SVGTransformProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGTransform)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGTransformConstants::SetMatrix:
		{
			SVGMatrix matrix = KDOM::ecma_cast<SVGMatrix>(exec, args[0], &toSVGMatrix);
			obj.setMatrix(matrix);
			return Undefined();
		}
		case SVGTransformConstants::SetTranslate:
		{
			double x = args[0]->toNumber(exec);
			double y = args[1]->toNumber(exec);
			obj.setTranslate(x, y);
			return Undefined();
		}
		case SVGTransformConstants::SetScale:
		{
			double sx = args[0]->toNumber(exec);
			double sy = args[1]->toNumber(exec);
			obj.setScale(sx, sy);
			return Undefined();
		}
		case SVGTransformConstants::SetRotate:
		{
			double angle = args[0]->toNumber(exec);
			double cx = args[1]->toNumber(exec);
			double cy = args[2]->toNumber(exec);
			obj.setRotate(angle, cx, cy);
			return Undefined();
		}
		case SVGTransformConstants::SetSkewX:
		{
			double angle = args[0]->toNumber(exec);
			obj.setSkewX(angle);
			return Undefined();
		}
		case SVGTransformConstants::SetSkewY:
		{
			double angle = args[0]->toNumber(exec);
			obj.setSkewY(angle);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(KDOM::DOMException)
	return Undefined();
}

SVGTransform SVGTransform::null;

SVGTransform::SVGTransform() : impl(0)
{
}

SVGTransform::SVGTransform(SVGTransformImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGTransform::SVGTransform(const SVGTransform &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGTransform)

unsigned short SVGTransform::type() const
{
	if(!impl)
		return SVG_TRANSFORM_UNKNOWN;

	return impl->type();
}

SVGMatrix SVGTransform::matrix() const
{
	if(!impl)
		return SVGMatrix(0);

	return SVGMatrix(impl->matrix());
}

double SVGTransform::angle() const
{
	if(!impl)
		return -1;

	return impl->angle();
}

void SVGTransform::setMatrix(const SVGMatrix &matrix)
{
	if(impl)
		impl->setMatrix(matrix.handle());
}

void SVGTransform::setTranslate(double tx, double ty)
{
	if(impl)
		impl->setTranslate(tx, ty);
}

void SVGTransform::setScale(double sx, double sy)
{
	if(impl)
		impl->setScale(sx, sy);
}

void SVGTransform::setRotate(double angle, double cx, double cy)
{
	if(impl)
		impl->setRotate(angle, cx, cy);
}

void SVGTransform::setSkewX(double angle)
{
	if(impl)
		impl->setSkewX(angle);
}

void SVGTransform::setSkewY(double angle)
{
	if(impl)
		impl->setSkewY(angle);
}

// vim:ts=4:noet
