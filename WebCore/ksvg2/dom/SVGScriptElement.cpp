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

#include <kdom/DOMString.h>
#include <kdom/ecma/Ecma.h>

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGScriptElement.h"
#include "SVGScriptElementImpl.h"
#include "SVGDocumentImpl.h"

#include "SVGConstants.h"
#include "SVGScriptElement.lut.h"
using namespace KSVG;

/*
@begin SVGScriptElement::s_hashTable 3
 type    SVGScriptElementConstants::Type    DontDelete
@end
*/

ValueImp *SVGScriptElement::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE
    
    switch(token)
    {
        case SVGScriptElementConstants::Type:
            return KDOM::getDOMString(type());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(SVGException)
    return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGScriptElementImpl *>(EventTarget::d))

void SVGScriptElement::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGScriptElementConstants::Type:
        {
            // No need to call setType(.), as it just calls setAttribute('type', 'value'),
            // which has already been done by KDOMDocumentBuilder::startAttribute.
            // In every other case an ecma script wants to change the type, which is legal.
            if(d && impl->ownerDocument()->parsing())
                setType(KDOM::toDOMString(exec, value));

            break;
        }
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(SVGException)
}

SVGScriptElement SVGScriptElement::null;

SVGScriptElement::SVGScriptElement() : SVGElement(), SVGURIReference(), SVGExternalResourcesRequired()
{
}

SVGScriptElement::SVGScriptElement(SVGScriptElementImpl *i) : SVGElement(i), SVGURIReference(i), SVGExternalResourcesRequired(i)
{
}

SVGScriptElement::SVGScriptElement(const SVGScriptElement &other) : SVGElement(), SVGURIReference(), SVGExternalResourcesRequired()
{
    (*this) = other;
}

SVGScriptElement::SVGScriptElement(const KDOM::Node &other) : SVGElement(), SVGURIReference(), SVGExternalResourcesRequired()
{
    (*this) = other;
}

SVGScriptElement::~SVGScriptElement()
{
}

SVGScriptElement &SVGScriptElement::operator=(const SVGScriptElement &other)
{
    SVGElement::operator=(other);
    SVGURIReference::operator=(other);
    SVGExternalResourcesRequired::operator=(other);
    return *this;
}

SVGScriptElement &SVGScriptElement::operator=(const KDOM::Node &other)
{
    SVGScriptElementImpl *ohandle = static_cast<SVGScriptElementImpl *>(other.handle());
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
            SVGURIReference::operator=(ohandle);
            SVGExternalResourcesRequired::operator=(ohandle);
        }
    }

    return *this;
}

KDOM::DOMString SVGScriptElement::type() const
{
    if(!impl)
        return KDOM::DOMString();

    return KDOM::DOMString(impl->type());
}

void SVGScriptElement::setType(const KDOM::DOMString &type)
{
    if(impl)
        impl->setType(type.implementation());
}

// vim:ts=4:noet
