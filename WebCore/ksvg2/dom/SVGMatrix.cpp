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
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGMatrix.h"
#include "SVGMatrixImpl.h"
#include "SVGElementImpl.h"

#include "SVGConstants.h"
#include "SVGMatrix.lut.h"
using namespace KSVG;

/*
@begin SVGMatrix::s_hashTable 7
 a					SVGMatrixConstants::A					DontDelete
 b					SVGMatrixConstants::B					DontDelete
 c					SVGMatrixConstants::C					DontDelete
 d					SVGMatrixConstants::D					DontDelete
 e					SVGMatrixConstants::E					DontDelete
 f					SVGMatrixConstants::F					DontDelete
@end
@begin SVGMatrixProto::s_hashTable 13
 inverse			SVGMatrixConstants::Inverse				DontDelete|Function 0
 multiply			SVGMatrixConstants::Multiply			DontDelete|Function 1
 translate			SVGMatrixConstants::Translate			DontDelete|Function 2
 scale				SVGMatrixConstants::Scale				DontDelete|Function 1
 rotate				SVGMatrixConstants::Rotate				DontDelete|Function 1
 rotateFromVector	SVGMatrixConstants::RotateFromVector	DontDelete|Function 2
 scaleNonUniform	SVGMatrixConstants::ScaleNonUniform		DontDelete|Function 2
 flipX				SVGMatrixConstants::FlipX				DontDelete|Function 0
 flipY				SVGMatrixConstants::FlipY				DontDelete|Function 0
 skewX				SVGMatrixConstants::SkewX				DontDelete|Function 1
 skewY				SVGMatrixConstants::SkewY				DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGMatrix", SVGMatrixProto, SVGMatrixProtoFunc)

ValueImp *SVGMatrix::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGMatrixConstants::A:
			return Number(a());
		case SVGMatrixConstants::B:
			return Number(b());
		case SVGMatrixConstants::C:
			return Number(c());
		case SVGMatrixConstants::D:
			return Number(d());
		case SVGMatrixConstants::E:
			return Number(e());
		case SVGMatrixConstants::F:
			return Number(f());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

void SVGMatrix::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGMatrixConstants::A:
		{
			setA(value->toNumber(exec));
			break;
		}
		case SVGMatrixConstants::B:
		{
			setB(value->toNumber(exec));
			break;
		}
		case SVGMatrixConstants::C:
		{
			setC(value->toNumber(exec));
			break;
		}
		case SVGMatrixConstants::D:
		{
			setD(value->toNumber(exec));
			break;
		}
		case SVGMatrixConstants::E:
		{
			setE(value->toNumber(exec));
			break;
		}
		case SVGMatrixConstants::F:
		{
			setF(value->toNumber(exec));
			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

ValueImp *SVGMatrixProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGMatrix)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGMatrixConstants::Inverse:
			return KDOM::safe_cache<SVGMatrix>(exec, obj.inverse());
		case SVGMatrixConstants::Translate:
		{
			double x = args[0]->toNumber(exec);
			double y = args[1]->toNumber(exec);
			return KDOM::safe_cache<SVGMatrix>(exec, obj.translate(x, y));
		}
		case SVGMatrixConstants::Multiply:
		{
			SVGMatrix secondMatrix = KDOM::ecma_cast<SVGMatrix>(exec, args[0], &toSVGMatrix);
			return KDOM::safe_cache<SVGMatrix>(exec, obj.multiply(secondMatrix));
		}
		case SVGMatrixConstants::Scale:
		{
			double scaleFactor = args[0]->toNumber(exec);
			return KDOM::safe_cache<SVGMatrix>(exec, obj.scale(scaleFactor));
		}
		case SVGMatrixConstants::Rotate:
		{
			double angle = args[0]->toNumber(exec);
			return KDOM::safe_cache<SVGMatrix>(exec, obj.rotate(angle));
		}
		case SVGMatrixConstants::RotateFromVector:
		{
			double x = args[0]->toNumber(exec);
			double y = args[1]->toNumber(exec);
			return KDOM::safe_cache<SVGMatrix>(exec, obj.rotateFromVector(x, y));
		}
		case SVGMatrixConstants::ScaleNonUniform:
		{
			double scaleFactorX = args[0]->toNumber(exec);
			double scaleFactorY = args[1]->toNumber(exec);
			return KDOM::safe_cache<SVGMatrix>(exec, obj.scaleNonUniform(scaleFactorX, scaleFactorY));
		}
		case SVGMatrixConstants::FlipX:
			return KDOM::safe_cache<SVGMatrix>(exec, obj.flipX());
		case SVGMatrixConstants::FlipY:
			return KDOM::safe_cache<SVGMatrix>(exec, obj.flipY());
		case SVGMatrixConstants::SkewX:
		{
			double angle = args[0]->toNumber(exec);
			return KDOM::safe_cache<SVGMatrix>(exec, obj.skewX(angle));
		}
		case SVGMatrixConstants::SkewY:
		{
			double angle = args[0]->toNumber(exec);
			return KDOM::safe_cache<SVGMatrix>(exec, obj.skewY(angle));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(KDOM::DOMException)
	return Undefined();
}

SVGMatrix SVGMatrix::null;

SVGMatrix::SVGMatrix() : impl(0)
{
}

SVGMatrix::SVGMatrix(SVGMatrixImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGMatrix::SVGMatrix(const SVGMatrix &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGMatrix)

void SVGMatrix::setA(double a)
{
	if(impl)
		impl->setA(a);
}

double SVGMatrix::a() const
{
	if(!impl)
		return -1;

	return impl->a();
}

void SVGMatrix::setB(double b)
{
	if(impl)
		impl->setB(b);
}

double SVGMatrix::b() const
{
	if(!impl)
		return -1;

	return impl->b();
}

void SVGMatrix::setC(double c)
{
	if(impl)
		impl->setC(c);
}

double SVGMatrix::c() const
{
	if(!impl)
		return -1;

	return impl->c();
}

void SVGMatrix::setD(double d)
{
	if(impl)
		impl->setD(d);
}

double SVGMatrix::d() const
{
	if(!impl)
		return -1;

	return impl->d();
}

void SVGMatrix::setE(double e)
{
	if(impl)
		impl->setE(e);
}

double SVGMatrix::e() const
{
	if(!impl)
		return -1;

	return impl->e();
}

void SVGMatrix::setF(double f)
{
	if(impl)
		impl->setF(f);
}

double SVGMatrix::f() const
{
	if(!impl)
		return -1;

	return impl->f();
}

SVGMatrix SVGMatrix::multiply(const SVGMatrix &secondMatrix)
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->multiply(secondMatrix.impl));
}

SVGMatrix SVGMatrix::inverse()
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->inverse());
}

SVGMatrix SVGMatrix::translate(double x, double y)
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->translate(x, y));
}

SVGMatrix SVGMatrix::scale(double scaleFactor)
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->scale(scaleFactor));
}

SVGMatrix SVGMatrix::scaleNonUniform(double scaleFactorX, double scaleFactorY)
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->scaleNonUniform(scaleFactorX, scaleFactorY));
}

SVGMatrix SVGMatrix::rotate(double angle)
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->rotate(angle));
}

SVGMatrix SVGMatrix::rotateFromVector(double x, double y)
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->rotateFromVector(x, y));
}

SVGMatrix SVGMatrix::flipX()
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->flipX());
}

SVGMatrix SVGMatrix::flipY()
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->flipY());
}

SVGMatrix SVGMatrix::skewX(double angle)
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->skewX(angle));
}

SVGMatrix SVGMatrix::skewY(double angle)
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->skewY(angle));
}

// vim:ts=4:noet
