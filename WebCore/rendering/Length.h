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
*/

#ifndef LENGTH_H
#define LENGTH_H

namespace WebCore {

    const int undefinedLength = -1;

    enum LengthType { Auto, Relative, Percent, Fixed, Static, Intrinsic, MinIntrinsic };

    struct Length {
        Length() : m_value(0) { }
        Length(LengthType t) : m_value(t) { }
        Length(int v, LengthType t, bool q = false) : m_value((v * 16) | (q << 3) | t) { }
            // FIXME: Doesn't work if the passed-in value is very large!

        bool operator==(const Length& o) const { return m_value == o.m_value; }
        bool operator!=(const Length& o) const { return m_value != o.m_value; }

        int value() const { return (m_value & ~0xF) / 16; }
        LengthType type() const { return static_cast<LengthType>(m_value & 7); }
        bool quirk() const { return (m_value >> 3) & 1; }

        void setValue(LengthType t, int value) { m_value = value * 16 | (m_value & 0x8) | t; }
        void setValue(int value) { m_value = value * 16 | (m_value & 0xF); }

        // note: works only for certain types, returns undefinedLength otherwise
        int calcValue(int maxValue) const
        {
            switch (type()) {
                case Fixed:
                    return value();
                case Percent:
                    return maxValue * value() / 100;
                case Auto:
                    return maxValue;
                default:
                    return undefinedLength;
            }
        }

        int calcMinValue(int maxValue) const
        {
            switch (type()) {
                case Fixed:
                    return value();
                case Percent:
                    return maxValue * value() / 100;
                case Auto:
                default:
                    return 0;
            }
        }

        bool isAuto() const { return type() == Auto; }
        bool isRelative() const { return type() == Relative; }
        bool isPercent() const { return type() == Percent; }
        bool isFixed() const { return type() == Fixed; }
        bool isStatic() const { return type() == Static; }
        bool isIntrinsicOrAuto() const { return type() == Auto || type() == MinIntrinsic || type() == Intrinsic; }

    private:
        int m_value;
    };

}

#endif
