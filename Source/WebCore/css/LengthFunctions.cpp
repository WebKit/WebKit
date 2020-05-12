/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
    Copyright (C) 2011 Rik Cabanier (cabanier@adobe.com)
    Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
    Copyright (C) 2012 Motorola Mobility, Inc. All rights reserved.

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

#include "config.h"
#include "LengthFunctions.h"

#include "FloatSize.h"
#include "LayoutSize.h"
#include "LengthPoint.h"
#include "LengthSize.h"

namespace WebCore {

int intValueForLength(const Length& length, LayoutUnit maximumValue)
{
    return static_cast<int>(valueForLength(length, maximumValue));
}

LayoutUnit valueForLength(const Length& length, LayoutUnit maximumValue)
{
    switch (length.type()) {
    case Fixed:
    case Percent:
    case Calculated:
        return minimumValueForLength(length, maximumValue);
    case FillAvailable:
    case Auto:
        return maximumValue;
    case Relative:
    case Intrinsic:
    case MinIntrinsic:
    case MinContent:
    case MaxContent:
    case FitContent:
    case Undefined:
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

// FIXME: when subpixel layout is supported this copy of floatValueForLength() can be removed. See bug 71143.
float floatValueForLength(const Length& length, LayoutUnit maximumValue)
{
    switch (length.type()) {
    case Fixed:
        return length.value();
    case Percent:
        return static_cast<float>(maximumValue * length.percent() / 100.0f);
    case FillAvailable:
    case Auto:
        return static_cast<float>(maximumValue);
    case Calculated:
        return length.nonNanCalculatedValue(maximumValue);
    case Relative:
    case Intrinsic:
    case MinIntrinsic:
    case MinContent:
    case MaxContent:
    case FitContent:
    case Undefined:
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

float floatValueForLength(const Length& length, float maximumValue)
{
    switch (length.type()) {
    case Fixed:
        return length.value();
    case Percent:
        return static_cast<float>(maximumValue * length.percent() / 100.0f);
    case FillAvailable:
    case Auto:
        return static_cast<float>(maximumValue);
    case Calculated:
        return length.nonNanCalculatedValue(maximumValue);
    case Relative:
    case Intrinsic:
    case MinIntrinsic:
    case MinContent:
    case MaxContent:
    case FitContent:
    case Undefined:
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

LayoutSize sizeForLengthSize(const LengthSize& length, const LayoutSize& maximumValue)
{
    return { valueForLength(length.width, maximumValue.width()), valueForLength(length.height, maximumValue.height()) };
}

LayoutPoint pointForLengthPoint(const LengthPoint& lengthPoint, const LayoutSize& maximumValue)
{
    return { valueForLength(lengthPoint.x(), maximumValue.width()), valueForLength(lengthPoint.y(), maximumValue.height()) };
}

FloatSize floatSizeForLengthSize(const LengthSize& lengthSize, const FloatSize& boxSize)
{
    return { floatValueForLength(lengthSize.width, boxSize.width()), floatValueForLength(lengthSize.height, boxSize.height()) };
}

FloatPoint floatPointForLengthPoint(const LengthPoint& lengthPoint, const FloatSize& boxSize)
{
    return { floatValueForLength(lengthPoint.x(), boxSize.width()), floatValueForLength(lengthPoint.y(), boxSize.height()) };
}

} // namespace WebCore
