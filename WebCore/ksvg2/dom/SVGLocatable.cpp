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

#include <kdom/kdom.h>
#include <kdom/ecma/Ecma.h>
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGRect.h"
#include "SVGMatrix.h"
#include "SVGElement.h"
#include "SVGLocatable.h"
#include "SVGException.h"
#include "SVGElementImpl.h"
#include "SVGExceptionImpl.h"
#include "SVGLocatableImpl.h"

#include "SVGConstants.h"
#include "SVGLocatable.lut.h"
using namespace KSVG;

/*
@begin SVGLocatable::s_hashTable 3
 nearestViewportElement        SVGLocatableConstants::NearestViewportElement    DontDelete|ReadOnly
 farthestViewportElement    SVGLocatableConstants::FarthestViewportElement    DontDelete|ReadOnly
@end
@begin SVGLocatableProto::s_hashTable 5
 getBBox                    SVGLocatableConstants::GetBBox                    DontDelete|Function 0
 getCTM                        SVGLocatableConstants::GetCTM                    DontDelete|Function 0
 getScreenCTM                SVGLocatableConstants::GetScreenCTM                DontDelete|Function 0
 getTransformToElement        SVGLocatableConstants::GetTransformToElement    DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGLocatable", SVGLocatableProto, SVGLocatableProtoFunc)

ValueImp *SVGLocatable::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGLocatableConstants::NearestViewportElement:
            return KDOM::getDOMNode(exec, nearestViewportElement());
        case SVGLocatableConstants::FarthestViewportElement:
            return KDOM::getDOMNode(exec, farthestViewportElement());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(SVGException)
    return Undefined();
}

ValueImp *SVGLocatableProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
    SVGLocatable obj(cast(exec, thisObj));
    KDOM_ENTER_SAFE

    switch(id)
    {
        case SVGLocatableConstants::GetBBox:
            return KDOM::safe_cache<SVGRect>(exec, obj.getBBox());
        case SVGLocatableConstants::GetCTM:
            return KDOM::safe_cache<SVGMatrix>(exec, obj.getCTM());
        case SVGLocatableConstants::GetScreenCTM:
            return KDOM::safe_cache<SVGMatrix>(exec, obj.getScreenCTM());
        case SVGLocatableConstants::GetTransformToElement:
        {
            SVGElement element = KDOM::ecma_cast<SVGElement>(exec, args[0], &toSVGElement);
            return KDOM::safe_cache<SVGMatrix>(exec, obj.getTransformToElement(element));
        }
        default:
            kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
    }

    KDOM_LEAVE_CALL_SAFE(KDOM::DOMException)
    return Undefined();
}

SVGLocatable SVGLocatable::null;

SVGLocatable::SVGLocatable() : impl(0)
{
}

SVGLocatable::SVGLocatable(SVGLocatableImpl *i) : impl(i)
{
}

SVGLocatable::SVGLocatable(const SVGLocatable &other) : impl(other.impl)
{
}

SVGLocatable::~SVGLocatable()
{
}

SVGLocatable &SVGLocatable::operator=(const SVGLocatable &other)
{
    if(impl != other.impl)
        impl = other.impl;

    return *this;
}

SVGLocatable &SVGLocatable::operator=(SVGLocatableImpl *other)
{
    if(impl != other)
        impl = other;

    return *this;
}

SVGElement SVGLocatable::nearestViewportElement() const
{
    if(!impl)
        return SVGElement::null;

    return SVGElement(impl->nearestViewportElement());
}

SVGElement SVGLocatable::farthestViewportElement() const
{
    if(!impl)
        return SVGElement::null;

    return SVGElement(impl->farthestViewportElement());
}

SVGRect SVGLocatable::getBBox() const
{
    if(!impl)
        return SVGRect::null;

    return SVGRect(impl->getBBox());
}

SVGMatrix SVGLocatable::getCTM() const
{
    if(!impl)
        return SVGMatrix::null;

    return SVGMatrix(impl->getCTM());
}

SVGMatrix SVGLocatable::getScreenCTM() const
{
    if(!impl)
        return SVGMatrix::null;

    return SVGMatrix(impl->getScreenCTM());
}

SVGMatrix SVGLocatable::getTransformToElement(const SVGElement &element) const
{
    if(!impl)
        return SVGMatrix::null;

    return SVGMatrix(impl->getTransformToElement(static_cast<SVGElementImpl *>(element.handle())));
}

// vim:ts=4:noet
