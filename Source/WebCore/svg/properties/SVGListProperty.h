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

#include "SVGPropertyTearOff.h"
#include "SVGPropertyTraits.h"
#include <wtf/Ref.h>

namespace WebCore {

enum ListModification {
    ListModificationUnknown = 0,
    ListModificationInsert = 1,
    ListModificationReplace = 2,
    ListModificationRemove = 3,
    ListModificationAppend = 4
};

template<typename PropertyType>
class SVGAnimatedListPropertyTearOff;

template<typename PropertyType>
class SVGListProperty : public SVGProperty {
public:
    typedef SVGListProperty<PropertyType> Self;

    using ListItemType = typename SVGPropertyTraits<PropertyType>::ListItemType;
    using ListItemTearOff = typename SVGPropertyTraits<PropertyType>::ListItemTearOff;
    using AnimatedListPropertyTearOff = SVGAnimatedListPropertyTearOff<PropertyType>;
    using ListWrapperCache = typename AnimatedListPropertyTearOff::ListWrapperCache;

    ExceptionOr<bool> canAlterList() const
    {
        if (m_role == AnimValRole)
            return Exception { NoModificationAllowedError };

        return true;
    }

    static void detachListWrappersAndResize(ListWrapperCache* wrappers, unsigned newListSize = 0)
    {
        // See SVGPropertyTearOff::detachWrapper() for an explanation about what's happening here.
        ASSERT(wrappers);
        for (auto& item : *wrappers) {
            if (item)
                item->detachWrapper();
        }

        // Reinitialize the wrapper cache to be equal to the new values size, after the XML DOM changed the list.
        if (newListSize)
            wrappers->fill(0, newListSize);
        else
            wrappers->clear();
    }

    void detachListWrappers(unsigned newListSize)
    {
        detachListWrappersAndResize(m_wrappers, newListSize);
    }

    void setValuesAndWrappers(PropertyType* values, ListWrapperCache* wrappers, bool shouldOwnValues)
    {
        // This is only used for animVal support, to switch the underlying values & wrappers
        // to the current animated values, once animation for a list starts.
        ASSERT(m_values);
        ASSERT(m_wrappers);
        ASSERT(m_role == AnimValRole);
        if (m_ownsValues)
            delete m_values;
        m_values = values;
        m_ownsValues = shouldOwnValues;
        m_wrappers = wrappers;
        ASSERT(m_values->size() == m_wrappers->size());
    }

    // SVGList::clear()
    ExceptionOr<void> clearValues()
    {
        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        m_values->clear();
        commitChange();
        return { };
    }

    ExceptionOr<void> clearValuesAndWrappers()
    {
        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        detachListWrappers(0);
        m_values->clear();
        commitChange();
        return { };
    }

    // SVGList::numberOfItems()
    unsigned numberOfItems() const
    {
        return m_values->size();
    }

    // SVGList::initialize()
    ExceptionOr<ListItemType> initializeValues(const ListItemType& newItem)
    {
        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        // Spec: If the inserted item is already in a list, it is removed from its previous list before it is inserted into this list.
        processIncomingListItemValue(newItem, 0);

        // Spec: Clears all existing current items from the list and re-initializes the list to hold the single item specified by the parameter.
        m_values->clear();
        m_values->append(newItem);

        commitChange();
        return ListItemType { newItem };
    }

    ExceptionOr<Ref<ListItemTearOff>> initializeValuesAndWrappers(ListItemTearOff& item)
    {
        ASSERT(m_wrappers);

        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        ASSERT(m_values->size() == m_wrappers->size());

        Ref<ListItemTearOff> newItem(item);

        // Spec: If the inserted item is already in a list, it is removed from its previous list before it is inserted into this list.
        processIncomingListItemWrapper(newItem, 0);

        // Spec: Clears all existing current items from the list and re-initializes the list to hold the single item specified by the parameter.
        detachListWrappers(0);
        m_values->clear();

        m_values->append(newItem->propertyReference());
        m_wrappers->append(newItem.ptr());

        commitChange();
        return WTFMove(newItem);
    }

    // SVGList::getItem()
    ExceptionOr<bool> canGetItem(unsigned index)
    {
        if (index >= m_values->size())
            return Exception { IndexSizeError };

        return true;
    }

