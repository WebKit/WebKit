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
class SVGStaticListPropertyTearOff : public SVGListProperty<PropertyType> {
public:
    using Self = SVGStaticListPropertyTearOff<PropertyType>;
    using Base = SVGListProperty<PropertyType>;

    using ListItemType = typename SVGPropertyTraits<PropertyType>::ListItemType;
    using ListItemTearOff = typename SVGPropertyTraits<PropertyType>::ListItemTearOff;

    using Base::m_role;
    using Base::m_values;

    static Ref<Self> create(SVGElement& contextElement, PropertyType& values)
    {
        return adoptRef(*new Self(contextElement, values));
    }

    ExceptionOr<void> clear()
    {
        return Base::clearValues();
    }

    ExceptionOr<ListItemType> initialize(const ListItemType& newItem)
    {
        return Base::initializeValues(newItem);
    }

    ExceptionOr<ListItemType> getItem(unsigned index)
    {
        return Base::getItemValues(index);
    }

    ExceptionOr<ListItemType> insertItemBefore(const ListItemType& newItem, unsigned index)
    {
        return Base::insertItemBeforeValues(newItem, index);
    }

    ExceptionOr<ListItemType> replaceItem(const ListItemType& newItem, unsigned index)
    {
        return Base::replaceItemValues(newItem, index);
    }

    ExceptionOr<ListItemType> removeItem(unsigned index)
    {
        return Base::removeItemValues(index);
    }

    ExceptionOr<ListItemType> appendItem(const ListItemType& newItem)
    {
        return Base::appendItemValues(newItem);
    }

protected:
    SVGStaticListPropertyTearOff(SVGElement* contextElement, PropertyType& values)
        : Base(UndefinedRole, values, nullptr)
        , m_contextElement(*contextElement)
    {
    }

    bool isReadOnly() const override
    {
        return m_role == AnimValRole;
    }

    void commitChange() override
    {
        ASSERT(m_values);
        m_values->commitChange(m_contextElement);
    }

    bool processIncomingListItemValue(const ListItemType&, unsigned*) override
    {
        // no-op for static lists
        return true;
    }

    bool processIncomingListItemWrapper(Ref<ListItemTearOff>&, unsigned*) override
    {
        ASSERT_NOT_REACHED();
        return true;
    }

    Ref<SVGElement> m_contextElement;
};

}
