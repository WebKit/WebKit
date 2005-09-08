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

#ifndef KDOM_RectImpl_H
#define KDOM_RectImpl_H

#include <kdom/Shared.h>

namespace KDOM
{
    class CDFInterface;
    class CSSPrimitiveValueImpl;
    class RectImpl : public Shared
    {
    public:
        RectImpl(CDFInterface *interface);
        virtual ~RectImpl();

        // 'Rect' functions
        CSSPrimitiveValueImpl *top() const { return m_top; }
        CSSPrimitiveValueImpl *right() const { return m_right; }
        CSSPrimitiveValueImpl *bottom() const { return m_bottom; }
        CSSPrimitiveValueImpl *left() const { return m_left; }

        void setTop(CSSPrimitiveValueImpl *top);
        void setRight(CSSPrimitiveValueImpl *right);
        void setBottom(CSSPrimitiveValueImpl *bottom);
        void setLeft(CSSPrimitiveValueImpl *left);

    protected:
        CDFInterface *m_interface;

        mutable CSSPrimitiveValueImpl *m_top;
        mutable CSSPrimitiveValueImpl *m_right;
        mutable CSSPrimitiveValueImpl *m_bottom;
        mutable CSSPrimitiveValueImpl *m_left;
    };
};

#endif

// vim:ts=4:noet
