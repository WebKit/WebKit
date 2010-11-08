/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGStaticPropertyTearOff_h
#define SVGStaticPropertyTearOff_h

#if ENABLE(SVG)
#include "SVGPropertyTearOff.h"

namespace WebCore {

template<typename ContextElement, typename PropertyType>
class SVGStaticPropertyTearOff : public SVGPropertyTearOff<PropertyType> {
public:
    typedef SVGStaticPropertyTearOff<ContextElement, PropertyType> Self;
    typedef void (ContextElement::*UpdateMethod)();

    // Used for non-animated POD types that are not associated with a SVGAnimatedProperty object, nor with a XML DOM attribute
    // (for example: SVGSVGElement::currentTranslate).
    static PassRefPtr<Self> create(ContextElement* contextElement, PropertyType& value, UpdateMethod update)
    {
        ASSERT(contextElement);
        return adoptRef(new Self(contextElement, value, update));
    }

    virtual void commitChange() { (m_contextElement.get()->*m_update)(); }

private:
    SVGStaticPropertyTearOff(ContextElement* contextElement, PropertyType& value, UpdateMethod update)
        : SVGPropertyTearOff<PropertyType>(0, value)
        , m_update(update)
        , m_contextElement(contextElement)
    {
    }

    UpdateMethod m_update;
    RefPtr<ContextElement> m_contextElement;
};

}

#endif // ENABLE(SVG)
#endif // SVGStaticPropertyTearOff_h
