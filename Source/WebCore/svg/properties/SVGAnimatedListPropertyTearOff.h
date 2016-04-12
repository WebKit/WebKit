/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
    typedef Vector<RefPtr<ListItemTearOff>> ListWrapperCache;
    typedef SVGListProperty<PropertyType> ListProperty;
    typedef SVGListPropertyTearOff<PropertyType> ListPropertyTearOff;
    typedef PropertyType ContentType;

    virtual RefPtr<ListProperty> baseVal()
    {
        if (m_baseVal)
            return m_baseVal;

        auto property = ListPropertyTearOff::create(this, BaseValRole, m_values, m_wrappers);
        m_baseVal = property.ptr();
        return WTFMove(property);
    }

    virtual RefPtr<ListProperty> animVal()
    {
        if (m_animVal)
            return m_animVal;

        auto property = ListPropertyTearOff::create(this, AnimValRole, m_values, m_wrappers);
        m_animVal = property.ptr();
        return WTFMove(property);
    }
    
    bool isAnimating() const override { return m_animatedProperty; }
    bool isAnimatedListTearOff() const override { return true; }
    void propertyWillBeDeleted(const SVGProperty& property) override
    {
        if (&property == m_baseVal)
            m_baseVal = nullptr;
        else if (&property == m_animVal)
            m_animVal = nullptr;
    }

    int findItem(SVGProperty* property)
    {
        // This should ever be called for our baseVal, as animVal can't modify the list.
        // It's safe to cast to ListPropertyTearOff here as all classes inheriting from us supply their own removeItemFromList() method.
        typedef SVGPropertyTearOff<typename SVGPropertyTraits<PropertyType>::ListItemType> ListItemTearOff;
        return static_pointer_cast<ListPropertyTearOff>(baseVal())->findItem(static_cast<ListItemTearOff*>(property));
    }

    void removeItemFromList(size_t itemIndex, bool shouldSynchronizeWrappers)
    {
        // This should ever be called for our baseVal, as animVal can't modify the list.
        // It's safe to cast to ListPropertyTearOff here as all classes inheriting from us supply their own removeItemFromList() method.
        static_pointer_cast<ListPropertyTearOff>(baseVal())->removeItemFromList(itemIndex, shouldSynchronizeWrappers);
    }

    void detachListWrappers(unsigned newListSize)
    {
        ListProperty::detachListWrappersAndResize(&m_wrappers, newListSize);
    }

    PropertyType& currentAnimatedValue()
    {
        ASSERT(isAnimating());
        return m_animatedProperty->values();
    }

    const PropertyType& currentBaseValue() const
    {
        return m_values;
    }

    void animationStarted(PropertyType* newAnimVal, bool shouldOwnValues = false)
    {
        ASSERT(!isAnimating());
        ASSERT(newAnimVal);
        ASSERT(m_values.size() == m_wrappers.size());
        ASSERT(m_animatedWrappers.isEmpty());

        // Switch to new passed in value type & new wrappers list. The new wrappers list must be created for the new value.
        if (!newAnimVal->isEmpty())
            m_animatedWrappers.fill(0, newAnimVal->size());

        m_animatedProperty = animVal();
        m_animatedProperty->setValuesAndWrappers(newAnimVal, &m_animatedWrappers, shouldOwnValues);
        ASSERT(m_animatedProperty->values().size() == m_animatedProperty->wrappers().size());
        ASSERT(m_animatedProperty->wrappers().size() == m_animatedWrappers.size());
    }

    void animationEnded()
    {
        ASSERT(isAnimating());
        ASSERT(m_values.size() == m_wrappers.size());

        ASSERT(m_animatedProperty->values().size() == m_animatedProperty->wrappers().size());
        ASSERT(m_animatedProperty->wrappers().size() == m_animatedWrappers.size());

        m_animatedProperty->setValuesAndWrappers(&m_values, &m_wrappers, false);
        ASSERT(m_animatedProperty->values().size() == m_animatedProperty->wrappers().size());
        ASSERT(m_animatedProperty->wrappers().size() == m_wrappers.size());

        m_animatedWrappers.clear();
        m_animatedProperty = nullptr;
    }

    void synchronizeWrappersIfNeeded()
    {
        ASSERT(isAnimating());

        // Eventually the wrapper list needs synchronization because any SVGAnimateLengthList::calculateAnimatedValue() call may
        // mutate the length of our values() list, and thus the wrapper() cache needs synchronization, to have the same size.
        // Also existing wrappers which point directly at elements in the existing SVGLengthList have to be detached (so a copy
        // of them is created, so existing animVal variables in JS are kept-alive). If we'd detach them later the underlying
        // SVGLengthList was already mutated, and our list item wrapper tear offs would point nowhere. Assertions would fire.
        m_animatedProperty->detachListWrappers(m_animatedProperty->values().size());

        ASSERT(m_animatedProperty->values().size() == m_animatedProperty->wrappers().size());
        ASSERT(m_animatedProperty->wrappers().size() == m_animatedWrappers.size());
    }

    void animValWillChange()
    {
        ASSERT(m_values.size() == m_wrappers.size());
        synchronizeWrappersIfNeeded();
    }

    void animValDidChange()
    {
        ASSERT(m_values.size() == m_wrappers.size());
        synchronizeWrappersIfNeeded();
    }

    static Ref<SVGAnimatedListPropertyTearOff<PropertyType>> create(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, PropertyType& values)
    {
        ASSERT(contextElement);
        return adoptRef(*new SVGAnimatedListPropertyTearOff<PropertyType>(contextElement, attributeName, animatedPropertyType, values));
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

    // Cache the raw pointer but return a RefPtr<>. This will break the cyclic reference
    // between SVGListPropertyTearOff and SVGAnimatedListPropertyTearOff once the property
    // pointer is not needed.
    ListProperty* m_baseVal { nullptr };
    ListProperty* m_animVal { nullptr };

    RefPtr<ListProperty> m_animatedProperty;
};

}

#endif // SVGAnimatedListPropertyTearOff_h
