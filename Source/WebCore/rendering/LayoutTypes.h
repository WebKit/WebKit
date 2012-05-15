/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// These typedefs are being used to abstract layout and hit testing off
// of integers and eventually replace them with floats. Once this transition
// is complete, these types will be removed. Progress can be tracked at
// https://bugs.webkit.org/show_bug.cgi?id=60318

#ifndef LayoutTypes_h
#define LayoutTypes_h

#include "FloatRect.h"
#include "IntRect.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

typedef int LayoutUnit;
typedef IntPoint LayoutPoint;
typedef IntSize LayoutSize;
typedef IntRect LayoutRect;

#define MAX_LAYOUT_UNIT INT_MAX
#define MIN_LAYOUT_UNIT INT_MIN
#define ZERO_LAYOUT_UNIT 0

inline LayoutRect enclosingLayoutRect(const FloatRect& rect)
{
    return enclosingIntRect(rect);
}

inline IntRect pixelSnappedIntRect(const LayoutRect& rect)
{
    return rect;
}

inline IntRect pixelSnappedIntRect(LayoutUnit left, LayoutUnit top, LayoutUnit width, LayoutUnit height)
{
    return IntRect(left, top, width, height);
}

inline IntRect pixelSnappedIntRect(const LayoutPoint& location, const LayoutSize& size)
{
    return IntRect(location, size);
}

inline IntRect pixelSnappedIntRectFromEdges(LayoutUnit left, LayoutUnit top, LayoutUnit right, LayoutUnit bottom)
{
    return IntRect(left, top, right - left, bottom - top);
}

inline int snapSizeToPixel(LayoutUnit size, LayoutUnit location) 
{
    UNUSED_PARAM(location);
    return size;
}

inline IntSize pixelSnappedIntSize(LayoutSize size, LayoutPoint location)
{
    UNUSED_PARAM(location);
    return size;
}

inline IntSize roundedIntSize(const LayoutSize& s)
{
    return s;
}

inline LayoutSize roundedLayoutSize(const FloatSize& s)
{
    return roundedIntSize(s);
}

inline IntPoint roundedIntPoint(const LayoutPoint& p)
{
    return p;
}

inline LayoutPoint roundedLayoutPoint(const FloatPoint& p)
{
    return roundedIntPoint(p);
}

inline LayoutPoint flooredLayoutPoint(const FloatPoint& p)
{
    return flooredIntPoint(p);
}

inline LayoutPoint flooredLayoutPoint(const FloatSize& s)
{
    return flooredIntPoint(s);
}

inline LayoutSize flooredLayoutSize(const FloatPoint& p)
{
    return LayoutSize(static_cast<int>(p.x()), static_cast<int>(p.y()));
}

inline int roundToInt(LayoutUnit value)
{
    return value;
}

inline int floorToInt(LayoutUnit value)
{
    return value;
}

inline LayoutUnit roundedLayoutUnit(float value)
{
    return lroundf(value);
}

inline LayoutUnit ceiledLayoutUnit(float value)
{
    return ceilf(value);
}

inline LayoutUnit absoluteValue(const LayoutUnit& value)
{
    return abs(value);
}

inline LayoutSize toLayoutSize(const LayoutPoint& p)
{
    return LayoutSize(p.x(), p.y());
}

inline LayoutPoint toLayoutPoint(const LayoutSize& p)
{
    return LayoutPoint(p.width(), p.height());
}

inline LayoutUnit layoutMod(const LayoutUnit& numerator, const LayoutUnit& denominator)
{
    return numerator % denominator;
}

inline LayoutUnit clampToLayoutUnit(double value)
{
    return clampToInteger(value);
}

inline bool isIntegerValue(const LayoutUnit)
{
    return true;
}

inline LayoutUnit boundedMultiply(const LayoutUnit& a, const LayoutUnit& b)
{
    return a * b;
}

} // namespace WebCore

#endif // LayoutTypes_h
