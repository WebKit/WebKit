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

#ifndef SVGListPropertyTearOff_h
#define SVGListPropertyTearOff_h

#if ENABLE(SVG)
#include "ExceptionCode.h"
#include "SVGAnimatedProperty.h"
#include "SVGPropertyTearOff.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

template<typename PropertyType>
class SVGAnimatedListPropertyTearOff;

template<typename PropertyType>
class SVGListPropertyTearOff : public SVGProperty {
public:
    typedef SVGListPropertyTearOff<PropertyType> Self;

    typedef typename SVGPropertyTraits<PropertyType>::ListItemType ListItemType;
    typedef SVGPropertyTearOff<ListItemType> ListItemTearOff;
    typedef PassRefPtr<ListItemTearOff> PassListItemTearOff;
    typedef SVGAnimatedListPropertyTearOff<PropertyType> AnimatedListPropertyTearOff;
    typedef typename SVGAnimatedListPropertyTearOff<PropertyType>::ListWrapperCache ListWrapperCache;

    static PassRefPtr<Self> create(AnimatedListPropertyTearOff* animatedProperty, SVGPropertyRole role)
    {
        ASSERT(animatedProperty);
        return adoptRef(new Self(animatedProperty, role));
    }

    int removeItemFromList(ListItemTearOff* removeItem, bool shouldSynchronizeWrappers)
    {
        PropertyType& values = m_animatedProperty->values();
        ListWrapperCache& wrappers = m_animatedProperty->wrappers();

        // Lookup item in cache and remove its corresponding wrapper.
        unsigned size = wrappers.size();
        ASSERT(size == values.size());
        for (unsigned i = 0; i < size; ++i) {
            RefPtr<ListItemTearOff>& item = wrappers.at(i);
            if (item != removeItem)
                continue;

            item->detachWrapper();
            wrappers.remove(i);
            values.remove(i);

            if (shouldSynchronizeWrappers)
                commitChange();

            return i;
        }

        return -1;
    }

    // SVGList API
    void clear(ExceptionCode& ec)
    {
        if (m_role == AnimValRole) {
            ec = NO_MODIFICATION_ALLOWED_ERR;
            return;
        }

        m_animatedProperty->detachListWrappers(0);
        m_animatedProperty->values().clear();
    }

    unsigned numberOfItems() const
    {
        return m_animatedProperty->values().size();
    }

    PassListItemTearOff initialize(PassListItemTearOff passNewItem, ExceptionCode& ec)
    {
        if (m_role == AnimValRole) {
            ec = NO_MODIFICATION_ALLOWED_ERR;
            return 0;
        }

        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            ec = TYPE_MISMATCH_ERR;
            return 0;
        }

        PropertyType& values = m_animatedProperty->values();
        ListWrapperCache& wrappers = m_animatedProperty->wrappers();

        RefPtr<ListItemTearOff> newItem = passNewItem;
        ASSERT(values.size() == wrappers.size());

        // Spec: If the inserted item is already in a list, it is removed from its previous list before it is inserted into this list.
        removeItemFromListIfNeeded(newItem.get(), 0);

        // Spec: Clears all existing current items from the list and re-initializes the list to hold the single item specified by the parameter.
        m_animatedProperty->detachListWrappers(0);
        values.clear();

        values.append(newItem->propertyReference());
        wrappers.append(newItem);

