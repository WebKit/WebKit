/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGList_h
#define SVGList_h

#if ENABLE(SVG)
#include "ExceptionCode.h"
#include "SVGListTraits.h"

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

    class QualifiedName;

    template<typename Item>
    struct SVGListTypeOperations {
        static Item nullItem()
        {
            return SVGListTraits<UsesDefaultInitializer<Item>::value, Item>::nullItem();
        }
    };

    template<typename Item>
    class SVGList : public RefCounted<SVGList<Item> > {
    private:
        typedef SVGListTypeOperations<Item> TypeOperations;

    public:
        SVGList(const QualifiedName& attributeName) 
            : m_associatedAttributeName(attributeName)
        {
        }

        virtual ~SVGList() { m_vector.clear(); }

        const QualifiedName& associatedAttributeName() const { return m_associatedAttributeName; }

        unsigned int numberOfItems() const { return m_vector.size(); }
        void clear(ExceptionCode &) { m_vector.clear(); }

        Item initialize(Item newItem, ExceptionCode& ec)
        {
            clear(ec);
            return appendItem(newItem, ec);
        }

        Item getFirst() const
        {
            ExceptionCode ec = 0;
            return getItem(0, ec);
        }

        Item getLast() const
        {
            ExceptionCode ec = 0;
            return getItem(m_vector.size() - 1, ec);
        }

        Item getItem(unsigned int index, ExceptionCode& ec)
        {
            if (index >= m_vector.size()) {
                ec = INDEX_SIZE_ERR;
                return TypeOperations::nullItem();
            }

            return m_vector.at(index);
        }

        const Item getItem(unsigned int index, ExceptionCode& ec) const
        {
            if (index >= m_vector.size()) {
                ec = INDEX_SIZE_ERR;
                return TypeOperations::nullItem();
            }

            return m_vector[index];
        }

        Item insertItemBefore(Item newItem, unsigned int index, ExceptionCode&)
        {
            m_vector.insert(index, newItem);
            return newItem;
        }

        Item replaceItem(Item newItem, unsigned int index, ExceptionCode& ec)
        {
            if (index >= m_vector.size()) {
                ec = INDEX_SIZE_ERR;
                return TypeOperations::nullItem();
            }

            m_vector[index] = newItem;
            return newItem;
        }

        Item removeItem(unsigned int index, ExceptionCode& ec)
        {
            if (index >= m_vector.size()) {
                ec = INDEX_SIZE_ERR;
                return TypeOperations::nullItem();
            }

            Item item = m_vector[index];
            m_vector.remove(index);
            return item;
        }

        Item appendItem(Item newItem, ExceptionCode&)
        {
            m_vector.append(newItem);
            return newItem;
        }

    private:
        Vector<Item> m_vector;
        const QualifiedName& m_associatedAttributeName;
    };

    template<typename Item>
    class SVGPODListItem : public RefCounted<SVGPODListItem<Item> > {
    public:
        SVGPODListItem() : m_item() { }
        SVGPODListItem(const Item& item) : m_item(item) { }

        operator Item&() { return m_item; }
        operator const Item&() const { return m_item; }

        // Updating facilities, used by JSSVGPODTypeWrapperCreatorForList
        Item value() const { return m_item; }
        void setValue(Item newItem) { m_item = newItem; }

    private:
        Item m_item;
    };

    template<typename Item>
    class SVGPODList : public SVGList<RefPtr<SVGPODListItem<Item> > >
    {
    public:
        SVGPODList(const QualifiedName& attributeName) : SVGList<RefPtr<SVGPODListItem<Item> > >(attributeName) { }

        Item initialize(Item newItem, ExceptionCode& ec)
        {
            SVGPODListItem<Item>* ptr(SVGList<RefPtr<SVGPODListItem<Item> > >::initialize(new SVGPODListItem<Item>(newItem), ec).get());
            if (!ptr)
                return Item();

            return static_cast<const Item&>(*ptr); 
        }

        Item getFirst() const
        {
            SVGPODListItem<Item>* ptr(SVGList<RefPtr<SVGPODListItem<Item> > >::getFirst().get());
            if (!ptr)
                return Item();

            return static_cast<const Item&>(*ptr);
        }

        Item getLast() const
        {
            SVGPODListItem<Item>* ptr(SVGList<RefPtr<SVGPODListItem<Item> > >::getLast().get());
            if (!ptr)
                return Item();

            return static_cast<const Item&>(*ptr); 
        }

        Item getItem(unsigned int index, ExceptionCode& ec)
        {
            SVGPODListItem<Item>* ptr(SVGList<RefPtr<SVGPODListItem<Item> > >::getItem(index, ec).get());
            if (!ptr)
                return Item();

            return static_cast<const Item&>(*ptr);
        }

        const Item getItem(unsigned int index, ExceptionCode& ec) const
        {
            SVGPODListItem<Item>* ptr(SVGList<RefPtr<SVGPODListItem<Item> > >::getItem(index, ec).get());
             if (!ptr)
                return Item();

            return static_cast<const Item&>(*ptr);
        }

        Item insertItemBefore(Item newItem, unsigned int index, ExceptionCode& ec)
        {
            SVGPODListItem<Item>* ptr(SVGList<RefPtr<SVGPODListItem<Item> > >::insertItemBefore(new SVGPODListItem<Item>(newItem), index, ec).get());
            if (!ptr)
                return Item();

            return static_cast<const Item&>(*ptr); 
        }

        Item replaceItem(Item newItem, unsigned int index, ExceptionCode& ec)
        {
            SVGPODListItem<Item>* ptr(SVGList<RefPtr<SVGPODListItem<Item> > >::replaceItem(new SVGPODListItem<Item>(newItem), index, ec).get());
            if (!ptr)
                return Item();

            return static_cast<const Item&>(*ptr);
        }

        Item removeItem(unsigned int index, ExceptionCode& ec)
        {
            SVGPODListItem<Item>* ptr(SVGList<RefPtr<SVGPODListItem<Item> > >::removeItem(index, ec).get());
            if (!ptr)
                return Item();

            return static_cast<const Item&>(*ptr); 
        }

        Item appendItem(Item newItem, ExceptionCode& ec)
        {
            SVGPODListItem<Item>* ptr(SVGList<RefPtr<SVGPODListItem<Item> > >::appendItem(new SVGPODListItem<Item>(newItem), ec).get());
            if (!ptr)
                return Item();

            return static_cast<const Item&>(*ptr); 
        }
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGList_h
