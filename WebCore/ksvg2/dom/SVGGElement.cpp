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

#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGGElement.h"
#include "SVGGElementImpl.h"

#include "SVGConstants.h"
#include "SVGGElement.lut.h"
using namespace KSVG;

/*
@begin SVGGElement::s_hashTable 3
 dummy    SVGGElementConstants::Dummy    DontDelete|ReadOnly
@end
*/

ValueImp *SVGGElement::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(KDOM::DOMException)
    return Undefined();
}

SVGGElement SVGGElement::null;

SVGGElement::SVGGElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
}

SVGGElement::SVGGElement(SVGGElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i)
{
}

SVGGElement::SVGGElement(const SVGGElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
    (*this) = other;
}

SVGGElement::SVGGElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
    (*this) = other;
}

SVGGElement::~SVGGElement()
{
}

SVGGElement &SVGGElement::operator=(const SVGGElement &other)
{
    SVGElement::operator=(other);
    SVGTests::operator=(other);
    SVGLangSpace::operator=(other);
    SVGExternalResourcesRequired::operator=(other);
    SVGStylable::operator=(other);
    SVGTransformable::operator=(other);
    return *this;
}

SVGGElement &SVGGElement::operator=(const KDOM::Node &other)
{
    SVGGElementImpl *ohandle = static_cast<SVGGElementImpl *>(other.handle());
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
            SVGTransformable::operator=(ohandle);
        }
    }

    return *this;
}

// vim:ts=4:noet
