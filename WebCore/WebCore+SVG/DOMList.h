/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_DOMList_H
#define KDOM_DOMList_H
#if SVG_SUPPORT

#include "DeprecatedPtrList.h"
#include "Shared.h"

namespace WebCore
{
    template<class T>
    class DOMList : public Shared<DOMList<T> >
    {
    public:
        DOMList() : Shared<DOMList<T> >() { m_impl.setAutoDelete(false); }
        DOMList(const DOMList &other) { *this = other; }
        virtual ~DOMList() { clear(); }

//        DOMList<T> &operator=(const DOMList<T> &other)
//        {
//            // Clear own list
//            clear();
//
//            // Clone other's elements and append them
//            DOMList<T> &get = const_cast<DOMList<T> &>(other);
//            unsigned int nr = other.numberOfItems();
//            for(unsigned int i = 0; i < nr; i++)
//            {
//                T *obj = new T(*get.getItem(i));
//                obj->ref();
//
//                appendItem(obj);
//            }
//
//            return *this;
//        }

        unsigned int numberOfItems() const { return m_impl.count(); }

        virtual void clear()
        {
            for(unsigned int i = 0; i < numberOfItems(); i++)
                getItem(i)->deref();

            m_impl.clear();
        }

        T *initialize(T *newItem)
        {
            clear();
            return appendItem(newItem);
        }

        T *getFirst() const { return m_impl.getFirst(); }

        T *getLast() const { return m_impl.getLast(); }

        T *getItem(unsigned int index) { return m_impl.at(index); }
        const T *getItem(unsigned int index) const { return const_cast<DOMList<T> *>(this)->m_impl.at(index); }

        virtual T *insertItemBefore(T *newItem, unsigned int index)
        {
            m_impl.insert(index, newItem);
            return newItem;
        }

        virtual T *replaceItem(T *newItem, unsigned int index)
        {
            m_impl.take(index);
            m_impl.insert(index, newItem);
            return newItem;
        }

        virtual T *removeItem(unsigned int index)
        {
            return m_impl.take(index);
        }

        virtual void removeItem(const T *item)
        {
            m_impl.remove(item);
        }

        virtual T *appendItem(T *newItem)
        {
            m_impl.append(newItem);
            return newItem;
        }

        virtual bool contains(const T *item)
        {
            if(m_impl.findRef(item) != -1)
                return true;

            return false;
        }

    private:
        Q3PtrList<T> m_impl;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
