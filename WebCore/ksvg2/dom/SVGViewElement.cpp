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

#include "SVGStringList.h"
#include "SVGStringListImpl.h"
#include "SVGException.h"
#include "SVGViewElement.h"
#include "SVGExceptionImpl.h"
#include "SVGViewElementImpl.h"

#include "SVGConstants.h"
#include "SVGViewElement.lut.h"
using namespace KSVG;

/*
@begin SVGViewElement::s_hashTable 3
 viewTarget        SVGViewElementConstants::ViewTarget        DontDelete|ReadOnly
@end
*/

ValueImp *SVGViewElement::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE
    
    switch(token)
    {
        case SVGViewElementConstants::ViewTarget:
            return KDOM::safe_cache<SVGStringList>(exec, viewTarget());
    default:
        kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(SVGException)
    return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGViewElementImpl *>(SVGElement::d))

SVGViewElement SVGViewElement::null;

SVGViewElement::SVGViewElement()
: SVGElement(), SVGExternalResourcesRequired(), SVGFitToViewBox(), SVGZoomAndPan()
{
}

SVGViewElement::SVGViewElement(SVGViewElementImpl *i)
: SVGElement(i), SVGExternalResourcesRequired(i), SVGFitToViewBox(i), SVGZoomAndPan(i)
{
}

SVGViewElement::SVGViewElement(const SVGViewElement &other)
: SVGElement(), SVGExternalResourcesRequired(), SVGFitToViewBox(), SVGZoomAndPan()
{
    (*this) = other;
}

SVGViewElement::SVGViewElement(const KDOM::Node &other)
: SVGElement(), SVGExternalResourcesRequired(), SVGFitToViewBox(), SVGZoomAndPan()
{
    (*this) = other;
}

SVGViewElement::~SVGViewElement()
{
}

SVGViewElement &SVGViewElement::operator=(const SVGViewElement &other)
{
    SVGElement::operator=(other);
    SVGExternalResourcesRequired::operator=(other);
    SVGFitToViewBox::operator=(other);
    SVGZoomAndPan::operator=(other);
    return *this;
}

SVGViewElement &SVGViewElement::operator=(const KDOM::Node &other)
{
    SVGViewElementImpl *ohandle = static_cast<SVGViewElementImpl *>(other.handle());
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
            SVGExternalResourcesRequired::operator=(ohandle);
            SVGFitToViewBox::operator=(ohandle);
            SVGZoomAndPan::operator=(ohandle);
        }
    }

    return *this;
}

SVGStringList SVGViewElement::viewTarget() const
{
    if(!impl)
        return SVGStringList::null;

    return SVGStringList(impl->viewTarget());
}

// vim:ts=4:noet
