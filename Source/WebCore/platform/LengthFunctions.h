/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/LayoutPoint.h>
#include <WebCore/LayoutUnit.h>
#include <WebCore/Length.h>

namespace WebCore {

class FloatSize;
class FloatPoint;
class LayoutSize;

struct Length;
struct LengthSize;
struct LengthPoint;

int intValueForLength(const Length&, LayoutUnit maximumValue, float zoom);
WEBCORE_EXPORT float floatValueForLength(const Length&, float maximumValue, float zoom);
WEBCORE_EXPORT LayoutUnit valueForLength(const Length&, LayoutUnit maximumValue, float zoom);

LayoutSize sizeForLengthSize(const LengthSize&, const LayoutSize& maximumValue, float zoom);
FloatSize floatSizeForLengthSize(const LengthSize&, const FloatSize& maximumValue, float zoom);

LayoutPoint pointForLengthPoint(const LengthPoint&, const LayoutSize& maximumValue, float zoom);
FloatPoint floatPointForLengthPoint(const LengthPoint&, const FloatSize& maximumValue, float zoom);

template<typename ReturnType, typename MaximumType>
ReturnType minimumValueForLengthWithLazyMaximum(const Length& length, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, float zoom)
{
    switch (length.type()) {
    case LengthType::Fixed:
        return ReturnType(length.value() * zoom);
    case LengthType::Percent:
        return ReturnType(static_cast<float>(lazyMaximumValueFunctor() * length.percent() / 100.0f));
    case LengthType::Calculated:
        return ReturnType(length.nonNanCalculatedValue(lazyMaximumValueFunctor()));
    case LengthType::FillAvailable:
    case LengthType::Auto:
    case LengthType::Normal:
    case LengthType::Content:
        return ReturnType(0);
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FitContent:
    case LengthType::Undefined:
        break;
    }
    ASSERT_NOT_REACHED();
    return ReturnType(0);
}

template<typename ReturnType, typename MaximumType>
ReturnType valueForLengthWithLazyMaximum(const Length& length, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, float zoom)
{
    switch (length.type()) {
    case LengthType::Fixed:
        return ReturnType(length.value() * zoom);
    case LengthType::Percent:
        return ReturnType(static_cast<float>(lazyMaximumValueFunctor() * length.percent() / 100.0f));
    case LengthType::Calculated:
        return ReturnType(length.nonNanCalculatedValue(lazyMaximumValueFunctor()));
    case LengthType::FillAvailable:
    case LengthType::Auto:
    case LengthType::Normal:
        return ReturnType(lazyMaximumValueFunctor());
    case LengthType::Content:
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FitContent:
    case LengthType::Undefined:
        break;
    }
    ASSERT_NOT_REACHED();
    return ReturnType(0);
}

inline float floatValueForLengthWithLazyLayoutUnitMaximum(const Length& length, NOESCAPE const Invocable<LayoutUnit()> auto& lazyMaximumValueFunctor, float zoom)
{
    return valueForLengthWithLazyMaximum<float, LayoutUnit>(length, lazyMaximumValueFunctor, zoom);
}

inline float floatValueForLengthWithLazyFloatMaximum(const Length& length, NOESCAPE const Invocable<float()> auto& lazyMaximumValueFunctor, float zoom)
{
    return valueForLengthWithLazyMaximum<float, float>(length, lazyMaximumValueFunctor, zoom);
}

inline LayoutUnit minimumValueForLength(const Length& length, LayoutUnit maximumValue, float zoom)
{
    return minimumValueForLengthWithLazyMaximum<LayoutUnit, LayoutUnit>(length, [&] ALWAYS_INLINE_LAMBDA {
        return maximumValue;
    }, zoom);
}

inline int minimumIntValueForLength(const Length& length, LayoutUnit maximumValue, float zoom)
{
    return minimumValueForLengthWithLazyMaximum<int, LayoutUnit>(length, [&] ALWAYS_INLINE_LAMBDA {
        return maximumValue;
    }, zoom);
}

inline LayoutUnit valueForLength(const Length& length, auto maximumValue, float zoom)
{
    return valueForLength(length, LayoutUnit(maximumValue), zoom);
}

inline LayoutUnit minimumValueForLength(const Length& length, auto maximumValue, float zoom)
{
    return minimumValueForLength(length, LayoutUnit(maximumValue), zoom);
}

} // namespace WebCore
