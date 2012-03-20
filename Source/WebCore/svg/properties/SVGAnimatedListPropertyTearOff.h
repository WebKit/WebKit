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
#include "SVGStaticListPropertyTearOff.h"

namespace WebCore {

template<typename PropertyType>
class SVGPropertyTearOff;

template<typename PropertyType>
class SVGAnimatedListPropertyTearOff : public SVGAnimatedProperty {
public:
    typedef typename SVGPropertyTraits<PropertyType>::ListItemType ListItemType;
    typedef SVGPropertyTearOff<ListItemType> ListItemTearOff;
    typedef Vector<RefPtr<ListItemTearOff> > ListWrapperCache;
    typedef SVGListPropertyTearOff<PropertyType> ListPropertyTearOff;

    ListPropertyTearOff* baseVal()
    {
        if (!m_baseVal)
            m_baseVal = ListPropertyTearOff::create(this, BaseValRole, m_values, m_wrappers);
        return static_cast<ListPropertyTearOff*>(m_baseVal.get());
    }

    ListPropertyTearOff* animVal()
    {
        if (!m_animVal)
            m_animVal = ListPropertyTearOff::create(this, AnimValRole, m_values, m_wrappers);
        return static_cast<ListPropertyTearOff*>(m_animVal.get());
    }

    virtual bool isAnimatedListTearOff() const { return true; }

    int removeItemFromList(SVGProperty* property, bool shouldSynchronizeWrappers)
    {
        // This should ever be called for our baseVal, as animVal can't modify the list.
        typedef SVGPropertyTearOff<typename SVGPropertyTraits<PropertyType>::ListItemType> ListItemTearOff;
        return static_cast<ListPropertyTearOff*>(m_baseVal.get())->removeItemFromList(static_cast<ListItemTearOff*>(property), shouldSynchronizeWrappers);
    }

    void detachListWrappers(unsigned newListSize)
    {
        if (m_baseVal)
            baseVal()->detachListWrappers(newListSize);
    }

    PropertyType& currentAnimatedValue()
    {
        ASSERT(m_isAnimating);
        ASSERT(m_animVal);
        return animVal()->values();
    }

    const PropertyType& currentBaseValue() const
    {
        return m_values;
    }

    void animationStarted(PropertyType* newAnimVal)
    {
        ASSERT(!m_isAnimating);
        ASSERT(newAnimVal);
        ASSERT(m_values.size() == m_wrappers.size());
        ASSERT(m_animatedWrappers.isEmpty());

        // Switch to new passed in value type & new wrappers list. The new wrappers list must be created for the new value.
        if (!newAnimVal->isEmpty())
            m_animatedWrappers.fill(0, newAnimVal->size());

        animVal()->setValuesAndWrappers(newAnimVal, &m_animatedWrappers);
        ASSERT(animVal()->values().size() == animVal()->wrappers().size());
        ASSERT(animVal()->wrappers().size() == m_animatedWrappers.size());
        m_isAnimating = true;
    }

    void animationEnded()
    {
        ASSERT(m_isAnimating);
        ASSERT(m_animVal);
        ASSERT(m_values.size() == m_wrappers.size());
        ASSERT(animVal()->values().size() == animVal()->wrappers().size());
        ASSERT(animVal()->wrappers().size() == m_animatedWrappers.size());

        animVal()->setValuesAndWrappers(&m_values, &m_wrappers);
        ASSERT(animVal()->values().size() == animVal()->wrappers().size());
        ASSERT(animVal()->wrappers().size() == m_wrappers.size());

        m_animatedWrappers.clear();
        m_isAnimating = false;

        ASSERT(contextElement());
        contextElement()->svgAttributeChanged(attributeName());
    }

    void synchronizeWrappersIfNeeded()
    {
        // Eventually the wrapper list needs synchronization because any SVGAnimateLengthList::calculateAnimatedValue() call may
        // mutate the length of our values() list, and thus the wrapper() cache needs synchronization, to have the same size.
        // Also existing wrappers which point directly at elements in the existing SVGLengthList have to be detached (so a copy
        // of them is created, so existing animVal variables in JS are kept-alive). If we'd detach them later the underlying
        // SVGLengthList was already mutated, and our list item wrapper tear offs would point nowhere. Assertions would fire.
        animVal()->detachListWrappers(animVal()->values().size());

        ASSERT(animVal()->values().size() == animVal()->wrappers().size());
        ASSERT(animVal()->wrappers().size() == m_animatedWrappers.size());
    }

    void animValWillChange()
    {
        ASSERT(m_isAnimating);
        ASSERT(m_animVal);
        ASSERT(m_values.size() == m_wrappers.size());
        synchronizeWrappersIfNeeded();
    }

    void animValDidChange()
    {
        ASSERT(m_isAnimating);
        ASSERT(m_animVal);
        ASSERT(m_values.size() == m_wrappers.size());
        synchronizeWrappersIfNeeded();

        ASSERT(contextElement());
        contextElement()->svgAttributeChanged(attributeName());
    }

    static PassRefPtr<SVGAnimatedListPropertyTearOff<PropertyType> > create(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, PropertyType& values)
    {
        ASSERT(contextElement);
        return adoptRef(new SVGAnimatedListPropertyTearOff<PropertyType>(contextElement, attributeName, animatedPropertyType, values));
    }

protected:
    SVGAnimatedListPropertyTearOff(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, PropertyType& values)
        : SVGAnimatedProperty(contextElement, attributeName, animatedPropertyType)
        , m_values(values)
    {
        if (!values.isEmpty())
            m_wrappers.fill(0, values.size());
    }

    PropertyType& m_values;

    ListWrapperCache m_wrappers;
    ListWrapperCache m_animatedWrappers;

    RefPtr<SVGProperty> m_baseVal;
    RefPtr<SVGProperty> m_animVal;
};

}

#endif // ENABLE(SVG)
#endif // SVGAnimatedListPropertyTearOff_h