    ExceptionOr<ListItemType> getItemValues(unsigned index)
    {
        auto result = canGetItem(index);
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        // Spec: Returns the specified item from the list. The returned item is the item itself and not a copy.
        return ListItemType { m_values->at(index) };
    }

    ExceptionOr<Ref<ListItemTearOff>> getItemValuesAndWrappers(AnimatedListPropertyTearOff& animatedList, unsigned index)
    {
        ASSERT(m_wrappers);

        auto result = canGetItem(index);
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        // Spec: Returns the specified item from the list. The returned item is the item itself and not a copy.
        // Any changes made to the item are immediately reflected in the list.
        ASSERT(m_values->size() == m_wrappers->size());
        RefPtr<ListItemTearOff> wrapper = m_wrappers->at(index);
        if (!wrapper) {
            // Create new wrapper, which is allowed to directly modify the item in the list, w/o copying and cache the wrapper in our map.
            // It is also associated with our animated property, so it can notify the SVG Element which holds the SVGAnimated*List
            // that it has been modified (and thus can call svgAttributeChanged(associatedAttributeName)).
            wrapper = ListItemTearOff::create(animatedList, UndefinedRole, m_values->at(index));
            m_wrappers->at(index) = wrapper.get();
        }

        return wrapper.releaseNonNull();
    }

    // SVGList::insertItemBefore()
    ExceptionOr<ListItemType> insertItemBeforeValues(const ListItemType& newItem, unsigned index)
    {
        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        // Spec: If the index is greater than or equal to numberOfItems, then the new item is appended to the end of the list.
        if (index > m_values->size())
            index = m_values->size();

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        if (!processIncomingListItemValue(newItem, &index)) {
            // Inserting the item before itself is a no-op.
            return ListItemType { newItem };
        }

        // Spec: Inserts a new item into the list at the specified position. The index of the item before which the new item is to be
        // inserted. The first item is number 0. If the index is equal to 0, then the new item is inserted at the front of the list.
        m_values->insert(index, newItem);

        commitChange();
        return ListItemType { newItem };
    }

    ExceptionOr<Ref<ListItemTearOff>> insertItemBeforeValuesAndWrappers(ListItemTearOff& item, unsigned index)
    {
        ASSERT(m_wrappers);

        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        // Spec: If the index is greater than or equal to numberOfItems, then the new item is appended to the end of the list.
        if (index > m_values->size())
            index = m_values->size();

        ASSERT(m_values->size() == m_wrappers->size());

        Ref<ListItemTearOff> newItem(item);

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        if (!processIncomingListItemWrapper(newItem, &index))
            return WTFMove(newItem);

        // Spec: Inserts a new item into the list at the specified position. The index of the item before which the new item is to be
        // inserted. The first item is number 0. If the index is equal to 0, then the new item is inserted at the front of the list.
        m_values->insert(index, newItem->propertyReference());

        // Store new wrapper at position 'index', change its underlying value, so mutations of newItem, directly affect the item in the list.
        m_wrappers->insert(index, newItem.ptr());

        commitChange();
        return WTFMove(newItem);
    }

    // SVGList::replaceItem()
    ExceptionOr<bool> canReplaceItem(unsigned index)
    {
        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        if (index >= m_values->size())
            return Exception { IndexSizeError };

        return true;
    }

    ExceptionOr<ListItemType> replaceItemValues(const ListItemType& newItem, unsigned index)
    {
        auto result = canReplaceItem(index);
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        // Spec: If the item is already in this list, note that the index of the item to replace is before the removal of the item.
        if (!processIncomingListItemValue(newItem, &index)) {
            // Replacing the item with itself is a no-op.
            return ListItemType { newItem };
        }

        if (m_values->isEmpty()) {
            // 'newItem' already lived in our list, we removed it, and now we're empty, which means there's nothing to replace.
            return Exception { IndexSizeError };
        }

        // Update the value at the desired position 'index'. 
        m_values->at(index) = newItem;

        commitChange();
        return ListItemType { newItem };
    }

