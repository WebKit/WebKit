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
#include "AnimationList.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

#define FILL_UNSET_PROPERTY(test, propGet, propSet) \
for (i = 0; i < size() && animation(i).test(); ++i) { } \
if (i) { \
    for (size_t j = 0; i < size(); ++i, ++j) \
        animation(i).propSet(animation(j).propGet()); \
}

AnimationList::AnimationList() = default;

AnimationList::AnimationList(const AnimationList& other, CopyBehavior copyBehavior)
{
    if (copyBehavior == CopyBehavior::Reference)
        m_animations = other.m_animations;
    else {
        m_animations = other.m_animations.map([](auto& animation) {
            return Animation::create(animation.get());
        });
    }
}

void AnimationList::fillUnsetProperties()
{
    size_t i;
    FILL_UNSET_PROPERTY(isDelaySet, delay, fillDelay);
    FILL_UNSET_PROPERTY(isDirectionSet, direction, fillDirection);
    FILL_UNSET_PROPERTY(isDurationSet, duration, fillDuration);
    FILL_UNSET_PROPERTY(isFillModeSet, fillMode, fillFillMode);
    FILL_UNSET_PROPERTY(isIterationCountSet, iterationCount, fillIterationCount);
    FILL_UNSET_PROPERTY(isPlayStateSet, playState, fillPlayState);
    FILL_UNSET_PROPERTY(isTimelineSet, timeline, fillTimeline);
    FILL_UNSET_PROPERTY(isTimingFunctionSet, timingFunction, fillTimingFunction);
    FILL_UNSET_PROPERTY(isPropertySet, property, fillProperty);
    FILL_UNSET_PROPERTY(isCompositeOperationSet, compositeOperation, fillCompositeOperation);
    FILL_UNSET_PROPERTY(isAllowsDiscreteTransitionsSet, allowsDiscreteTransitions, fillAllowsDiscreteTransitions);
}

bool AnimationList::operator==(const AnimationList& other) const
{
    if (size() != other.size())
        return false;
    for (size_t i = 0; i < size(); ++i) {
        if (animation(i) != other.animation(i))
            return false;
    }
    return true;
}

TextStream& operator<<(TextStream& ts, const AnimationList& animationList)
{
    ts << "[";
    for (size_t i = 0; i < animationList.size(); ++i) {
        if (i > 0)
            ts << ", ";
        ts << animationList.animation(i);
    }
    ts << "]";
    return ts;
}

} // namespace WebCore
