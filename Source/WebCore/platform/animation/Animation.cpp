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
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Animation::Animation()
    : m_name(initialName())
    , m_iterationCount(initialIterationCount())
    , m_delay(initialDelay())
    , m_duration(initialDuration())
    , m_timingFunction(initialTimingFunction())
    , m_direction(initialDirection())
    , m_fillMode(static_cast<unsigned>(initialFillMode()))
    , m_playState(static_cast<unsigned>(initialPlayState()))
    , m_compositeOperation(static_cast<unsigned>(initialCompositeOperation()))
    , m_delaySet(false)
    , m_directionSet(false)
    , m_durationSet(false)
    , m_fillModeSet(false)
    , m_iterationCountSet(false)
    , m_nameSet(false)
    , m_playStateSet(false)
    , m_propertySet(false)
    , m_timingFunctionSet(false)
    , m_compositeOperationSet(false)
    , m_isNone(false)
    , m_delayFilled(false)
    , m_directionFilled(false)
    , m_durationFilled(false)
    , m_fillModeFilled(false)
    , m_iterationCountFilled(false)
    , m_playStateFilled(false)
    , m_propertyFilled(false)
    , m_timingFunctionFilled(false)
    , m_compositeOperationFilled(false)
{
}

Animation::Animation(const Animation& o)
    : RefCounted<Animation>()
    , m_property(o.m_property)
    , m_name(o.m_name)
    , m_customOrUnknownProperty(o.m_customOrUnknownProperty)
    , m_iterationCount(o.m_iterationCount)
    , m_delay(o.m_delay)
    , m_duration(o.m_duration)
    , m_timingFunction(o.m_timingFunction)
    , m_nameStyleScopeOrdinal(o.m_nameStyleScopeOrdinal)
    , m_direction(o.m_direction)
    , m_fillMode(o.m_fillMode)
    , m_playState(o.m_playState)
    , m_compositeOperation(o.m_compositeOperation)
    , m_delaySet(o.m_delaySet)
    , m_directionSet(o.m_directionSet)
    , m_durationSet(o.m_durationSet)
    , m_fillModeSet(o.m_fillModeSet)
    , m_iterationCountSet(o.m_iterationCountSet)
    , m_nameSet(o.m_nameSet)
    , m_playStateSet(o.m_playStateSet)
    , m_propertySet(o.m_propertySet)
    , m_timingFunctionSet(o.m_timingFunctionSet)
    , m_compositeOperationSet(o.m_compositeOperationSet)
    , m_isNone(o.m_isNone)
    , m_delayFilled(o.m_delayFilled)
    , m_directionFilled(o.m_directionFilled)
    , m_durationFilled(o.m_durationFilled)
    , m_fillModeFilled(o.m_fillModeFilled)
    , m_iterationCountFilled(o.m_iterationCountFilled)
    , m_playStateFilled(o.m_playStateFilled)
    , m_propertyFilled(o.m_propertyFilled)
    , m_timingFunctionFilled(o.m_timingFunctionFilled)
    , m_compositeOperationFilled(o.m_compositeOperationFilled)
{
}

Animation::~Animation() = default;

bool Animation::animationsMatch(const Animation& other, bool matchProperties) const
{
    bool result = m_name.string == other.m_name.string
        && m_playState == other.m_playState
        && m_compositeOperation == other.m_compositeOperation
        && m_playStateSet == other.m_playStateSet
        && m_customOrUnknownProperty == other.m_customOrUnknownProperty
        && m_iterationCount == other.m_iterationCount
        && m_delay == other.m_delay
        && m_duration == other.m_duration
        && *(m_timingFunction.get()) == *(other.m_timingFunction.get())
        && m_nameStyleScopeOrdinal == other.m_nameStyleScopeOrdinal
        && m_direction == other.m_direction
        && m_fillMode == other.m_fillMode
        && m_delaySet == other.m_delaySet
        && m_directionSet == other.m_directionSet
        && m_durationSet == other.m_durationSet
        && m_fillModeSet == other.m_fillModeSet
        && m_iterationCountSet == other.m_iterationCountSet
        && m_nameSet == other.m_nameSet
        && m_timingFunctionSet == other.m_timingFunctionSet
        && m_compositeOperationSet == other.m_compositeOperationSet
        && m_isNone == other.m_isNone;

    if (!result)
        return false;

    return !matchProperties || (m_property.mode == other.m_property.mode && m_property.id == other.m_property.id && m_propertySet == other.m_propertySet);
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
    case Animation::TransitionMode::SingleProperty: ts << nameLiteral(transitionProperty.id); break;
    case Animation::TransitionMode::CustomProperty: ts << "custom property"; break;
    case Animation::TransitionMode::UnknownProperty: ts << "unknown property"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Animation::AnimationDirection direction)
{
    switch (direction) {
    case Animation::AnimationDirectionNormal: ts << "normal"; break;
    case Animation::AnimationDirectionAlternate: ts << "alternate"; break;
    case Animation::AnimationDirectionReverse: ts << "reverse"; break;
    case Animation::AnimationDirectionAlternateReverse: ts << "alternate-reverse"; break;
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
