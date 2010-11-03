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

#ifndef SVGStringListPropertyTearOff_h
#define SVGStringListPropertyTearOff_h

#if ENABLE(SVG)
#include "ExceptionCode.h"
#include "SVGProperty.h"
#include "SVGStringList.h"

namespace WebCore {

// Used for lists that return non-modifyable values: SVGStringList.
class SVGStringListPropertyTearOff : public SVGProperty {
public:
    static PassRefPtr<SVGStringListPropertyTearOff> create(SVGElement* contextElement, SVGStringList& values)
    {
        return adoptRef(new SVGStringListPropertyTearOff(contextElement, values));
    }

    // SVGList API
    void clear(ExceptionCode&)
    {
        m_values.clear();
    }

    unsigned numberOfItems() const
    {
        return m_values.size();
    }

    String initialize(const String& newItem, ExceptionCode&)
    {
        // Spec: Clears all existing current items from the list and re-initializes the list to hold the single item specified by the parameter.
        m_values.clear();
        m_values.append(newItem);

        m_values.commitChange(m_contextElement.get());
        return newItem;
    }

    String getItem(unsigned index, ExceptionCode& ec)
    {
        if (index >= m_values.size()) {
            ec = INDEX_SIZE_ERR;
            return String();
        }

        return m_values.at(index);
    }

    String insertItemBefore(const String& newItem, unsigned index, ExceptionCode&)
    {
        // Spec: If the index is greater than or equal to numberOfItems, then the new item is appended to the end of the list.
        if (index > m_values.size())
             index = m_values.size();

        // Spec: Inserts a new item into the list at the specified position. The index of the item before which the new item is to be
        // inserted. The first item is number 0. If the index is equal to 0, then the new item is inserted at the front of the list.
        m_values.insert(index, newItem);

        m_values.commitChange(m_contextElement.get());
        return newItem;
    }

    String replaceItem(const String& newItem, unsigned index, ExceptionCode& ec)
    {
        if (index >= m_values.size()) {
            ec = INDEX_SIZE_ERR;
            return String();
        }

        // Update the value and the wrapper at the desired position 'index'. 
        m_values.at(index) = newItem;

        m_values.commitChange(m_contextElement.get());
        return newItem;
    }

    String removeItem(unsigned index, ExceptionCode& ec)
    {
        if (index >= m_values.size()) {
            ec = INDEX_SIZE_ERR;
            return String();
        }

        String oldItem = m_values.at(index);
        m_values.remove(index);

        m_values.commitChange(m_contextElement.get());
        return oldItem;
    }

    String appendItem(const String& newItem, ExceptionCode&)
    {
        // Append the value and wrapper at the end of the list.
        m_values.append(newItem);

        m_values.commitChange(m_contextElement.get());
        return newItem;
    }

private:
    SVGStringListPropertyTearOff(SVGElement* contextElement, SVGStringList& values)
        : m_contextElement(contextElement)
        , m_values(values)
    {
        ASSERT(m_contextElement);
    }

private:
    RefPtr<SVGElement> m_contextElement;
    SVGStringList& m_values;
};

}

#endif // ENABLE(SVG)
#endif // SVGStringListPropertyTearOff_h
