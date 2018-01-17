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

#pragma once

#include "SVGListProperty.h"

namespace WebCore {

template<typename PropertyType>
class SVGListPropertyTearOff : public SVGListProperty<PropertyType> {
public:
    using Base = SVGListProperty<PropertyType>;
    using Self = SVGListPropertyTearOff<PropertyType>;

    using ListItemType = typename SVGPropertyTraits<PropertyType>::ListItemType;
    using ListItemTearOff = typename SVGPropertyTraits<PropertyType>::ListItemTearOff;
    using PtrListItemTearOff = RefPtr<ListItemTearOff>;
    using AnimatedListPropertyTearOff = SVGAnimatedListPropertyTearOff<PropertyType>;
    using ListWrapperCache = typename SVGAnimatedListPropertyTearOff<PropertyType>::ListWrapperCache;

    using Base::m_role;
    using Base::m_values;
    using Base::m_wrappers;

    static Ref<Self> create(AnimatedListPropertyTearOff& animatedProperty, SVGPropertyRole role, PropertyType& values, ListWrapperCache& wrappers)
    {
        return adoptRef(*new Self(animatedProperty, role, values, wrappers));
    }

    int findItem(ListItemTearOff* item) const
    {
        ASSERT(m_values);
        ASSERT(m_wrappers);

        unsigned size = m_wrappers->size();
        ASSERT(size == m_values->size());
        for (size_t i = 0; i < size; ++i) {
            if (item == m_wrappers->at(i))
                return i;
        }

        return -1;
    }

    void removeItemFromList(size_t itemIndex, bool shouldSynchronizeWrappers)
    {
        ASSERT(m_values);
        ASSERT(m_wrappers);
        ASSERT(m_values->size() == m_wrappers->size());
        ASSERT_WITH_SECURITY_IMPLICATION(itemIndex < m_wrappers->size());

        auto item = m_wrappers->at(itemIndex);
        item->detachWrapper();
        m_wrappers->remove(itemIndex);
        m_values->remove(itemIndex);

        if (shouldSynchronizeWrappers)
            commitChange();
    }

    // SVGList API
    ExceptionOr<void> clear()
    {
        return Base::clearValuesAndWrappers();
    }

    ExceptionOr<Ref<ListItemTearOff>> initialize(ListItemTearOff& newItem)
    {
        return Base::initializeValuesAndWrappers(newItem);
    }

    ExceptionOr<Ref<ListItemTearOff>> getItem(unsigned index)
    {
        return Base::getItemValuesAndWrappers(m_animatedProperty.get(), index);
    }

    ExceptionOr<Ref<ListItemTearOff>> insertItemBefore(ListItemTearOff& newItem, unsigned index)
    {
        return Base::insertItemBeforeValuesAndWrappers(newItem, index);
    }

    ExceptionOr<Ref<ListItemTearOff>> replaceItem(ListItemTearOff& newItem, unsigned index)
    {
        return Base::replaceItemValuesAndWrappers(newItem, index);
    }

    ExceptionOr<Ref<ListItemTearOff>> removeItem(unsigned index)
    {
        return Base::removeItemValuesAndWrappers(m_animatedProperty.get(), index);
    }

    ExceptionOr<Ref<ListItemTearOff>> appendItem(ListItemTearOff& newItem)
    {
        return Base::appendItemValuesAndWrappers(newItem);
    }

protected:
    SVGListPropertyTearOff(AnimatedListPropertyTearOff& animatedProperty, SVGPropertyRole role, PropertyType& values, ListWrapperCache& wrappers)
        : SVGListProperty<PropertyType>(role, values, &wrappers)
        , m_animatedProperty(animatedProperty)
    {
    }

    bool isReadOnly() const override
    {
        if (m_role == AnimValRole)
            return true;
        if (m_animatedProperty->isReadOnly())
            return true;
        return false;
    }

    void commitChange() override
    {
        ASSERT(m_values);
        ASSERT(m_wrappers);

        // Update existing wrappers, as the index in the values list has changed.
        unsigned size = m_wrappers->size();
        ASSERT(size == m_values->size());
        for (unsigned i = 0; i < size; ++i) {
            auto item = m_wrappers->at(i);
            if (!item)
                continue;
            item->setAnimatedProperty(m_animatedProperty.ptr());
            item->setValue(m_values->at(i));
        }

        m_animatedProperty->commitChange();
    }

    bool processIncomingListItemValue(const ListItemType&, unsigned*) override
    {
        ASSERT_NOT_REACHED();
        return true;
    }

    bool processIncomingListItemWrapper(Ref<ListItemTearOff>& newItem, unsigned* indexToModify) override
    {
        auto animatedPropertyOfItem = makeRefPtr(newItem->animatedProperty());

        // newItem has been created manually, it doesn't belong to any SVGElement.
        // (for example: "textElement.x.baseVal.appendItem(svgsvgElement.createSVGLength())")
        if (!animatedPropertyOfItem)
            return true;

        // newItem belongs to a SVGElement, but its associated SVGAnimatedProperty is not an animated list tear off.
        // (for example: "textElement.x.baseVal.appendItem(rectElement.width.baseVal)")
        if (!animatedPropertyOfItem->isAnimatedListTearOff()) {
            // We have to copy the incoming newItem, as we're not allowed to insert this tear off as is into our wrapper cache.
            // Otherwhise we'll end up having two SVGAnimatedPropertys that operate on the same SVGPropertyTearOff. Consider the example above:
            // SVGRectElements SVGAnimatedLength 'width' property baseVal points to the same tear off object
            // that's inserted into SVGTextElements SVGAnimatedLengthList 'x'. textElement.x.baseVal.getItem(0).value += 150 would
            // mutate the rectElement width _and_ the textElement x list. That's obviously wrong, take care of that.
            newItem = ListItemTearOff::create(newItem->propertyReference());
            return true;
        }

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        // 'newItem' is already living in another list. If it's not our list, synchronize the other lists wrappers after the removal.
        bool livesInOtherList = animatedPropertyOfItem != m_animatedProperty.ptr();
        AnimatedListPropertyTearOff* propertyTearOff = static_cast<AnimatedListPropertyTearOff*>(animatedPropertyOfItem.get());
        int indexToRemove = propertyTearOff->findItem(newItem.ptr());
        ASSERT(indexToRemove != -1);

        // Do not remove newItem if already in this list at the target index.
        if (!livesInOtherList && indexToModify && static_cast<unsigned>(indexToRemove) == *indexToModify)
            return false;

        propertyTearOff->removeItemFromList(indexToRemove, true);

        if (!indexToModify)
            return true;

        // If the item lived in our list, adjust the insertion index.
        if (!livesInOtherList) {
            unsigned& index = *indexToModify;
            // Spec: If the item is already in this list, note that the index of the item to (replace|insert before) is before the removal of the item.
            if (static_cast<unsigned>(indexToRemove) < index)
                --index;
        }

        return true;
    }

    // Back pointer to the animated property that created us
    // For example (text.x.baseVal): m_animatedProperty points to the 'x' SVGAnimatedLengthList object
    Ref<AnimatedListPropertyTearOff> m_animatedProperty;
};

}
