/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "GraphicsLayerAnimation.h"

#include <wtf/PointerComparison.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

GraphicsLayerAnimation::~GraphicsLayerAnimation() = default;

GraphicsLayerAnimation::GraphicsLayerAnimation()
    : m_timingFunction(CubicBezierTimingFunction::create())
{
}

GraphicsLayerAnimation::GraphicsLayerAnimation(const GraphicsLayerAnimation& o)
    : RefCounted<GraphicsLayerAnimation>()
    , m_delay(o.m_delay)
    , m_duration(o.m_duration)
    , m_iterationCount(o.m_iterationCount)
    , m_playbackRate(o.m_playbackRate)
    , m_timingFunction(o.m_timingFunction)
    , m_defaultTimingFunctionForKeyframes(o.m_defaultTimingFunctionForKeyframes)
    , m_compositeOperation(o.m_compositeOperation)
    , m_direction(o.m_direction)
    , m_fillMode(o.m_fillMode)
    , m_playState(o.m_playState)
{
}

bool GraphicsLayerAnimation::operator==(const GraphicsLayerAnimation& other) const
{
    return m_compositeOperation == other.m_compositeOperation
        && m_delay == other.m_delay
        && m_direction == other.m_direction
        && m_duration == other.m_duration
        && m_fillMode == other.m_fillMode
        && m_iterationCount == other.m_iterationCount
        && m_playState == other.m_playState
        && arePointingToEqualData(m_timingFunction.get(), other.m_timingFunction.get());
}

TextStream& operator<<(TextStream& ts, const GraphicsLayerAnimation& animation)
{
    ts.dumpProperty("delay"_s, animation.iterationCount());
    ts.dumpProperty("direction"_s, animation.direction());
    ts.dumpProperty("duration"_s, animation.duration());
    ts.dumpProperty("fill-mode"_s, animation.fillMode());
    ts.dumpProperty("iteration count"_s, animation.iterationCount());
    ts.dumpProperty("play-state"_s, animation.playState());
    if (animation.timingFunction())
        ts.dumpProperty("timing function"_s, *animation.timingFunction());

    return ts;
}

TextStream& operator<<(TextStream& ts, GraphicsLayerAnimation::Direction direction)
{
    switch (direction) {
    case GraphicsLayerAnimation::Direction::Normal: ts << "normal"_s; break;
    case GraphicsLayerAnimation::Direction::Alternate: ts << "alternate"_s; break;
    case GraphicsLayerAnimation::Direction::Reverse: ts << "reverse"_s; break;
    case GraphicsLayerAnimation::Direction::AlternateReverse: ts << "alternate-reverse"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, GraphicsLayerAnimation::FillMode fillMode)
{
    switch (fillMode) {
    case GraphicsLayerAnimation::FillMode::None: ts << "none"_s; break;
    case GraphicsLayerAnimation::FillMode::Forwards: ts << "forwards"_s; break;
    case GraphicsLayerAnimation::FillMode::Backwards: ts << "backwards"_s; break;
    case GraphicsLayerAnimation::FillMode::Both: ts << "both"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, GraphicsLayerAnimation::PlayState playState)
{
    switch (playState) {
    case GraphicsLayerAnimation::PlayState::Running: ts << "running"_s; break;
    case GraphicsLayerAnimation::PlayState::Paused: ts << "paused"_s; break;
    }
    return ts;
}

} // namespace WebCore
