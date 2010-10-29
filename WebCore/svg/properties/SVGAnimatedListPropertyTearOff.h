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

#ifndef SVGAnimatedListPropertyTearOff_h
#define SVGAnimatedListPropertyTearOff_h

#if ENABLE(SVG)
#include "SVGAnimatedProperty.h"
#include "SVGListPropertyTearOff.h"

namespace WebCore {

template<typename PropertyType>
class SVGPropertyTearOff;

template<typename PropertyType>
class SVGAnimatedListPropertyTearOff : public SVGAnimatedProperty {
public:
    SVGProperty* baseVal()
    {
        if (!m_baseVal)
            m_baseVal = SVGListPropertyTearOff<PropertyType>::create(this, BaseValRole, m_property);
        return m_baseVal.get();
    }

    SVGProperty* animVal()
    {
        if (!m_animVal)
            m_animVal = SVGListPropertyTearOff<PropertyType>::create(this, AnimValRole, m_property);
        return m_animVal.get();
    }

    virtual bool isAnimatedListTearOff() const { return true; }

    int removeItemFromList(SVGProperty* property, bool shouldSynchronizeWrappers)
    {
        // FIXME: No animVal support.
        if (!m_baseVal)
            return -1;

        typedef SVGPropertyTearOff<typename SVGPropertyTraits<PropertyType>::ListItemType> ListItemTearOff;
        return static_pointer_cast<SVGListPropertyTearOff<PropertyType> >(m_baseVal)->removeItemFromList(static_cast<ListItemTearOff*>(property), shouldSynchronizeWrappers);
    }

    void detachListWrappers(unsigned newListSize)
    {
        if (m_baseVal)
            static_pointer_cast<SVGListPropertyTearOff<PropertyType> >(m_baseVal)->detachListWrappers(newListSize);
        if (m_animVal)
            static_pointer_cast<SVGListPropertyTearOff<PropertyType> >(m_animVal)->detachListWrappers(newListSize);
    }

private:
    friend class SVGAnimatedProperty;

    static PassRefPtr<SVGAnimatedListPropertyTearOff<PropertyType> > create(SVGElement* contextElement, const QualifiedName& attributeName, PropertyType& property)
    {
        ASSERT(contextElement);
        return adoptRef(new SVGAnimatedListPropertyTearOff<PropertyType>(contextElement, attributeName, property));
    }

    SVGAnimatedListPropertyTearOff(SVGElement* contextElement, const QualifiedName& attributeName, PropertyType& property)
        : SVGAnimatedProperty(contextElement, attributeName)
        , m_property(property)
    {
    }

private:
    PropertyType& m_property;

    RefPtr<SVGProperty> m_baseVal;
    RefPtr<SVGProperty> m_animVal;
};

}

#endif // ENABLE(SVG)
#endif // SVGAnimatedListPropertyTearOff_h
