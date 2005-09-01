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
#include <kdom/Shared.h>
#include <kdom/DOMString.h>

#include <kdom/ecma/Ecma.h>

#include "SVGHelper.h"
#include "SVGDocument.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGUseElement.h"
#include "SVGUseElementImpl.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGUseElement.lut.h"
using namespace KSVG;

/*
@begin SVGUseElement::s_hashTable 7
 x                    SVGUseElementConstants::X                DontDelete|ReadOnly
 y                    SVGUseElementConstants::Y                DontDelete|ReadOnly
 width                SVGUseElementConstants::Width            DontDelete|ReadOnly
 height                SVGUseElementConstants::Height            DontDelete|ReadOnly
@end
*/

// TODO: instanceRoot        SVGUseElementConstants::InstanceRoot    DontDelete|ReadOnly

ValueImp *SVGUseElement::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGUseElementConstants::X:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, x());
        case SVGUseElementConstants::Y:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, y());
        case SVGUseElementConstants::Width:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, width());
        case SVGUseElementConstants::Height:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, height());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(SVGException)
    return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGUseElementImpl *>(d))

SVGUseElement SVGUseElement::null;

SVGUseElement::SVGUseElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGURIReference()
{
}

SVGUseElement::SVGUseElement(SVGUseElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i), SVGURIReference(i)
{
}

SVGUseElement::SVGUseElement(const SVGUseElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGURIReference()
{
    (*this) = other;
}

SVGUseElement::SVGUseElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGURIReference()
{
    (*this) = other;
}

SVGUseElement::~SVGUseElement()
{
}

SVGUseElement &SVGUseElement::operator=(const SVGUseElement &other)
{
    SVGElement::operator=(other);
    SVGTests::operator=(other);
    SVGLangSpace::operator=(other);
    SVGExternalResourcesRequired::operator=(other);
    SVGStylable::operator=(other);
    SVGTransformable::operator=(other);
    SVGURIReference::operator=(other);
    return *this;
}

SVGUseElement &SVGUseElement::operator=(const KDOM::Node &other)
{
    SVGUseElementImpl *ohandle = static_cast<SVGUseElementImpl *>(other.handle());
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
            SVGURIReference::operator=(ohandle);
        }
    }

    return *this;
}

SVGAnimatedLength SVGUseElement::x() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->x());
}

SVGAnimatedLength SVGUseElement::y() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->y());
}

SVGAnimatedLength SVGUseElement::width() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->width());
}

SVGAnimatedLength SVGUseElement::height() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->height());
}
/*
SVGElementInstance SVGUseElement::instanceRoot() const
{
    if(!d)
        return SVGElementInstance::null;

    return SVGElementInstance(impl->instanceRoot());
}

SVGElementInstance SVGUseElement::animatedInstanceRoot() const
{
    if(!d)
        return SVGElementInstance::null;

    return SVGElementInstance(impl->animatedInstanceRoot());
}*/

// vim:ts=4:noet
