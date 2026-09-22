/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2026 Igalia S.L.
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
 */

#include "config.h"
#include "AcceleratedAnimation.h"

#if USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER)
#include "AnimationUtilities.h"
#include "GraphicsLayerAnimationValue.h"
#include "GraphicsLayerFilterAnimationValue.h"
#include "GraphicsLayerFloatAnimationValue.h"
#include "GraphicsLayerTransformAnimationValue.h"
#include "LayoutSize.h"
#include "TranslateTransformOperation.h"

namespace WebCore {

static FilterOperations applyFilterAnimation(const FilterOperations& from, const FilterOperations& to, double progress)
{
    if (!progress)
        return from;

    if (progress == 1)
        return to;

    if (!from.isEmpty() && !to.isEmpty() && !from.operationsMatch(to))
        return to;

    auto blendFunc = [](FilterOperation* fromOp, FilterOperation& toOp, double progress, bool blendToPassthrough) {
        return toOp.blend(fromOp, progress, blendToPassthrough);
    };

    size_t fromSize = from.size();
    size_t toSize = to.size();
    size_t size = std::max(fromSize, toSize);

    Vector<Ref<FilterOperation>> operations;
    operations.reserveInitialCapacity(size);

    for (size_t i = 0; i < size; i++) {
        RefPtr<FilterOperation> fromOp = i < fromSize ? from[i].ptr() : nullptr;
        RefPtr<FilterOperation> toOp = i < toSize ? to[i].ptr() : nullptr;
        RefPtr<FilterOperation> blendedOp = toOp ? blendFunc(fromOp.get(), *toOp, progress, false) : (fromOp ? blendFunc(nullptr, *fromOp, progress, true) : nullptr);
        if (blendedOp) {
            operations.append(blendedOp.releaseNonNull());
            continue;
        }

        if (progress > 0.5) {
            if (toOp)
                operations.append(toOp.releaseNonNull());
            else
                operations.append(PassthroughFilterOperation::create());
        } else {
            if (fromOp)
                operations.append(fromOp.releaseNonNull());
            else
                operations.append(PassthroughFilterOperation::create());
        }
    }

    return FilterOperations { WTF::move(operations) };
}

static float applyOpacityAnimation(float fromOpacity, float toOpacity, double progress)
{
    if (progress == 1.0)
        return toOpacity;

    if (!progress)
        return fromOpacity;

    return fromOpacity + progress * (toOpacity - fromOpacity);
}

static TransformationMatrix applyTransformAnimation(const TransformOperations& from, const TransformOperations& to, double progress)
{
    TransformationMatrix matrix;

    if (!progress) {
        from.apply(matrix);
        return matrix;
    }

    if (progress == 1) {
        to.apply(matrix);
        return matrix;
    }

    blend(from, to, progress).apply(matrix);
    return matrix;
}

static bool shouldReverseAnimationValue(GraphicsLayerAnimation::Direction direction, int loopCount)
{
    return (direction == GraphicsLayerAnimation::Direction::Alternate && loopCount & 1)
        || (direction == GraphicsLayerAnimation::Direction::AlternateReverse && !(loopCount & 1))
        || direction == GraphicsLayerAnimation::Direction::Reverse;
}

static double normalizedAnimationValue(double runningTime, double duration, GraphicsLayerAnimation::Direction direction, double iterationCount)
{
    if (!duration)
        return 0;

    const int loopCount = runningTime / duration;
    const double lastFullLoop = duration * double(loopCount);
    const double remainder = runningTime - lastFullLoop;
    // Ignore remainder when we've reached the end of animation.
    const double normalized = (loopCount == iterationCount) ? 1.0 : (remainder / duration);

    return shouldReverseAnimationValue(direction, loopCount) ? 1 - normalized : normalized;
}

static double normalizedAnimationValueForFillsForwards(double iterationCount, GraphicsLayerAnimation::Direction direction)
{
    if (direction == GraphicsLayerAnimation::Direction::Normal)
        return 1;
    if (direction == GraphicsLayerAnimation::Direction::Reverse)
        return 0;
    return shouldReverseAnimationValue(direction, iterationCount) ? 1 : 0;
}

AcceleratedAnimation::AcceleratedAnimation(const String& name, const GraphicsLayerKeyframeValueList& keyframes, const GraphicsLayerAnimation& animation)
    : m_name(name.isSafeToSendToAnotherThread() ? name : name.isolatedCopy())
    , m_keyframes(keyframes)
    , m_timingFunction(animation.timingFunction()->clone())
    , m_defaultTimingFunctionForKeyframes(animation.defaultTimingFunctionForKeyframes() ? animation.defaultTimingFunctionForKeyframes()->clone() : RefPtr<TimingFunction> { })
    , m_iterationCount(animation.iterationCount())
    , m_duration(animation.duration().value_or(0))
    , m_direction(animation.direction())
{
}

void AcceleratedAnimation::apply(ApplyResult& applyResult, const Playback& playback, MonotonicTime time) const
{
    auto elapsedTime = [&] {
        if (playback.state == State::Paused)
            return playback.pauseTime;
        return time - playback.startTime;
    }();

    // Even when the animation has stopped and doesn't fill forwards, we should calculate the last value to avoid a flash.
    double normalizedValue = normalizedAnimationValue(elapsedTime.seconds(), m_duration, m_direction, m_iterationCount);

    auto state = playback.state;
    if (m_iterationCount != GraphicsLayerAnimation::IterationCountInfinite && elapsedTime.seconds() >= m_duration * m_iterationCount)
        state = State::Stopped;

    if (state == State::Stopped)
        normalizedValue = normalizedAnimationValueForFillsForwards(m_iterationCount, m_direction);

    applyResult.hasRunningAnimations |= (state == State::Playing);

    if (!normalizedValue) {
        applyInternal(applyResult, m_keyframes.at(0), m_keyframes.at(1), 0);
        return;
    }

    if (normalizedValue == 1.0) {
        applyInternal(applyResult, m_keyframes.at(m_keyframes.size() - 2), m_keyframes.at(m_keyframes.size() - 1), 1);
        return;
    }

    if (m_keyframes.size() == 2) {
        if (!m_defaultTimingFunctionForKeyframes)
            normalizedValue = m_timingFunction->transformProgress(normalizedValue, m_duration);

        normalizedValue = timingFunctionForKeyframe(m_keyframes.at(0)).transformProgress(normalizedValue, m_duration);
        applyInternal(applyResult, m_keyframes.at(0), m_keyframes.at(1), normalizedValue);
        return;
    }

    if (!m_defaultTimingFunctionForKeyframes)
        normalizedValue = m_timingFunction->transformProgress(normalizedValue, m_duration);

    for (size_t i = 0; i < m_keyframes.size() - 1; ++i) {
        const auto& from = m_keyframes.at(i);
        const auto& to = m_keyframes.at(i + 1);
        if (from.keyTime() > normalizedValue || to.keyTime() < normalizedValue)
            continue;

        normalizedValue = (normalizedValue - from.keyTime()) / (to.keyTime() - from.keyTime());
        normalizedValue = timingFunctionForKeyframe(from).transformProgress(normalizedValue, m_duration);
        applyInternal(applyResult, from, to, normalizedValue);
        break;
    }
}

bool AcceleratedAnimation::isTransformAnimation() const
{
    switch (m_keyframes.property()) {
    case AnimatedProperty::Translate:
    case AnimatedProperty::Rotate:
    case AnimatedProperty::Scale:
    case AnimatedProperty::Transform:
        return true;
    default:
        break;
    }
    return false;
}

const TimingFunction& AcceleratedAnimation::timingFunctionForKeyframe(const GraphicsLayerAnimationValue& from) const
{
    if (auto* keyframeTimingFunction = from.timingFunction())
        return *keyframeTimingFunction;

    if (m_defaultTimingFunctionForKeyframes)
        return *m_defaultTimingFunctionForKeyframes;

    return LinearTimingFunction::identity();
}

void AcceleratedAnimation::applyInternal(ApplyResult& applyResult, const GraphicsLayerAnimationValue& from, const GraphicsLayerAnimationValue& to, float progress) const
{
    switch (m_keyframes.property()) {
    case AnimatedProperty::Translate:
    case AnimatedProperty::Rotate:
    case AnimatedProperty::Scale:
    case AnimatedProperty::Transform: {
        ASSERT(applyResult.transform);
        auto transform = applyTransformAnimation(static_cast<const GraphicsLayerTransformAnimationValue&>(from).value(), static_cast<const GraphicsLayerTransformAnimationValue&>(to).value(), progress);
        applyResult.transform->multiply(transform);
        return;
    }
    case AnimatedProperty::Opacity:
        applyResult.opacity = applyOpacityAnimation((static_cast<const GraphicsLayerFloatAnimationValue&>(from).value()), (static_cast<const GraphicsLayerFloatAnimationValue&>(to).value()), progress);
        return;
    case AnimatedProperty::Filter:
    case AnimatedProperty::WebkitBackdropFilter:
        applyResult.filters = applyFilterAnimation(static_cast<const GraphicsLayerFilterAnimationValue&>(from).value(), static_cast<const GraphicsLayerFilterAnimationValue&>(to).value(), progress);
        return;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER)
