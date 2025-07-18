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
    , m_timeline(initialTimeline())
    , m_timingFunction(initialTimingFunction())
    , m_range(initialRange())
    , m_direction(static_cast<unsigned>(initialDirection()))
    , m_fillMode(static_cast<unsigned>(initialFillMode()))
    , m_playState(static_cast<unsigned>(initialPlayState()))
    , m_compositeOperation(static_cast<unsigned>(initialCompositeOperation()))
    , m_allowsDiscreteTransitions(initialAllowsDiscreteTransitions())
    , m_delaySet(false)
    , m_directionSet(false)
    , m_durationSet(false)
    , m_fillModeSet(false)
    , m_iterationCountSet(false)
    , m_nameSet(false)
    , m_playStateSet(false)
    , m_propertySet(false)
    , m_timelineSet(false)
    , m_timingFunctionSet(false)
    , m_compositeOperationSet(false)
    , m_allowsDiscreteTransitionsSet(false)
    , m_rangeStartSet(false)
    , m_rangeEndSet(false)
    , m_delayFilled(false)
    , m_directionFilled(false)
    , m_durationFilled(false)
    , m_fillModeFilled(false)
    , m_iterationCountFilled(false)
    , m_playStateFilled(false)
    , m_propertyFilled(false)
    , m_timelineFilled(false)
    , m_timingFunctionFilled(false)
    , m_compositeOperationFilled(false)
    , m_allowsDiscreteTransitionsFilled(false)
    , m_rangeStartFilled(false)
    , m_rangeEndFilled(false)
{
}

Animation::Animation(const Animation& o)
    : RefCounted<Animation>()
    , m_property(o.m_property)
    , m_name(o.m_name)
    , m_iterationCount(o.m_iterationCount)
    , m_delay(o.m_delay)
    , m_duration(o.m_duration)
    , m_timeline(o.m_timeline)
    , m_timingFunction(o.m_timingFunction)
    , m_range(o.m_range)
    , m_direction(o.m_direction)
    , m_fillMode(o.m_fillMode)
    , m_playState(o.m_playState)
    , m_compositeOperation(o.m_compositeOperation)
    , m_allowsDiscreteTransitions(o.m_allowsDiscreteTransitions)
    , m_delaySet(o.m_delaySet)
    , m_directionSet(o.m_directionSet)
    , m_durationSet(o.m_durationSet)
    , m_fillModeSet(o.m_fillModeSet)
    , m_iterationCountSet(o.m_iterationCountSet)
    , m_nameSet(o.m_nameSet)
    , m_playStateSet(o.m_playStateSet)
    , m_propertySet(o.m_propertySet)
    , m_timelineSet(o.m_timelineSet)
    , m_timingFunctionSet(o.m_timingFunctionSet)
    , m_compositeOperationSet(o.m_compositeOperationSet)
    , m_allowsDiscreteTransitionsSet(o.m_allowsDiscreteTransitionsSet)
    , m_rangeStartSet(o.m_rangeStartSet)
    , m_rangeEndSet(o.m_rangeEndSet)
    , m_delayFilled(o.m_delayFilled)
    , m_directionFilled(o.m_directionFilled)
    , m_durationFilled(o.m_durationFilled)
    , m_fillModeFilled(o.m_fillModeFilled)
    , m_iterationCountFilled(o.m_iterationCountFilled)
    , m_playStateFilled(o.m_playStateFilled)
    , m_propertyFilled(o.m_propertyFilled)
    , m_timelineFilled(o.m_timelineFilled)
    , m_timingFunctionFilled(o.m_timingFunctionFilled)
    , m_compositeOperationFilled(o.m_compositeOperationFilled)
    , m_allowsDiscreteTransitionsFilled(o.m_allowsDiscreteTransitionsFilled)
    , m_rangeStartFilled(o.m_rangeStartFilled)
    , m_rangeEndFilled(o.m_rangeEndFilled)
{
}

Animation::~Animation() = default;

