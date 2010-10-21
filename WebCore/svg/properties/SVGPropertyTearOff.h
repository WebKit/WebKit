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

#ifndef SVGPropertyTearOff_h
#define SVGPropertyTearOff_h

#if ENABLE(SVG)
#include "SVGElement.h"
#include "SVGProperty.h"

namespace WebCore {

class SVGAnimatedProperty;

template<typename PropertyType>
class SVGPropertyTearOff : public SVGProperty {
public:
    typedef SVGPropertyTearOff<PropertyType> Self;

    // Used for [SVGAnimatedProperty] types (for example: SVGAnimatedLength::baseVal()).
    // Also used for list tear offs (for example: text.x.baseVal.getItem(0)).
    static PassRefPtr<Self> create(SVGAnimatedProperty* animatedProperty, SVGPropertyRole, PropertyType& value)
    {
        ASSERT(animatedProperty);
        return adoptRef(new Self(animatedProperty, value));
    }

    // Used for [SVGLiveProperty] types (for example: SVGSVGElement::createSVGLength()).
    static PassRefPtr<Self> create(const PropertyType& initialValue)
    {
        return adoptRef(new Self(initialValue));
    }

    PropertyType& propertyReference() { return *m_value; }
    SVGAnimatedProperty* animatedProperty() const { return m_animatedProperty.get(); }

    virtual int removeItemFromList(SVGAnimatedProperty*) { return -1; }

    // Used only by the list tear offs!
    void setValue(PropertyType& value)
    {
        if (m_valueIsCopy)
            delete m_value;
        m_valueIsCopy = false;
        m_value = &value;
    }

    void setAnimatedProperty(SVGAnimatedProperty* animatedProperty) { m_animatedProperty = animatedProperty; }

    void commitChange()
    {
        if (!m_animatedProperty || m_valueIsCopy)
            return;
        SVGElement* contextElement = m_animatedProperty->contextElement();
        ASSERT(contextElement);
        contextElement->invalidateSVGAttributes();
        contextElement->svgAttributeChanged(m_animatedProperty->attributeName());
    }

    SVGElement* contextElement() const
    {
        if (!m_animatedProperty || m_valueIsCopy)
            return 0;
        return m_animatedProperty->contextElement();
    }

    void detachWrapper()
    {
        // Switch from a live value, to a non-live value.
        // For example: <text x="50"/>
        // var item = text.x.baseVal.getItem(0);
        // text.setAttribute("x", "100");
        // item.value still has to report '50' and it has to be possible to modify 'item'
        // w/o changing the "new item" (with x=100) in the text element.
        // Whenever the XML DOM modifies the "x" attribute, all existing wrappers are detached, using this function.
        ASSERT(!m_valueIsCopy);
        m_value = new PropertyType(*m_value);
        m_valueIsCopy = true;
    }

private:
    SVGPropertyTearOff(SVGAnimatedProperty* animatedProperty, PropertyType& value)
        : m_animatedProperty(animatedProperty)
        , m_value(&value)
        , m_valueIsCopy(false)
    {
        // Using operator & is completly fine, as SVGAnimatedProperty owns this reference,
        // and we're guaranteed to live as long as SVGAnimatedProperty does.
    }

    SVGPropertyTearOff(const PropertyType& initialValue)
        : m_animatedProperty(0)
        , m_value(new PropertyType(initialValue))
        , m_valueIsCopy(true)
    {
    }

    virtual ~SVGPropertyTearOff()
    {
        if (m_valueIsCopy)
            delete m_value;
    }

    RefPtr<SVGAnimatedProperty> m_animatedProperty;
    PropertyType* m_value;
    bool m_valueIsCopy : 1;
};

}

#endif // ENABLE(SVG)
#endif // SVGPropertyTearOff_h
