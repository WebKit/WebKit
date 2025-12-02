/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "RenderStyleConstants.h"
#include "RenderStyleDifference.h"
#include "RenderStyleInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {

StyleMarqueeData::StyleMarqueeData()
    : marqueeIncrement(RenderStyle::initialMarqueeIncrement())
    , marqueeSpeed(RenderStyle::initialMarqueeSpeed())
    , marqueeRepetition(RenderStyle::initialMarqueeRepetition())
    , marqueeBehavior(static_cast<unsigned>(RenderStyle::initialMarqueeBehavior()))
    , marqueeDirection(static_cast<unsigned>(RenderStyle::initialMarqueeDirection()))
{
}

inline StyleMarqueeData::StyleMarqueeData(const StyleMarqueeData& o)
    : RefCounted<StyleMarqueeData>()
    , marqueeIncrement(o.marqueeIncrement)
    , marqueeSpeed(o.marqueeSpeed)
    , marqueeRepetition(o.marqueeRepetition)
    , marqueeBehavior(o.marqueeBehavior)
    , marqueeDirection(o.marqueeDirection)
{
}

Ref<StyleMarqueeData> StyleMarqueeData::create()
{
    return adoptRef(*new StyleMarqueeData);
}

Ref<StyleMarqueeData> StyleMarqueeData::copy() const
{
    return adoptRef(*new StyleMarqueeData(*this));
}

bool StyleMarqueeData::operator==(const StyleMarqueeData& o) const
{
    return marqueeIncrement == o.marqueeIncrement
        && marqueeSpeed == o.marqueeSpeed
        && marqueeRepetition == o.marqueeRepetition
        && marqueeBehavior == o.marqueeBehavior
        && marqueeDirection == o.marqueeDirection;
}

#if !LOG_DISABLED
void StyleMarqueeData::dumpDifferences(TextStream& ts, const StyleMarqueeData& other) const
{
    LOG_IF_DIFFERENT(marqueeIncrement);
    LOG_IF_DIFFERENT(marqueeSpeed);
    LOG_IF_DIFFERENT(marqueeRepetition);
    LOG_IF_DIFFERENT_WITH_CAST(MarqueeBehavior, marqueeBehavior);
    LOG_IF_DIFFERENT_WITH_CAST(MarqueeDirection, marqueeDirection);
}
#endif // !LOG_DISABLED

} // namespace WebCore
