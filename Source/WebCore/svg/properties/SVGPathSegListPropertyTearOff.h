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

#ifndef SVGPathSegListPropertyTearOff_h
#define SVGPathSegListPropertyTearOff_h

#if ENABLE(SVG)
#include "SVGAnimatedListPropertyTearOff.h"
#include "SVGPathSegList.h"

namespace WebCore {

class SVGPathElement;

class SVGPathSegListPropertyTearOff : public SVGListProperty<SVGPathSegList> {
public:
    typedef SVGListProperty<SVGPathSegList> Base;
    typedef SVGAnimatedListPropertyTearOff<SVGPathSegList> AnimatedListPropertyTearOff;
    typedef SVGPropertyTraits<SVGPathSegList>::ListItemType ListItemType;
    typedef PassRefPtr<SVGPathSeg> PassListItemType;

    static PassRefPtr<SVGPathSegListPropertyTearOff> create(AnimatedListPropertyTearOff* animatedProperty, SVGPropertyRole role, SVGPathSegRole pathSegRole, SVGPathSegList& values, ListWrapperCache& wrappers)
    {
        ASSERT(animatedProperty);
        return adoptRef(new SVGPathSegListPropertyTearOff(animatedProperty, role, pathSegRole, values, wrappers));
    }

    int removeItemFromList(const ListItemType& removeItem, bool shouldSynchronizeWrappers)
    {
        ASSERT(m_values);
        unsigned size = m_values->size();
        for (unsigned i = 0; i < size; ++i) {
            ListItemType& item = m_values->at(i);
            if (item != removeItem)
                continue;

            m_values->remove(i);

            if (shouldSynchronizeWrappers)
                commitChange();

            return i;
        }

        return -1;
    }

    // SVGList API
    void clear(ExceptionCode&);

    PassListItemType initialize(PassListItemType passNewItem, ExceptionCode& ec)
    {
        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            ec = SVGException::SVG_WRONG_TYPE_ERR;
            return 0;
        }

        ListItemType newItem = passNewItem;
        return Base::initializeValues(newItem, ec);
    }

    PassListItemType getItem(unsigned index, ExceptionCode&);

    PassListItemType insertItemBefore(PassListItemType passNewItem, unsigned index, ExceptionCode& ec)
    {
        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            ec = SVGException::SVG_WRONG_TYPE_ERR;
            return 0;
        }

        ListItemType newItem = passNewItem;
        return Base::insertItemBeforeValues(newItem, index, ec);
    }

    PassListItemType replaceItem(PassListItemType passNewItem, unsigned index, ExceptionCode& ec)
    {
        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            ec = SVGException::SVG_WRONG_TYPE_ERR;
            return 0;
        }

        ListItemType newItem = passNewItem;
        return Base::replaceItemValues(newItem, index, ec);
    }

    PassListItemType removeItem(unsigned index, ExceptionCode&);

    PassListItemType appendItem(PassListItemType passNewItem, ExceptionCode& ec)
    {
        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            ec = SVGException::SVG_WRONG_TYPE_ERR;
            return 0;
        }

        ListItemType newItem = passNewItem;
        return Base::appendItemValues(newItem, ec);
    }

private:
    SVGPathSegListPropertyTearOff(AnimatedListPropertyTearOff* animatedProperty, SVGPropertyRole role, SVGPathSegRole pathSegRole, SVGPathSegList& values, ListWrapperCache& wrappers)
        : SVGListProperty<SVGPathSegList>(role, values, &wrappers)
        , m_animatedProperty(animatedProperty)
        , m_pathSegRole(pathSegRole)
    {
    }

    SVGPathElement* contextElement() const;

    virtual void commitChange()
    {
        ASSERT(m_values);
        m_values->commitChange(m_animatedProperty->contextElement());
    }

    virtual void processIncomingListItemValue(const ListItemType& newItem, unsigned* indexToModify);
    virtual void processIncomingListItemWrapper(RefPtr<ListItemTearOff>&, unsigned*)
    {
        ASSERT_NOT_REACHED();
    }

private:
    RefPtr<AnimatedListPropertyTearOff> m_animatedProperty;
    SVGPathSegRole m_pathSegRole;
};

}

#endif // ENABLE(SVG)
#endif // SVGListPropertyTearOff_h