    ExceptionOr<Ref<ListItemTearOff>> replaceItemValuesAndWrappers(ListItemTearOff& item, unsigned index)
    {
        ASSERT(m_wrappers);

        auto result = canReplaceItem(index);
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        ASSERT(m_values->size() == m_wrappers->size());

        Ref<ListItemTearOff> newItem(item);

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        // Spec: If the item is already in this list, note that the index of the item to replace is before the removal of the item.
        if (!processIncomingListItemWrapper(newItem, &index))
            return WTFMove(newItem);

        if (m_values->isEmpty()) {
            ASSERT(m_wrappers->isEmpty());
            // 'newItem' already lived in our list, we removed it, and now we're empty, which means there's nothing to replace.
            return Exception { IndexSizeError };
        }

        // Detach the existing wrapper.
        RefPtr<ListItemTearOff> oldItem = m_wrappers->at(index);
        if (oldItem)
            oldItem->detachWrapper();

        // Update the value and the wrapper at the desired position 'index'. 
        m_values->at(index) = newItem->propertyReference();
        m_wrappers->at(index) = newItem.ptr();

        commitChange();
        return WTFMove(newItem);
    }

    // SVGList::removeItem()
    ExceptionOr<bool> canRemoveItem(unsigned index)
    {
        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        if (index >= m_values->size())
            return Exception { IndexSizeError };

        return true;
    }

    ExceptionOr<ListItemType> removeItemValues(unsigned index)
    {
        auto result = canRemoveItem(index);
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        ListItemType oldItem = m_values->at(index);
        m_values->remove(index);

        commitChange();
        return WTFMove(oldItem);
    }

    ExceptionOr<Ref<ListItemTearOff>> removeItemValuesAndWrappers(AnimatedListPropertyTearOff& animatedList, unsigned index)
    {
        ASSERT(m_wrappers);

        auto result = canRemoveItem(index);
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        ASSERT(m_values->size() == m_wrappers->size());

        // Detach the existing wrapper.
        RefPtr<ListItemTearOff> oldItem = m_wrappers->at(index);
        if (!oldItem)
            oldItem = ListItemTearOff::create(animatedList, UndefinedRole, m_values->at(index));

        oldItem->detachWrapper();
        m_wrappers->remove(index);
        m_values->remove(index);

        commitChange();
        return oldItem.releaseNonNull();
    }

    // SVGList::appendItem()
    ExceptionOr<ListItemType> appendItemValues(const ListItemType& newItem)
    {
        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        processIncomingListItemValue(newItem, 0);

        // Append the value at the end of the list.
        m_values->append(newItem);

        commitChange(ListModificationAppend);
        return ListItemType { newItem };
    }

    ExceptionOr<Ref<ListItemTearOff>> appendItemValuesAndWrappers(ListItemTearOff& item)
    {
        ASSERT(m_wrappers);

        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        ASSERT(m_values->size() == m_wrappers->size());

        Ref<ListItemTearOff> newItem(item);

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        processIncomingListItemWrapper(newItem, 0);

        // Append the value and wrapper at the end of the list.
        m_values->append(newItem->propertyReference());
        m_wrappers->append(newItem.ptr());

        commitChange(ListModificationAppend);
        return WTFMove(newItem);
    }

    PropertyType& values()
    {
        ASSERT(m_values);
        return *m_values;
    }

    ListWrapperCache& wrappers() const
    {
        ASSERT(m_wrappers);
        return *m_wrappers;
    }

protected:
    SVGListProperty(SVGPropertyRole role, PropertyType& values, ListWrapperCache* wrappers)
        : m_role(role)
        , m_ownsValues(false)
        , m_values(&values)
        , m_wrappers(wrappers)
    {
    }

    virtual ~SVGListProperty()
    {
        if (m_ownsValues)
            delete m_values;
    }

    void commitChange() override = 0;
    virtual void commitChange(ListModification)
    {
        commitChange();
    }

    virtual bool processIncomingListItemValue(const ListItemType& newItem, unsigned* indexToModify) = 0;
    virtual bool processIncomingListItemWrapper(Ref<ListItemTearOff>& newItem, unsigned* indexToModify) = 0;

    SVGPropertyRole m_role;
    bool m_ownsValues;
    PropertyType* m_values;
    ListWrapperCache* m_wrappers;
};

}
