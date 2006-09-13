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

#ifndef KSVG_SVGList_H
#define KSVG_SVGList_H

#ifdef SVG_SUPPORT

#include <wtf/Vector.h>

#include "Shared.h"
#include "FloatPoint.h"
#include "ExceptionCode.h"
#include "PlatformString.h"

namespace WebCore {

    template<class Item>
    class SVGListBase : public Shared<SVGListBase<Item> >
    {
    public:
        SVGListBase() { }
        virtual ~SVGListBase() { clearVector(m_vector); }

        // To be implemented by the SVGList specializations!
        virtual Item nullItem() const = 0;
        virtual void clearVector(Vector<Item>& vector) const { vector.clear(); }

        unsigned int numberOfItems() const { return m_vector.size(); }
        void clear(ExceptionCode &) { clearVector(m_vector); }

        Item initialize(Item newItem, ExceptionCode& ec)
        {
            clear(ec);
            return appendItem(newItem, ec);
        }

        Item getFirst() const
        {
            if (m_vector.isEmpty())
                return nullItem();

            return m_vector.first();
        }

        Item getLast() const
        {
            if (m_vector.isEmpty())
                return nullItem();

            return m_vector.last();
        }

        Item getItem(unsigned int index, ExceptionCode& ec)
        {
            if (m_vector.size() < index) {
                ec = INDEX_SIZE_ERR;
                return nullItem();
            }

            return m_vector.at(index);
        }

        const Item getItem(unsigned int index, ExceptionCode& ec) const
        {
            if (m_vector.size() < index) {
                ec = INDEX_SIZE_ERR;
                return nullItem();
            }

            return m_vector.at(index);
        }

        Item insertItemBefore(Item newItem, unsigned int index, ExceptionCode&)
        {
            m_vector.insert(index, newItem);
            return newItem;
        }

        Item replaceItem(Item newItem, unsigned int index, ExceptionCode& ec)
        {
            if (m_vector.size() < index) {
                ec = INDEX_SIZE_ERR;
                return nullItem();
            }

            m_vector.at(index) = newItem;
            return newItem;
        }

        Item removeItem(unsigned int index, ExceptionCode& ec)
        {
            if (m_vector.size() < index) {
                ec = INDEX_SIZE_ERR;
                return nullItem();
            }

            Item item = m_vector.at(index);
            removeItem(index, ec);
            return item;
        }

        void removeItem(const Item item)
        {
            m_vector.remove(item);
        }

        Item appendItem(Item newItem, ExceptionCode&)
        {
            m_vector.append(newItem);
            return newItem;
        }

    private:
        Vector<Item> m_vector;
    };

    template<class Item>
    class SVGList : public SVGListBase<Item>
    {
    public:
        virtual Item nullItem() const { return 0; }

        virtual void clearVector(Vector<Item>& vector) const
        {
            typedef typename Vector<Item>::const_iterator iterator;
            
            iterator end = vector.end();
            for (iterator it = vector.begin(); it != end; ++it)
                (*it)->deref();

            vector.clear();    
        }
    };

    // Specialization for double
    template<>
    class SVGList<double> : public SVGListBase<double>
    {
    public:
        virtual double nullItem() const { return 0.0; }
    };

    // Specialization for String
    template<>
    class SVGList<String> : public SVGListBase<String>
    {
    public:
        virtual String nullItem() const { return String(); }
    };

    // Specialization for FloatPoint
    template<>
    class SVGList<FloatPoint> : public SVGListBase<FloatPoint>
    {
    public:
        virtual FloatPoint nullItem() const { return FloatPoint(); }
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // KSVG_SVGList_H

// vim:ts=4:noet
