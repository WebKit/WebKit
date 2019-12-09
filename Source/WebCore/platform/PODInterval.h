/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#ifndef NDEBUG
#include <wtf/text/TextStream.h>
#endif

namespace WebCore {

// Class representing a closed interval which can hold arbitrary
// endpoints and a piece of user
// data. An important characteristic for the algorithms we use is that
// if two intervals have identical endpoints but different user data,
// they are not considered to be equal. This situation can arise when
// representing the vertical extents of bounding boxes of overlapping
// triangles, where the pointer to the triangle is the user data of
// the interval.
//
// *Note* that the destructors of type T and UserData will *not* be
// called by this class. They must not allocate any memory that is
// required to be cleaned up in their destructors.
//
// The following constructors and operators must be implemented on
// type T:
//
//   - Copy constructor
//   - operator<
//   - operator==
//   - operator=
//
// The UserData type must support a copy constructor and assignment
// operator.
//
// In debug mode, printing of intervals and the data they contain is
// enabled. This uses WTF::TextStream.
//
// Note that this class requires a copy constructor and assignment
// operator in order to be stored in the red-black tree.

// FIXME: The prefix "POD" here isn't correct; this works with non-POD types.

template<class T, class UserData>
class PODInterval {
public:
    PODInterval(const T& low, const T& high)
        : m_low(low)
        , m_high(high)
        , m_maxHigh(high)
    {
    }

    PODInterval(const T& low, const T& high, const UserData& data)
        : m_low(low)
        , m_high(high)
        , m_data(data)
        , m_maxHigh(high)
    {
    }

    PODInterval(const T& low, const T& high, UserData&& data)
        : m_low(low)
        , m_high(high)
        , m_data(WTFMove(data))
        , m_maxHigh(high)
    {
    }

    const T& low() const { return m_low; }
    const T& high() const { return m_high; }
    const UserData& data() const { return m_data; }

    bool overlaps(const T& low, const T& high) const
    {
        if (this->high() < low)
            return false;
        if (high < this->low())
            return false;
        return true;
    }

    bool overlaps(const PODInterval& other) const
    {
        return overlaps(other.low(), other.high());
    }

    // Returns true if this interval is "less" than the other. The
    // comparison is performed on the low endpoints of the intervals.
    bool operator<(const PODInterval& other) const
    {
        return low() < other.low();
    }

    // Returns true if this interval is strictly equal to the other,
    // including comparison of the user data.
    bool operator==(const PODInterval& other) const
    {
        return (low() == other.low()
                && high() == other.high()
                && data() == other.data());
    }

    const T& maxHigh() const { return m_maxHigh; }
    void setMaxHigh(const T& maxHigh) { m_maxHigh = maxHigh; }

private:
    T m_low;
    T m_high;
    UserData m_data { };
    T m_maxHigh;
};

#ifndef NDEBUG

template<class T, class UserData>
TextStream& operator<<(TextStream& stream, const PODInterval<T, UserData>& interval)
{
    return stream << "[PODInterval (" << interval.low() << ", " << interval.high() << "), data=" << *interval.data() << ", maxHigh=" << interval.maxHigh() << ']';
}

#endif

} // namespace WebCore
