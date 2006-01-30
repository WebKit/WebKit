/*
    This file is part of the KDE libraries

    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006 Apple Computer, Inc.

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


    This widget holds some useful definitions needed for layouting Elements
*/

#ifndef HTML_LAYOUT_H
#define HTML_LAYOUT_H

#include "IntRect.h"
#include <stdint.h>

namespace WebCore {

    const int UNDEFINED = -1;

    enum LengthType { Auto = 0, Relative, Percent, Fixed, Static, Intrinsic, MinIntrinsic };
    struct Length
    {
        // FIXME: A more reliable and straightforward way of doing this is to just have
        // an unsigned and extract the bits as needed, instead of relying on bit fields layout
        // fitting in a 32-bit integer. Should fix this.

        Length() { *((uint32_t *)this) = 0; }
        Length(LengthType t) { type = t; value = 0; quirk = false; }
        Length(int v, LengthType t, bool q=false) : value(v), type(t), quirk(q) {}
        Length(const Length &o)
            { *((uint32_t *)this) = *((uint32_t *)&o); }

        Length& operator=(const Length& o)
            { *((uint32_t *)this) = *((uint32_t *)&o); return *this; }
        bool operator==(const Length& o) const
            { return *((uint32_t *)this) == *((uint32_t *)&o); }
        bool operator!=(const Length& o) const
            { return *((uint32_t *)this) != *((uint32_t *)&o); }


        int length() const { return value; }

        // note: works only for certain types, returns -1 otherwise
        int width(int maxWidth) const {
            switch (type) {
                case Fixed:
                    return value;
                case Percent:
                    return maxWidth * value / 100;
                case Auto:
                    return maxWidth;
                default:
                    return -1;
            }
        }

        int minWidth(int maxWidth) const {
            switch (type) {
                case Fixed:
                    return value;
                case Percent:
                    return maxWidth * value / 100;
                case Auto:
                default:
                    return 0;
            }
        }

        bool isAuto() const { return type == Auto; }
        bool isRelative() const { return type == Relative; }
        bool isPercent() const { return type == Percent; }
        bool isFixed() const { return type == Fixed; }
        bool isStatic() const { return type == Static; }
        bool isIntrinsicOrAuto() const { return type == Auto || type == MinIntrinsic || type == Intrinsic; }

        int value : 28;
        LengthType type : 3;
        bool quirk : 1;
    };

    struct GapRects {
        const IntRect& left() const { return m_left; }
        const IntRect& center() const { return m_center; }
        const IntRect& right() const { return m_right; }
        
        void uniteLeft(const IntRect& r) { m_left.unite(r); }
        void uniteCenter(const IntRect& r) { m_center.unite(r); }
        void uniteRight(const IntRect& r) { m_right.unite(r); }
        void unite(const GapRects& o) { uniteLeft(o.left()); uniteCenter(o.center()); uniteRight(o.right()); }

        operator IntRect() const {
            IntRect result = m_left;
            result.unite(m_center);
            result.unite(m_right);
            return result;
        }
        bool operator==(const GapRects& other) {
            return m_left == other.left() && m_center == other.center() && m_right == other.right();
        }
        bool operator!=(const GapRects& other) { return !(*this == other); }

    private:
        IntRect m_left;
        IntRect m_center;
        IntRect m_right;
    };

}

#endif
