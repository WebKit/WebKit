/*
    This file is part of the KDE libraries

    Copyright (C) 1999 Lars Knoll (knoll@kde.org)

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

#include <qrect.h>

/*
 * this namespace contains definitions for various types needed for
 * layouting.
 */
namespace khtml
{
    const int UNDEFINED = -1;

    // alignment
    enum VAlign { VNone=0, Bottom, VCenter, Top, Baseline };
    enum HAlign { HDefault, Left, HCenter, Right, HNone = 0 };

    /*
     * %multiLength and %Length
     */
    enum LengthType { Auto = 0, Relative, Percent, Fixed, Static, Intrinsic, MinIntrinsic };
    struct Length
    {
	Length() { *((Q_UINT32 *)this) = 0; }
        Length(LengthType t) { type = t; value = 0; quirk = false; }
        Length(int v, LengthType t, bool q=false) : value(v), type(t), quirk(q) {}
        Length(const Length &o)
	    { *((Q_UINT32 *)this) = *((Q_UINT32 *)&o); }

        Length& operator=(const Length& o)
            { *((Q_UINT32 *)this) = *((Q_UINT32 *)&o); return *this; }
        bool operator==(const Length& o) const
            { return *((Q_UINT32 *)this) == *((Q_UINT32 *)&o); }
        bool operator!=(const Length& o) const
            { return *((Q_UINT32 *)this) != *((Q_UINT32 *)&o); }


	int length() const { return value; }

	/*
	 * works only for Fixed and Percent, returns -1 otherwise
	 */
	int width(int maxWidth) const
	    {
		switch(type)
		{
		case Fixed:
		    return value;
		case Percent:
		    return maxWidth*value/100;
		case Auto:
		    return maxWidth;
		default:
		    return -1;
		}
	    }
	/*
	 * returns the minimum width value which could work...
	 */
	int minWidth(int maxWidth) const
	    {
		switch(type)
		{
		case Fixed:
		    return value;
		case Percent:
		    return maxWidth*value/100;
		case Auto:
		default:
		    return 0;
		}
	    }
        bool isAuto() const { return (type == Auto); }
        bool isRelative() const { return (type == Relative); }
        bool isPercent() const { return (type == Percent); }
        bool isFixed() const { return (type == Fixed); }
        bool isStatic() const { return (type == Static); }
        bool isIntrinsicOrAuto() const { return (type == Auto || type == MinIntrinsic || type == Intrinsic); }

        int value : 28;
        LengthType type : 3;
        bool quirk : 1;
    };

    struct GapRects {
        QRect m_left;
        QRect m_center;
        QRect m_right;
        
        QRect left() const { return m_left; }
        QRect center() const { return m_center; }
        QRect right() const { return m_right; }
        
        void uniteLeft(const QRect& r) { m_left = m_left.unite(r); }
        void uniteCenter(const QRect& r) { m_center = m_center.unite(r); }
        void uniteRight(const QRect& r) { m_right = m_right.unite(r); }
        void unite(const GapRects& o) { uniteLeft(o.left()); uniteCenter(o.center()); uniteRight(o.right()); }

        operator QRect() const {
            QRect result = m_left.unite(m_center);
            result = result.unite(m_right);
            return result;
        }
        bool operator==(const GapRects& other) {
            return m_left == other.left() && m_center == other.center() && m_right == other.right();
        }
        bool operator!=(const GapRects& other) { return !(*this == other); }
    };
};

#endif
