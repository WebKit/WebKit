/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 *
 */

#include "config.h"
#include "StyleMarqueeData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MarqueeData);

MarqueeData::MarqueeData()
    : marqueeIncrement(ComputedStyle::initialMarqueeIncrement())
    , marqueeSpeed(ComputedStyle::initialMarqueeSpeed())
    , marqueeRepetition(ComputedStyle::initialMarqueeRepetition())
    , marqueeBehavior(static_cast<unsigned>(ComputedStyle::initialMarqueeBehavior()))
    , marqueeDirection(static_cast<unsigned>(ComputedStyle::initialMarqueeDirection()))
{
}

inline MarqueeData::MarqueeData(const MarqueeData& o)
    : RefCounted<MarqueeData>()
    , marqueeIncrement(o.marqueeIncrement)
    , marqueeSpeed(o.marqueeSpeed)
    , marqueeRepetition(o.marqueeRepetition)
    , marqueeBehavior(o.marqueeBehavior)
    , marqueeDirection(o.marqueeDirection)
{
}

Ref<MarqueeData> MarqueeData::copy() const
{
    return adoptRef(*new MarqueeData(*this));
}

bool MarqueeData::operator==(const MarqueeData& o) const
{
    return marqueeIncrement == o.marqueeIncrement
        && marqueeSpeed == o.marqueeSpeed
        && marqueeRepetition == o.marqueeRepetition
        && marqueeBehavior == o.marqueeBehavior
        && marqueeDirection == o.marqueeDirection;
}

#if !LOG_DISABLED
void MarqueeData::dumpDifferences(TextStream& ts, const MarqueeData& other) const
{
    LOG_IF_DIFFERENT(marqueeIncrement);
    LOG_IF_DIFFERENT(marqueeSpeed);
    LOG_IF_DIFFERENT(marqueeRepetition);
    LOG_IF_DIFFERENT_WITH_CAST(MarqueeBehavior, marqueeBehavior);
    LOG_IF_DIFFERENT_WITH_CAST(MarqueeDirection, marqueeDirection);
}
#endif // !LOG_DISABLED

} // namespace Style
} // namespace WebCore
