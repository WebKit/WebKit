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
#if SVG_SUPPORT

#include <q3ptrlist.h>
#include <kdom/core/DOMList.h>
#include <ksvg2/svg/SVGStyledElementImpl.h>

namespace WebCore
{
    template<class T>
    class SVGList : public DOMList<T>
    {
    public:
        SVGList(const SVGStyledElementImpl *context = 0)
        : DOMList<T>(), m_context(context) {}

        void clear()
        {
            DOMList<T>::clear();

            if(m_context)
                m_context->notifyAttributeChange();
        }

        T *insertItemBefore(T *newItem, unsigned int index)
        {
            T *ret = DOMList<T>::insertItemBefore(newItem, index);

            if(m_context)
                m_context->notifyAttributeChange();

            return ret;
        }

        T *replaceItem(T *newItem, unsigned int index)
        {
            T *ret = DOMList<T>::replaceItem(newItem, index);

            if(m_context)
                m_context->notifyAttributeChange();

            return ret;
        }

        T *removeItem(unsigned int index)
        {
            T *ret = DOMList<T>::removeItem(index);

            if(m_context)
                m_context->notifyAttributeChange();

            return ret;
        }

        T *appendItem(T *newItem)
        {
            T *ret = DOMList<T>::appendItem(newItem);

            if(m_context)
                m_context->notifyAttributeChange();

            return ret;
        }

    protected:
        const SVGStyledElementImpl *m_context;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
