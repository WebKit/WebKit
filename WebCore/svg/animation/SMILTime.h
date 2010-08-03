/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef SMILTime_h
#define SMILTime_h

#if ENABLE(SVG)

#include <algorithm>

namespace WebCore {

    class SMILTime {
    public:
        SMILTime() : m_time(0) { }
        SMILTime(double time) : m_time(time) { }
        SMILTime(const SMILTime& o) : m_time(o.m_time) { }
        
        static SMILTime unresolved() { return unresolvedValue; }
        static SMILTime indefinite() { return indefiniteValue; }
        
        SMILTime& operator=(const SMILTime& o) { m_time = o.m_time; return *this; }
        double value() const { return m_time; }
        
        bool isFinite() const { return m_time < indefiniteValue; }
        bool isIndefinite() const { return m_time == indefiniteValue; }
        bool isUnresolved() const { return m_time == unresolvedValue; }
        
    private:
        static const double unresolvedValue;
        static const double indefiniteValue;

        double m_time;
    };

    inline bool operator==(const SMILTime& a, const SMILTime& b) { return a.isFinite() && a.value() == b.value(); }
    inline bool operator!=(const SMILTime& a, const SMILTime& b) { return !operator==(a, b); }
    inline bool operator>(const SMILTime& a, const SMILTime& b) { return a.value() > b.value(); }
    inline bool operator<(const SMILTime& a, const SMILTime& b) { return a.value() < b.value(); }
    inline bool operator>=(const SMILTime& a, const SMILTime& b) { return a.value() > b.value() || operator==(a, b); }
    inline bool operator<=(const SMILTime& a, const SMILTime& b) { return a.value() < b.value() || operator==(a, b); }

    SMILTime operator+(const SMILTime&, const SMILTime&);
    SMILTime operator-(const SMILTime&, const SMILTime&);
    // So multiplying times does not make too much sense but SMIL defines it for duration * repeatCount
    SMILTime operator*(const SMILTime&, const SMILTime&);
}

#endif
#endif
