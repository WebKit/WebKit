/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
#include "TextureMapperAnimation.h"

#if USE(TEXTURE_MAPPER)

#include "LayoutSize.h"
#include <algorithm>
#include <wtf/Scope.h>

namespace WebCore {

static RefPtr<FilterOperation> blendFunc(FilterOperation* fromOp, FilterOperation& toOp, double progress, bool blendToPassthrough = false)
{
    return toOp.blend(fromOp, progress, blendToPassthrough);
}

static FilterOperations applyFilterAnimation(const FilterOperations& from, const FilterOperations& to, double progress)
{
    // First frame of an animation.
    if (!progress)
        return from;

    // Last frame of an animation.
    if (progress == 1)
        return to;

    if (!from.isEmpty() && !to.isEmpty() && !from.operationsMatch(to))
        return to;

    size_t fromSize = from.size();
    size_t toSize = to.size();
    size_t size = std::max(fromSize, toSize);

    Vector<Ref<FilterOperation>> operations;
    operations.reserveInitialCapacity(size);

    for (size_t i = 0; i < size; i++) {
        RefPtr<FilterOperation> fromOp = (i < fromSize) ? from[i].ptr() : nullptr;
        RefPtr<FilterOperation> toOp = (i < toSize) ? to[i].ptr() : nullptr;
        RefPtr<FilterOperation> blendedOp = toOp ? blendFunc(fromOp.get(), *toOp, progress) : (fromOp ? blendFunc(nullptr, *fromOp, progress, true) : nullptr);
        if (blendedOp)
            operations.append(blendedOp.releaseNonNull());
        else {
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
    }

    return FilterOperations { WTFMove(operations) };
}

static bool shouldReverseAnimationValue(Animation::Direction direction, int loopCount)
{
    return (direction == Animation::Direction::Alternate && loopCount & 1)
        || (direction == Animation::Direction::AlternateReverse && !(loopCount & 1))
        || direction == Animation::Direction::Reverse;
}

static double normalizedAnimationValue(double runningTime, double duration, Animation::Direction direction, double iterationCount)
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

static double normalizedAnimationValueForFillsForwards(double iterationCount, Animation::Direction direction)
{
    if (direction == Animation::Direction::Normal)
        return 1;
    if (direction == Animation::Direction::Reverse)
        return 0;
    return shouldReverseAnimationValue(direction, iterationCount) ? 1 : 0;
}

static float applyOpacityAnimation(float fromOpacity, float toOpacity, double progress)
{
    // Optimization: special case the edge values (0 and 1).
    if (progress == 1.0)
        return toOpacity;

    if (!progress)
        return fromOpacity;

    return fromOpacity + progress * (toOpacity - fromOpacity);
}

static TransformationMatrix applyTransformAnimation(const TransformList& from, const TransformList& to, double progress)
{
    TransformationMatrix matrix;

    // First frame of an animation.
    if (!progress) {
        from.apply(matrix);
        return matrix;
    }

    // Last frame of an animation.
    if (progress == 1) {
        to.apply(matrix);
        return matrix;
    }

    blend(from, to, progress).apply(matrix);

    return matrix;
}

KeyframeValueList<FloatAnimationValue> TextureMapperAnimationBase::createThreadsafeKeyFrames(const KeyframeValueList<FloatAnimationValue>& originalKeyframes)
{
    return originalKeyframes;
}

KeyframeValueList<FilterAnimationValue> TextureMapperAnimationBase::createThreadsafeKeyFrames(const KeyframeValueList<FilterAnimationValue>& originalKeyframes)
{
    return originalKeyframes;
}

KeyframeValueList<TransformAnimationValue> TextureMapperAnimationBase::createThreadsafeKeyFrames(const KeyframeValueList<TransformAnimationValue>& originalKeyframes)
{
    // FIXME: This copy is likely no longer needed now that transforms no longer use Length internally.

    KeyframeValueList<TransformAnimationValue> newKeyframes(originalKeyframes.property());

    for (const auto& originalKeyframe : originalKeyframes) {
        auto newTransformFunctions = originalKeyframe.value().list.map([&](const auto& function) -> TransformFunction {
            return WTF::switchOn(function,
                [](const auto& function) -> TransformFunction {
                    return function;
                }
            );
        });

        newKeyframes.insert(
            TransformAnimationValue(
                originalKeyframe.keyTime(),
                TransformList { WTFMove(newTransformFunctions) },
                originalKeyframe.timingFunction() ? RefPtr<TimingFunction> { originalKeyframe.timingFunction()->clone() } : nullptr
            )
        );
    }

    return newKeyframes;
}

TextureMapperAnimationBase::TextureMapperAnimationBase(const String& name, const Animation& animation, MonotonicTime startTime, Seconds pauseTime, State state)
    : m_name(name.isSafeToSendToAnotherThread() ? name : name.isolatedCopy())
    , m_timingFunction(animation.timingFunction()->clone())
    , m_iterationCount(animation.iterationCount())
    , m_duration(animation.duration().value_or(0))
    , m_direction(animation.direction())
    , m_fillsForwards(animation.fillsForwards())
    , m_startTime(startTime)
    , m_pauseTime(pauseTime)
    , m_totalRunningTime(0_s)
    , m_lastRefreshedTime(m_startTime)
    , m_state(state)
{
}

TextureMapperAnimationBase::TextureMapperAnimationBase(const TextureMapperAnimationBase& other)
    : m_name(other.m_name.isSafeToSendToAnotherThread() ? other.m_name : other.m_name.isolatedCopy())
    , m_timingFunction(other.m_timingFunction->clone())
    , m_iterationCount(other.m_iterationCount)
    , m_duration(other.m_duration)
    , m_direction(other.m_direction)
    , m_fillsForwards(other.m_fillsForwards)
    , m_startTime(other.m_startTime)
    , m_pauseTime(other.m_pauseTime)
    , m_totalRunningTime(other.m_totalRunningTime)
    , m_lastRefreshedTime(other.m_lastRefreshedTime)
    , m_state(other.m_state)
{
}

TextureMapperAnimationBase& TextureMapperAnimationBase::operator=(const TextureMapperAnimationBase& other)
{
    m_name = other.m_name.isSafeToSendToAnotherThread() ? other.m_name : other.m_name.isolatedCopy();
    m_timingFunction = other.m_timingFunction->clone();
    m_iterationCount = other.m_iterationCount;
    m_duration = other.m_duration;
    m_direction = other.m_direction;
    m_fillsForwards = other.m_fillsForwards;
    m_startTime = other.m_startTime;
    m_pauseTime = other.m_pauseTime;
    m_totalRunningTime = other.m_totalRunningTime;
    m_lastRefreshedTime = other.m_lastRefreshedTime;
    m_state = other.m_state;
    return *this;
}

double TextureMapperAnimationBase::applyBase(ApplicationResult& applicationResults, MonotonicTime time, KeepInternalState keepInternalState)
{
    MonotonicTime oldLastRefreshedTime = m_lastRefreshedTime;
    Seconds oldTotalRunningTime = m_totalRunningTime;
    State oldState = m_state;

    auto maybeRestoreInternalState = makeScopeExit([&] {
        if (keepInternalState == KeepInternalState::Yes) {
            m_lastRefreshedTime = oldLastRefreshedTime;
            m_totalRunningTime = oldTotalRunningTime;
            m_state = oldState;
        }
    });

    // Even when m_state == State::Stopped && !m_fillsForwards, we should calculate the last value to avoid a flash.
    // CoordinatedGraphicsScene will soon remove the stopped animation and update the value instead of this function.

    Seconds totalRunningTime = computeTotalRunningTime(time);
    double normalizedValue = normalizedAnimationValue(totalRunningTime.seconds(), m_duration, m_direction, m_iterationCount);

    if (m_iterationCount != Animation::IterationCountInfinite && totalRunningTime.seconds() >= m_duration * m_iterationCount) {
        m_state = State::Stopped;
        m_pauseTime = 0_s;
        normalizedValue = normalizedAnimationValueForFillsForwards(m_iterationCount, m_direction);
    }

    applicationResults.hasRunningAnimations |= (m_state == State::Playing);

    return normalizedValue;
}

void TextureMapperAnimationBase::applyInternal(ApplicationResult& applicationResults, const FloatAnimationValue& from, const FloatAnimationValue& to, float progress)
{
    applicationResults.opacity = applyOpacityAnimation(from.value(), to.value(), progress);
}

void TextureMapperAnimationBase::applyInternal(ApplicationResult& applicationResults, const FilterAnimationValue& from, const FilterAnimationValue& to, float progress)
{
    applicationResults.filters = applyFilterAnimation(from.value(), to.value(), progress);
}

void TextureMapperAnimationBase::applyInternal(ApplicationResult& applicationResults, const TransformAnimationValue& from, const TransformAnimationValue& to, float progress)
{
    ASSERT(applicationResults.transform);
    auto transform = applyTransformAnimation(from.value(), to.value(), progress);
    applicationResults.transform->multiply(transform);
}

void TextureMapperAnimationBase::pause(Seconds time)
{
    m_state = State::Paused;
    m_pauseTime = time;
}

void TextureMapperAnimationBase::resume()
{
    m_state = State::Playing;
    // FIXME: This seems wrong. m_totalRunningTime is cleared.
    // https://bugs.webkit.org/show_bug.cgi?id=183113
    m_pauseTime = 0_s;
    m_totalRunningTime = m_pauseTime;
    m_lastRefreshedTime = MonotonicTime::now();
}

RefPtr<const TimingFunction> TextureMapperAnimationBase::timingFunctionForAnimationValue(const AnimationValueBase& animationValue)
{
    if (RefPtr function = animationValue.timingFunction())
        return function;
    if (RefPtr function = this->timingFunction())
        return function;
    return &CubicBezierTimingFunction::defaultTimingFunction();
}

Seconds TextureMapperAnimationBase::computeTotalRunningTime(MonotonicTime time)
{
    if (m_state == State::Paused)
        return m_pauseTime;

    MonotonicTime oldLastRefreshedTime = m_lastRefreshedTime;
    m_lastRefreshedTime = time;
    m_totalRunningTime += m_lastRefreshedTime - oldLastRefreshedTime;
    return m_totalRunningTime;
}

void TextureMapperAnimations::add(const TextureMapperAnimation<FloatAnimationValue>& animation)
{
    // Remove the old state if we are resuming a paused animation.
    remove(animation.name(), animation.keyframes().property());

    m_floatAnimations.append(animation);
}

void TextureMapperAnimations::add(const TextureMapperAnimation<FilterAnimationValue>& animation)
{
    // Remove the old state if we are resuming a paused animation.
    remove(animation.name(), animation.keyframes().property());

    m_filterAnimations.append(animation);
}

void TextureMapperAnimations::add(const TextureMapperAnimation<TransformAnimationValue>& animation)
{
    // Remove the old state if we are resuming a paused animation.
    remove(animation.name(), animation.keyframes().property());

    m_transformAnimations.append(animation);
}

void TextureMapperAnimations::remove(const String& name)
{
    m_floatAnimations.removeAllMatching([&name] (const auto& animation) {
        return animation.name() == name;
    });
    m_filterAnimations.removeAllMatching([&name] (const auto& animation) {
        return animation.name() == name;
    });
    m_transformAnimations.removeAllMatching([&name] (const auto& animation) {
        return animation.name() == name;
    });
}

void TextureMapperAnimations::remove(const String& name, GraphicsLayerAnimationProperty property)
{
    m_floatAnimations.removeAllMatching([&name, property] (const auto& animation) {
        return animation.name() == name && animation.keyframes().property() == property;
    });
    m_filterAnimations.removeAllMatching([&name, property] (const auto& animation) {
        return animation.name() == name && animation.keyframes().property() == property;
    });
    m_transformAnimations.removeAllMatching([&name, property] (const auto& animation) {
        return animation.name() == name && animation.keyframes().property() == property;
    });
}

void TextureMapperAnimations::pause(const String& name, Seconds offset)
{
    for (auto& animation : m_floatAnimations) {
        if (animation.name() == name)
            animation.pause(offset);
    }
    for (auto& animation : m_filterAnimations) {
        if (animation.name() == name)
            animation.pause(offset);
    }
    for (auto& animation : m_transformAnimations) {
        if (animation.name() == name)
            animation.pause(offset);
    }
}

void TextureMapperAnimations::suspend(MonotonicTime time)
{
    // FIXME: This seems wrong. `pause` takes time offset (Seconds), not MonotonicTime.
    // https://bugs.webkit.org/show_bug.cgi?id=183112
    for (auto& animation : m_floatAnimations)
        animation.pause(time.secondsSinceEpoch());
    for (auto& animation : m_filterAnimations)
        animation.pause(time.secondsSinceEpoch());
    for (auto& animation : m_transformAnimations)
        animation.pause(time.secondsSinceEpoch());
}

void TextureMapperAnimations::resume()
{
    for (auto& animation : m_floatAnimations)
        animation.resume();
    for (auto& animation : m_filterAnimations)
        animation.resume();
    for (auto& animation : m_transformAnimations)
        animation.resume();
}

void TextureMapperAnimations::apply(TextureMapperAnimationBase::ApplicationResult& applicationResults, MonotonicTime time, TextureMapperAnimationBase::KeepInternalState keepInternalState)
{
    Vector<TextureMapperAnimation<TransformAnimationValue>*> translateAnimations;
    Vector<TextureMapperAnimation<TransformAnimationValue>*> rotateAnimations;
    Vector<TextureMapperAnimation<TransformAnimationValue>*> scaleAnimations;
    Vector<TextureMapperAnimation<TransformAnimationValue>*> transformAnimations;
    Vector<TextureMapperAnimation<FloatAnimationValue>*> floatAnimations;
    Vector<TextureMapperAnimation<FilterAnimationValue>*> filterAnimations;

    for (auto& animation : m_floatAnimations)
        floatAnimations.append(&animation);
    for (auto& animation : m_filterAnimations)
        filterAnimations.append(&animation);

    for (auto& animation : m_transformAnimations) {
        switch (animation.keyframes().property()) {
        case GraphicsLayerAnimationProperty::Translate:
            translateAnimations.append(&animation);
            break;
        case GraphicsLayerAnimationProperty::Rotate:
            rotateAnimations.append(&animation);
            break;
        case GraphicsLayerAnimationProperty::Scale:
            scaleAnimations.append(&animation);
            break;
        case GraphicsLayerAnimationProperty::Transform:
            transformAnimations.append(&animation);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    if (!translateAnimations.isEmpty() || !rotateAnimations.isEmpty() || !scaleAnimations.isEmpty() || !transformAnimations.isEmpty()) {
        applicationResults.transform = TransformationMatrix();

        if (translateAnimations.isEmpty())
            applicationResults.transform->multiply(m_translate);
        else {
            for (auto* animation : translateAnimations)
                animation->apply(applicationResults, time, keepInternalState);
        }

        if (rotateAnimations.isEmpty())
            applicationResults.transform->multiply(m_rotate);
        else {
            for (auto* animation : rotateAnimations)
                animation->apply(applicationResults, time, keepInternalState);
        }

        if (scaleAnimations.isEmpty())
            applicationResults.transform->multiply(m_scale);
        else {
            for (auto* animation : scaleAnimations)
                animation->apply(applicationResults, time, keepInternalState);
        }

        if (transformAnimations.isEmpty())
            applicationResults.transform->multiply(m_transform);
        else
            transformAnimations.last()->apply(applicationResults, time, keepInternalState);
    }

    for (auto* animation : floatAnimations)
        animation->apply(applicationResults, time, keepInternalState);
    for (auto* animation : filterAnimations)
        animation->apply(applicationResults, time, keepInternalState);
}

bool TextureMapperAnimations::hasActiveAnimationsOfType(GraphicsLayerAnimationProperty type) const
{
    switch (type) {
    case GraphicsLayerAnimationProperty::Translate:
    case GraphicsLayerAnimationProperty::Scale:
    case GraphicsLayerAnimationProperty::Rotate:
    case GraphicsLayerAnimationProperty::Transform:
        return std::ranges::any_of(m_transformAnimations, [&type](const auto& animation) {
            return animation.keyframes().property() == type;
        });

    case GraphicsLayerAnimationProperty::Opacity:
        return std::ranges::any_of(m_floatAnimations, [&type](const auto& animation) {
            return animation.keyframes().property() == type;
        });

    case GraphicsLayerAnimationProperty::Filter:
    case GraphicsLayerAnimationProperty::WebkitBackdropFilter:
        return std::ranges::any_of(m_filterAnimations, [&type](const auto& animation) {
            return animation.keyframes().property() == type;
        });

    default:
        break;
    }

    return false;
}

bool TextureMapperAnimations::hasRunningAnimations() const
{
    bool hasRunningFloatAnimations = std::ranges::any_of(m_floatAnimations, [](const auto& animation) {
        return animation.state() == TextureMapperAnimationBase::State::Playing;
    });
    bool hasRunningFilterAnimations = std::ranges::any_of(m_filterAnimations, [](const auto& animation) {
        return animation.state() == TextureMapperAnimationBase::State::Playing;
    });
    bool hasRunningTransformAnimations = std::ranges::any_of(m_transformAnimations, [](const auto& animation) {
        return animation.state() == TextureMapperAnimationBase::State::Playing;
    });

    return hasRunningFloatAnimations || hasRunningFilterAnimations || hasRunningTransformAnimations;
}

bool TextureMapperAnimations::hasRunningTransformAnimations() const
{
    return std::ranges::any_of(m_transformAnimations, [](const auto& animation) {
        switch (animation.state()) {
        case TextureMapperAnimationBase::State::Playing:
        case TextureMapperAnimationBase::State::Paused:
            break;
        default:
            return false;
        }

        return true;
    });
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
