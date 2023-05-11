/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include "Animation.h"

#include "CommonAtomStrings.h"
#include "WebAnimationUtilities.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Animation::Animation()
    : m_name(initialName())
    , m_iterationCount(initialIterationCount())
    , m_delay(initialDelay())
    , m_duration(initialDuration())
    , m_timingFunction(initialTimingFunction())
    , m_direction(static_cast<unsigned>(initialDirection()))
    , m_fillMode(static_cast<unsigned>(initialFillMode()))
    , m_playState(static_cast<unsigned>(initialPlayState()))
    , m_compositeOperation(static_cast<unsigned>(initialCompositeOperation()))
{
}

Animation::Animation(const Animation& o)
    : RefCounted<Animation>()
    , m_property(o.m_property)
    , m_name(o.m_name)
    , m_iterationCount(o.m_iterationCount)
    , m_delay(o.m_delay)
    , m_duration(o.m_duration)
    , m_timingFunction(o.m_timingFunction)
    , m_nameStyleScopeOrdinal(o.m_nameStyleScopeOrdinal)
    , m_setProperties(o.m_setProperties)
    , m_filledProperties(o.m_filledProperties)
    , m_direction(o.m_direction)
    , m_fillMode(o.m_fillMode)
    , m_playState(o.m_playState)
    , m_compositeOperation(o.m_compositeOperation)
    , m_isNone(o.m_isNone)
{
}

Animation::~Animation() = default;

bool Animation::animationsMatch(const Animation& other, bool matchProperties) const
{
    bool result = m_name.string == other.m_name.string
        && m_playState == other.m_playState
        && m_compositeOperation == other.m_compositeOperation
        && m_iterationCount == other.m_iterationCount
        && m_delay == other.m_delay
        && m_duration == other.m_duration
        && *(m_timingFunction.get()) == *(other.m_timingFunction.get())
        && m_nameStyleScopeOrdinal == other.m_nameStyleScopeOrdinal
        && m_direction == other.m_direction
        && m_fillMode == other.m_fillMode
        && isDelaySet() == other.isDelaySet()
        && isDirectionSet() == other.isDirectionSet()
        && isDurationSet() == other.isDurationSet()
        && isFillModeSet() == other.isFillModeSet()
        && isIterationCountSet() == other.isIterationCountSet()
        && isNameSet() == other.isNameSet()
        && isTimingFunctionSet() == other.isTimingFunctionSet()
        && isCompositeOperationSet() == other.isCompositeOperationSet()
        && isPlayStateSet() == other.isPlayStateSet()
        && m_isNone == other.m_isNone;

    if (!result)
        return false;

    return !matchProperties || (m_property.mode == other.m_property.mode && m_property.animatableProperty == other.m_property.animatableProperty && isPropertySet() == other.isPropertySet());
}

auto Animation::initialName() -> const Name&
{
    static NeverDestroyed<Name> initialValue { Name { noneAtom(), true } };
    return initialValue;
}

TextStream& operator<<(TextStream& ts, Animation::TransitionProperty transitionProperty)
{
    switch (transitionProperty.mode) {
    case Animation::TransitionMode::All: ts << "all"; break;
    case Animation::TransitionMode::None: ts << "none"; break;
    case Animation::TransitionMode::SingleProperty: ts << animatablePropertyAsString(transitionProperty.animatableProperty); break;
    case Animation::TransitionMode::UnknownProperty: ts << "unknown property"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Animation::Direction direction)
{
    switch (direction) {
    case Animation::Direction::Normal: ts << "normal"; break;
    case Animation::Direction::Alternate: ts << "alternate"; break;
    case Animation::Direction::Reverse: ts << "reverse"; break;
    case Animation::Direction::AlternateReverse: ts << "alternate-reverse"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const Animation& animation)
{
    ts.dumpProperty("property", animation.property());
    ts.dumpProperty("name", animation.name().string);
    ts.dumpProperty("iteration count", animation.iterationCount());
    ts.dumpProperty("delay", animation.iterationCount());
    ts.dumpProperty("duration", animation.duration());
    if (animation.timingFunction())
        ts.dumpProperty("timing function", *animation.timingFunction());
    ts.dumpProperty("direction", animation.direction());
    ts.dumpProperty("fill-mode", animation.fillMode());
    ts.dumpProperty("play-state", animation.playState());

    return ts;
}

} // namespace WebCore