bool Animation::animationsMatch(const Animation& other, bool matchProperties) const
{
    bool result = m_name == other.m_name
        && m_playState == other.m_playState
        && m_compositeOperation == other.m_compositeOperation
        && m_allowsDiscreteTransitions == other.m_allowsDiscreteTransitions
        && m_playStateSet == other.m_playStateSet
        && m_iterationCount == other.m_iterationCount
        && m_delay == other.m_delay
        && m_duration == other.m_duration
        && m_timeline == other.m_timeline
        && *(m_timingFunction.get()) == *(other.m_timingFunction.get())
        && m_direction == other.m_direction
        && m_fillMode == other.m_fillMode
        && m_range == other.m_range
        && m_delaySet == other.m_delaySet
        && m_directionSet == other.m_directionSet
        && m_durationSet == other.m_durationSet
        && m_fillModeSet == other.m_fillModeSet
        && m_iterationCountSet == other.m_iterationCountSet
        && m_nameSet == other.m_nameSet
        && m_timelineSet == other.m_timelineSet
        && m_timingFunctionSet == other.m_timingFunctionSet
        && m_compositeOperationSet == other.m_compositeOperationSet
        && m_allowsDiscreteTransitionsSet == other.m_allowsDiscreteTransitionsSet
        && m_rangeStartSet == other.m_rangeStartSet
        && m_rangeEndSet == other.m_rangeEndSet;

    if (!result)
        return false;

    return !matchProperties || (m_property == other.m_property && m_propertySet == other.m_propertySet);
}

auto Animation::initialName() -> const Style::ScopedName&
{
    static NeverDestroyed<Style::ScopedName> initialValue { Style::ScopedName { noneAtom() } };
    return initialValue;
}

TextStream& operator<<(TextStream& ts, Animation::TransitionProperty transitionProperty)
{
    switch (transitionProperty.mode) {
    case Animation::TransitionMode::All: ts << "all"_s; break;
    case Animation::TransitionMode::None: ts << "none"_s; break;
    case Animation::TransitionMode::SingleProperty: ts << animatablePropertyAsString(transitionProperty.animatableProperty); break;
    case Animation::TransitionMode::UnknownProperty: ts << "unknown property"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Animation::Direction direction)
{
    switch (direction) {
    case Animation::Direction::Normal: ts << "normal"_s; break;
    case Animation::Direction::Alternate: ts << "alternate"_s; break;
    case Animation::Direction::Reverse: ts << "reverse"_s; break;
    case Animation::Direction::AlternateReverse: ts << "alternate-reverse"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Animation::TimelineKeyword keyword)
{
    switch (keyword) {
    case Animation::TimelineKeyword::Auto: ts << "auto"_s; break;
    case Animation::TimelineKeyword::None: ts << "none"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const Animation::AnonymousScrollTimeline& timeline)
{
    auto hasScroller = timeline.scroller != Scroller::Nearest;
    auto hasAxis = timeline.axis != ScrollAxis::Block;

    ts << "scroll("_s;
    if (hasScroller)
        ts << (timeline.scroller == Scroller::Root ? "root" : "self");
    if (hasScroller && hasAxis)
        ts << ' ';
    if (hasAxis)
        ts << timeline.axis;
    ts << ')';

    return ts;
}

TextStream& operator<<(TextStream& ts, const Animation::AnonymousViewTimeline& timeline)
{
    auto hasAxis = timeline.axis != ScrollAxis::Block;
    auto& insets = timeline.insets;
    auto hasEndInset = insets.end && insets.end != insets.start;
    auto hasStartInset = (insets.start && !insets.start->isAuto()) || (insets.start && insets.start->isAuto() && hasEndInset);

    ts << "view("_s;
    if (hasAxis)
        ts << timeline.axis;
    if (hasAxis && hasStartInset)
        ts << ' ';
    if (hasStartInset)
        ts << *insets.start;
    if (hasStartInset && hasEndInset)
        ts << ' ';
    if (hasEndInset)
        ts << *insets.end;
    ts << ')';

    return ts;
}

TextStream& operator<<(TextStream& ts, const Animation::Timeline& timeline)
{
    WTF::switchOn(timeline,
        [&](Animation::TimelineKeyword keyword) {
            ts << keyword;
        }, [&](const AtomString& customIdent) {
            ts << customIdent;
        }, [&] (const Animation::AnonymousScrollTimeline& anonymousScrollTimeline) {
            ts << anonymousScrollTimeline;
        }, [&] (const Animation::AnonymousViewTimeline& anonymousViewTimeline) {
            ts << anonymousViewTimeline;
        }
    );
    return ts;
}

TextStream& operator<<(TextStream& ts, const Animation& animation)
{
    ts.dumpProperty("property"_s, animation.property());
    ts.dumpProperty("name"_s, animation.name().name);
    ts.dumpProperty("iteration count"_s, animation.iterationCount());
    ts.dumpProperty("delay"_s, animation.iterationCount());
    ts.dumpProperty("duration"_s, animation.duration());
    ts.dumpProperty("timeline"_s, animation.timeline());
    if (animation.timingFunction())
        ts.dumpProperty("timing function"_s, *animation.timingFunction());
    ts.dumpProperty("direction"_s, animation.direction());
    ts.dumpProperty("fill-mode"_s, animation.fillMode());
    ts.dumpProperty("play-state"_s, animation.playState());

    return ts;
}

} // namespace WebCore
