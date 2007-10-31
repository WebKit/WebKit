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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef Length_h
#define Length_h

#include <wtf/Assertions.h>

namespace WebCore {

    const int undefinedLength = -1;
    const int percentScaleFactor = 128;

    enum LengthType { Auto, Relative, Percent, Fixed, Static, Intrinsic, MinIntrinsic };

    struct Length {
        Length()
            : m_value(0)
        {
        }

        Length(LengthType t)
            : m_value(t)
        {
        }

        Length(int v, LengthType t, bool q = false)
            : m_value((v * 16) | (q << 3) | t) // FIXME: Doesn't work if the passed-in value is very large!
        {
            ASSERT(t != Percent);
        }

        Length(double v, LengthType t, bool q = false)
            : m_value(static_cast<int>(v * percentScaleFactor) * 16 | (q << 3) | t)
        {
            ASSERT(t == Percent);
        }

        bool operator==(const Length& o) const { return m_value == o.m_value; }
        bool operator!=(const Length& o) const { return m_value != o.m_value; }

        int value() const {
            ASSERT(type() != Percent);
            return rawValue();
        }

        int rawValue() const { return (m_value & ~0xF) / 16; }

        double percent() const
        {
            ASSERT(type() == Percent);
            return static_cast<double>(rawValue()) / percentScaleFactor;
        }

        LengthType type() const { return static_cast<LengthType>(m_value & 7); }
        bool quirk() const { return (m_value >> 3) & 1; }

        void setValue(LengthType t, int value)
        {
            ASSERT(t != Percent);
            setRawValue(t, value);
        }

        void setRawValue(LengthType t, int value) { m_value = value * 16 | (m_value & 0x8) | t; }

        void setValue(int value)
        {
            ASSERT(!value || type() != Percent);
            setRawValue(value);
        }

        void setRawValue(int value) { m_value = value * 16 | (m_value & 0xF); }

        void setValue(LengthType t, double value)
        {
            ASSERT(t == Percent);
            m_value = static_cast<int>(value * percentScaleFactor) * 16 | (m_value & 0x8) | t;
        }

        void setValue(double value)
        {
            ASSERT(type() == Percent);
            m_value = static_cast<int>(value * percentScaleFactor) * 16 | (m_value & 0xF);
        }

        // note: works only for certain types, returns undefinedLength otherwise
        int calcValue(int maxValue) const
        {
            switch (type()) {
                case Fixed:
                    return value();
                case Percent:
                    return maxValue * rawValue() / (100 * percentScaleFactor);
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
                    return maxValue * rawValue() / (100 * percentScaleFactor);
                case Auto:
                default:
                    return 0;
            }
        }

        bool isUndefined() const { return rawValue() == undefinedLength; }
        bool isZero() const { return !(m_value & ~0xF); }
        bool isPositive() const { return rawValue() > 0; }
        bool isNegative() const { return rawValue() < 0; }

        bool isAuto() const { return type() == Auto; }
        bool isRelative() const { return type() == Relative; }
        bool isPercent() const { return type() == Percent; }
        bool isFixed() const { return type() == Fixed; }
        bool isStatic() const { return type() == Static; }
        bool isIntrinsicOrAuto() const { return type() == Auto || type() == MinIntrinsic || type() == Intrinsic; }

        Length blend(const Length& from, double progress) const
        {
            // Blend two lengths to produce a new length that is in between them.  Used for animation.
            if (!from.isZero() && !isZero() && from.type() != type())
                return *this;
    
            if (from.isZero() && isZero())
                return *this;
            
            LengthType resultType = type();
            if (isZero())
                resultType = from.type();
            
            if (resultType == Percent) {
                double fromPercent = from.isZero() ? 0. : from.percent();
                double toPercent = isZero() ? 0. : percent();
                return Length(fromPercent + (toPercent - fromPercent) * progress, Percent);
            } 
                
            int fromValue = from.isZero() ? 0 : from.value();
            int toValue = isZero() ? 0 : value();
            return Length(int(fromValue + (toValue - fromValue) * progress), resultType);
        }
        
    private:
        int m_value;
    };

} // namespace WebCore

#endif // Length_h
