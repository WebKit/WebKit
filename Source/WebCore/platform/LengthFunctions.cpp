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

int intValueForLength(const Length& length, LayoutUnit maximumValue, float zoom)
{
    return static_cast<int>(valueForLength(length, maximumValue, zoom));
}

LayoutUnit valueForLength(const Length& length, LayoutUnit maximumValue, float zoom)
{
    return valueForLengthWithLazyMaximum<LayoutUnit, LayoutUnit>(length, [&] ALWAYS_INLINE_LAMBDA {
        return maximumValue;
    }, zoom);
}

float floatValueForLength(const Length& length, float maximumValue, float zoom)
{
    return valueForLengthWithLazyMaximum<float, float>(length, [&] ALWAYS_INLINE_LAMBDA {
        return maximumValue;
    }, zoom);
}

LayoutSize sizeForLengthSize(const LengthSize& length, const LayoutSize& maximumValue, float zoom)
{
    return { valueForLength(length.width, maximumValue.width(), zoom), valueForLength(length.height, maximumValue.height(), zoom) };
}

LayoutPoint pointForLengthPoint(const LengthPoint& lengthPoint, const LayoutSize& maximumValue, float zoom)
{
    return { valueForLength(lengthPoint.x, maximumValue.width(), zoom), valueForLength(lengthPoint.y, maximumValue.height(), zoom) };
}

FloatSize floatSizeForLengthSize(const LengthSize& lengthSize, const FloatSize& boxSize, float zoom)
{
    return { floatValueForLength(lengthSize.width, boxSize.width(), zoom), floatValueForLength(lengthSize.height, boxSize.height(), zoom) };
}

FloatPoint floatPointForLengthPoint(const LengthPoint& lengthPoint, const FloatSize& boxSize, float zoom)
{
    return { floatValueForLength(lengthPoint.x, boxSize.width(), zoom), floatValueForLength(lengthPoint.y, boxSize.height(), zoom) };
}

} // namespace WebCore