        commitChange();
        return newItem.release();
    }

    PassListItemTearOff getItem(unsigned index, ExceptionCode& ec)
    {
        PropertyType& values = m_animatedProperty->values();
        if (index >= values.size()) {
            ec = INDEX_SIZE_ERR;
            return 0;
        }

        ListWrapperCache& wrappers = m_animatedProperty->wrappers();

        // Spec: Returns the specified item from the list. The returned item is the item itself and not a copy.
        // Any changes made to the item are immediately reflected in the list.
        ASSERT(values.size() == wrappers.size());
        RefPtr<ListItemTearOff> wrapper = wrappers.at(index);
        if (!wrapper) {
            // Create new wrapper, which is allowed to directly modify the item in the list, w/o copying and cache the wrapper in our map.
            // It is also associated with our animated property, so it can notify the SVG Element which holds the SVGAnimated*List
            // that it has been modified (and thus can call svgAttributeChanged(associatedAttributeName)).
            wrapper = ListItemTearOff::create(m_animatedProperty.get(), UndefinedRole, values.at(index));
            wrappers.at(index) = wrapper;
        }

        return wrapper.release();
    }

    PassListItemTearOff insertItemBefore(PassListItemTearOff passNewItem, unsigned index, ExceptionCode& ec)
    {
        if (m_role == AnimValRole) {
            ec = NO_MODIFICATION_ALLOWED_ERR;
            return 0;
        }

        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            ec = TYPE_MISMATCH_ERR;
            return 0;
        }

        PropertyType& values = m_animatedProperty->values();
        ListWrapperCache& wrappers = m_animatedProperty->wrappers();

        // Spec: If the index is greater than or equal to numberOfItems, then the new item is appended to the end of the list.
        if (index > values.size())
             index = values.size();

        RefPtr<ListItemTearOff> newItem = passNewItem;
        ASSERT(values.size() == wrappers.size());

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        removeItemFromListIfNeeded(newItem.get(), &index);

        // Spec: Inserts a new item into the list at the specified position. The index of the item before which the new item is to be
        // inserted. The first item is number 0. If the index is equal to 0, then the new item is inserted at the front of the list.
        values.insert(index, newItem->propertyReference());

        // Store new wrapper at position 'index', change its underlying value, so mutations of newItem, directly affect the item in the list.
        wrappers.insert(index, newItem);

        commitChange();
        return newItem.release();
    }

    PassListItemTearOff replaceItem(PassListItemTearOff passNewItem, unsigned index, ExceptionCode& ec)
    {
        if (m_role == AnimValRole) {
            ec = NO_MODIFICATION_ALLOWED_ERR;
            return 0;
        }

        PropertyType& values = m_animatedProperty->values();
        if (index >= values.size()) {
            ec = INDEX_SIZE_ERR;
            return 0;
        }

        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            ec = TYPE_MISMATCH_ERR;
            return 0;
        }

        ListWrapperCache& wrappers = m_animatedProperty->wrappers();
        ASSERT(values.size() == wrappers.size());
        RefPtr<ListItemTearOff> newItem = passNewItem;

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        // Spec: If the item is already in this list, note that the index of the item to replace is before the removal of the item.
        removeItemFromListIfNeeded(newItem.get(), &index);

        // Detach the existing wrapper.
        RefPtr<ListItemTearOff>& oldItem = wrappers.at(index);
        if (oldItem)
            oldItem->detachWrapper();

        // Update the value and the wrapper at the desired position 'index'. 
        values.at(index) = newItem->propertyReference();
        wrappers.at(index) = newItem;

        commitChange();
        return newItem.release();
    }

    PassListItemTearOff removeItem(unsigned index, ExceptionCode& ec)
    {
        if (m_role == AnimValRole) {
            ec = NO_MODIFICATION_ALLOWED_ERR;
            return 0;
        }

        PropertyType& values = m_animatedProperty->values();
        if (index >= values.size()) {
            ec = INDEX_SIZE_ERR;
            return 0;
        }

        ListWrapperCache& wrappers = m_animatedProperty->wrappers();
        ASSERT(values.size() == wrappers.size());

        // Detach the existing wrapper.
        RefPtr<ListItemTearOff>& oldItem = wrappers.at(index);
        if (oldItem) {
            oldItem->detachWrapper();
            wrappers.remove(index);
        }

        values.remove(index);

        commitChange();
        return oldItem.release();
    }

    PassListItemTearOff appendItem(PassListItemTearOff passNewItem, ExceptionCode& ec)
    {
        if (m_role == AnimValRole) {
            ec = NO_MODIFICATION_ALLOWED_ERR;
            return 0;
        }

        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            ec = TYPE_MISMATCH_ERR;
            return 0;
        }

        PropertyType& values = m_animatedProperty->values();
        ListWrapperCache& wrappers = m_animatedProperty->wrappers();

        RefPtr<ListItemTearOff> newItem = passNewItem;
        ASSERT(values.size() == wrappers.size());

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        removeItemFromListIfNeeded(newItem.get(), 0);

        // Append the value and wrapper at the end of the list.
        values.append(newItem->propertyReference());
        wrappers.append(newItem);

        commitChange();
        return newItem.release();
    }

private:
    SVGListPropertyTearOff(AnimatedListPropertyTearOff* animatedProperty, SVGPropertyRole role)
        : m_animatedProperty(animatedProperty)
        , m_role(role)
    {
        ASSERT(animatedProperty);
        ASSERT(role != UndefinedRole);
    }

    void commitChange()
    {
        PropertyType& values = m_animatedProperty->values();
        ListWrapperCache& wrappers = m_animatedProperty->wrappers();

        // Update existing wrappers, as the index in the values list has changed.
        unsigned size = wrappers.size();
        ASSERT(size == values.size());
        for (unsigned i = 0; i < size; ++i) {
            RefPtr<ListItemTearOff>& item = wrappers.at(i);
            if (!item)
                continue;
            item->setAnimatedProperty(m_animatedProperty.get());
            item->setValue(values.at(i));
        }

        m_animatedProperty->commitChange();
    }

    void removeItemFromListIfNeeded(ListItemTearOff* newItem, unsigned* indexToModify)
    {
        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        SVGAnimatedProperty* animatedPropertyOfItem = newItem->animatedProperty();
        if (!animatedPropertyOfItem || !animatedPropertyOfItem->isAnimatedListTearOff())
            return;

        // 'newItem' is already living in another list. If it's not our list, synchronize the other lists wrappers after the removal.
        bool livesInOtherList = animatedPropertyOfItem != m_animatedProperty;
        int removedIndex = static_cast<SVGAnimatedListPropertyTearOff<PropertyType>*>(animatedPropertyOfItem)->removeItemFromList(newItem, livesInOtherList);
        ASSERT(removedIndex != -1);

        if (!indexToModify)
            return;

        // If the item lived in our list, adjust the insertion index.
        if (!livesInOtherList) {
            unsigned& index = *indexToModify;
            // Spec: If the item is already in this list, note that the index of the item to (replace|insert before) is before the removal of the item.
            if (static_cast<unsigned>(removedIndex) < index)
                --index;
        }
    }

private:
    // Back pointer to the animated property that created us
    // For example (text.x.baseVal): m_animatedProperty points to the 'x' SVGAnimatedLengthList object
    RefPtr<AnimatedListPropertyTearOff> m_animatedProperty;

    // The role of this property (baseVal or animVal)
    SVGPropertyRole m_role;
};

}

#endif // ENABLE(SVG)
#endif // SVGListPropertyTearOff_h
