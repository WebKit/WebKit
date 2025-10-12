/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#pragma once

#include <WebCore/CompositeOperation.h>
#include <WebCore/TimingFunction.h>
#include <optional>

namespace WebCore {

class GraphicsLayerAnimation : public RefCounted<GraphicsLayerAnimation> {
public:
    static constexpr double IterationCountInfinite = -1;
    enum class Direction : uint8_t { Normal, Alternate, Reverse, AlternateReverse };
    enum class FillMode : uint8_t { None, Forwards, Backwards, Both };
    enum class PlayState : bool { Running, Paused };

    static Ref<GraphicsLayerAnimation> create() { return adoptRef(*new GraphicsLayerAnimation); }
    static Ref<GraphicsLayerAnimation> create(const GraphicsLayerAnimation& other) { return adoptRef(*new GraphicsLayerAnimation(other)); }
    WEBCORE_EXPORT ~GraphicsLayerAnimation();

    CompositeOperation compositeOperation() const { return static_cast<CompositeOperation>(m_compositeOperation); }
    void setCompositeOperation(CompositeOperation compositeOperation) { m_compositeOperation = static_cast<unsigned>(compositeOperation); }

    double delay() const { return m_delay; }
    void setDelay(double delay) { m_delay = delay; }

    Direction direction() const { return static_cast<Direction>(m_direction); }
    void setDirection(Direction direction) { m_direction = static_cast<unsigned>(direction); }

    void setDuration(std::optional<double> duration) { ASSERT(!duration || *duration >= 0); m_duration = duration; }
    std::optional<double> duration() const { return m_duration; }

    FillMode fillMode() const { return static_cast<FillMode>(m_fillMode); }
    void setFillMode(FillMode fillMode) { m_fillMode = static_cast<unsigned>(fillMode); }

    double playbackRate() const { return m_playbackRate; }
    void setPlaybackRate(double playbackRate) { m_playbackRate = playbackRate; }

    double iterationCount() const { return m_iterationCount; }
    void setIterationCount(double iterationCount) { m_iterationCount = iterationCount; }

    PlayState playState() const { return static_cast<PlayState>(m_playState); }
    void setPlayState(PlayState playState) { m_playState = static_cast<unsigned>(playState); }

    RefPtr<TimingFunction> timingFunction() const { return m_timingFunction.get(); }
    void setTimingFunction(RefPtr<TimingFunction>&& function) { m_timingFunction = WTFMove(function); }

    RefPtr<TimingFunction> defaultTimingFunctionForKeyframes() const { return m_defaultTimingFunctionForKeyframes.get(); }
    void setDefaultTimingFunctionForKeyframes(RefPtr<TimingFunction>&& function) { m_defaultTimingFunctionForKeyframes = WTFMove(function); }

    bool isZeroDuration() const { return (!m_duration || !*m_duration) && m_delay <= 0; }

    bool fillsBackwards() const { return fillMode() == FillMode::Backwards || fillMode() == FillMode::Both; }
    bool fillsForwards() const { return fillMode() == FillMode::Forwards || fillMode() == FillMode::Both; }

    bool directionIsForwards() const { return direction() == Direction::Normal || direction() == Direction::Alternate; }

    bool operator==(const GraphicsLayerAnimation&) const;

private:
    WEBCORE_EXPORT GraphicsLayerAnimation();
    GraphicsLayerAnimation(const GraphicsLayerAnimation&);

    double m_delay { 0 };
    std::optional<double> m_duration { };
    double m_iterationCount { 1.0 };
    double m_playbackRate { 1 };
    RefPtr<TimingFunction> m_timingFunction;
    RefPtr<TimingFunction> m_defaultTimingFunctionForKeyframes;
    PREFERRED_TYPE(CompositeOperation) unsigned m_compositeOperation : 2 { static_cast<unsigned>(CompositeOperation::Replace) };
    PREFERRED_TYPE(Direction) unsigned m_direction : 2 { static_cast<unsigned>(Direction::Normal) };
    PREFERRED_TYPE(FillMode) unsigned m_fillMode : 2 { static_cast<unsigned>(FillMode::None) };
    PREFERRED_TYPE(PlayState) unsigned m_playState : 2 { static_cast<unsigned>(Direction::Normal) };
};

WTF::TextStream& operator<<(WTF::TextStream&, const GraphicsLayerAnimation&);
WTF::TextStream& operator<<(WTF::TextStream&, GraphicsLayerAnimation::Direction);
WTF::TextStream& operator<<(WTF::TextStream&, GraphicsLayerAnimation::FillMode);
WTF::TextStream& operator<<(WTF::TextStream&, GraphicsLayerAnimation::PlayState);

} // namespace WebCore
