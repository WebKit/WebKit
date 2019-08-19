/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "SVGPathUtilities.h"
#include "SVGPropertyTraits.h"
#include <wtf/Vector.h>

namespace WebCore {

// Type definitions for the byte stream data
typedef union {
    bool value;
    unsigned char bytes[sizeof(bool)];
} BoolByte;

typedef union {
    float value;
    unsigned char bytes[sizeof(float)];
} FloatByte;

typedef union {
    unsigned short value;
    unsigned char bytes[sizeof(unsigned short)];
} UnsignedShortByte;

class SVGPathByteStream {
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef Vector<unsigned char> Data;
    typedef Data::const_iterator DataIterator;

    SVGPathByteStream() { }

    SVGPathByteStream(const String& string)
    {
        buildSVGPathByteStreamFromString(string, *this, UnalteredParsing);
    }

    SVGPathByteStream(const SVGPathByteStream& other)
    {
        *this = other;
    }

    SVGPathByteStream(SVGPathByteStream&& other)
    {
        *this = WTFMove(other);
    }

    SVGPathByteStream& operator=(const SVGPathByteStream& other)
    {
        if (*this == other)
            return *this;
        m_data = other.m_data;
        return *this;
    }

    SVGPathByteStream& operator=(SVGPathByteStream&& other)
    {
        if (*this == other)
            return *this;
        m_data = WTFMove(other.m_data);
        return *this;
    }

    bool operator==(const SVGPathByteStream& other) const { return m_data == other.m_data; }
    bool operator!=(const SVGPathByteStream& other) const { return !(*this == other); }

    std::unique_ptr<SVGPathByteStream> copy() const
    {
        return makeUnique<SVGPathByteStream>(*this);
    }

    DataIterator begin() const { return m_data.begin(); }
    DataIterator end() const { return m_data.end(); }

    void append(unsigned char byte) { m_data.append(byte); }
    void append(const SVGPathByteStream& other) { m_data.appendVector(other.m_data); }
    void clear() { m_data.clear(); }
    bool isEmpty() const { return m_data.isEmpty(); }
    unsigned size() const { return m_data.size(); }

private:
    Data m_data;
};

} // namespace WebCore
