/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "NicosiaAnimation.h"

#include "LayoutSize.h"
#include "TranslateTransformOperation.h"
#include <wtf/Scope.h>

namespace Nicosia {

using namespace WebCore;

static RefPtr<FilterOperation> blendFunc(FilterOperation* fromOp, FilterOperation& toOp, double progress, const FloatSize&, bool blendToPassthrough = false)
{
    return toOp.blend(fromOp, progress, blendToPassthrough);
}

static FilterOperations applyFilterAnimation(const FilterOperations& from, const FilterOperations& to, double progress, const FloatSize& boxSize)
{
    // First frame of an animation.
    if (!progress)
        return from;

    // Last frame of an animation.
    if (progress == 1)
        return to;

    if (!from.isEmpty() && !to.isEmpty() && !from.operationsMatch(to))
        return to;

    FilterOperations result;

    size_t fromSize = from.operations().size();
    size_t toSize = to.operations().size();
    size_t size = std::max(fromSize, toSize);
    for (size_t i = 0; i < size; i++) {
        RefPtr<FilterOperation> fromOp = (i < fromSize) ? from.operations()[i].get() : nullptr;
        RefPtr<FilterOperation> toOp = (i < toSize) ? to.operations()[i].get() : nullptr;
        RefPtr<FilterOperation> blendedOp = toOp ? blendFunc(fromOp.get(), *toOp, progress, boxSize) : (fromOp ? blendFunc(nullptr, *fromOp, progress, boxSize, true) : nullptr);
        if (blendedOp)
            result.operations().append(blendedOp);
        else {
            auto identityOp = PassthroughFilterOperation::create();
            if (progress > 0.5)
                result.operations().append(toOp ? toOp : WTFMove(identityOp));
            else
                result.operations().append(fromOp ? fromOp : WTFMove(identityOp));
        }
    }

    return result;
}

static bool shouldReverseAnimationValue(WebCore::Animation::Direction direction, int loopCount)
{
    return (direction == WebCore::Animation::Direction::Alternate && loopCount & 1)
        || (direction == WebCore::Animation::Direction::AlternateReverse && !(loopCount & 1))
        || direction == WebCore::Animation::Direction::Reverse;
}

static double normalizedAnimationValue(double runningTime, double duration, WebCore::Animation::Direction direction, double iterationCount)
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

static double normalizedAnimationValueForFillsForwards(double iterationCount, WebCore::Animation::Direction direction)
{
    if (direction == WebCore::Animation::Direction::Normal)
        return 1;
    if (direction == WebCore::Animation::Direction::Reverse)
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

static TransformationMatrix applyTransformAnimation(const TransformOperations& from, const TransformOperations& to, double progress, const FloatSize& boxSize)
{
    TransformationMatrix matrix;

    // First frame of an animation.
    if (!progress) {
        from.apply(boxSize, matrix);
        return matrix;
    }

    // Last frame of an animation.
    if (progress == 1) {
        to.apply(boxSize, matrix);
        return matrix;
    }

    to.blend(from, progress, LayoutSize { boxSize }).apply(boxSize, matrix);
    return matrix;
}

static const TimingFunction& timingFunctionForAnimationValue(const AnimationValue& animationValue, const Animation& animation)
{
    if (animationValue.timingFunction())
        return *animationValue.timingFunction();
    if (animation.timingFunction())
        return *animation.timingFunction();
    return CubicBezierTimingFunction::defaultTimingFunction();
}

static KeyframeValueList createThreadsafeKeyFrames(const KeyframeValueList& originalKeyframes, const FloatSize& boxSize)
{
    if (originalKeyframes.property() != AnimatedProperty::Transform)
        return originalKeyframes;

    // Currently translation operations are the only transform operations that store a non-fixed
    // Length. Some Lengths, in particular those for calc() operations, are not thread-safe or
    // multiprocess safe, because they maintain indices into a shared HashMap of CalculationValues.
    // This code converts all possible unsafe Length parameters to fixed Lengths, which are safe to
    // use in other threads and across IPC channels.
    KeyframeValueList keyframes = originalKeyframes;
    for (unsigned i = 0; i < keyframes.size(); i++) {
        const auto& transformValue = static_cast<const TransformAnimationValue&>(keyframes.at(i));
        for (auto& operation : transformValue.value().operations()) {
            if (is<TranslateTransformOperation>(operation)) {
                TranslateTransformOperation* translation = static_cast<TranslateTransformOperation*>(operation.get());
                translation->setX(Length(translation->xAsFloat(boxSize), LengthType::Fixed));
                translation->setY(Length(translation->yAsFloat(boxSize), LengthType::Fixed));
                translation->setZ(Length(translation->zAsFloat(), LengthType::Fixed));
            }
        }
    }

    return keyframes;
}

Animation::Animation(const String& name, const KeyframeValueList& keyframes, const FloatSize& boxSize, const WebCore::Animation& animation, MonotonicTime startTime, Seconds pauseTime, AnimationState state)
    : m_name(name.isSafeToSendToAnotherThread() ? name : name.isolatedCopy())
    , m_keyframes(createThreadsafeKeyFrames(keyframes, boxSize))
    , m_boxSize(boxSize)
    , m_timingFunction(animation.timingFunction()->clone())
    , m_iterationCount(animation.iterationCount())
    , m_duration(animation.duration())
    , m_direction(animation.direction())
    , m_fillsForwards(animation.fillsForwards())
    , m_startTime(startTime)
    , m_pauseTime(pauseTime)
    , m_totalRunningTime(0_s)
    , m_lastRefreshedTime(m_startTime)
    , m_state(state)
{
}

Animation::Animation(const Animation& other)
    : m_name(other.m_name.isSafeToSendToAnotherThread() ? other.m_name : other.m_name.isolatedCopy())
    , m_keyframes(other.m_keyframes)
    , m_boxSize(other.m_boxSize)
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

Animation& Animation::operator=(const Animation& other)
{
    m_name = other.m_name.isSafeToSendToAnotherThread() ? other.m_name : other.m_name.isolatedCopy();
    m_keyframes = other.m_keyframes;
    m_boxSize = other.m_boxSize;
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

void Animation::apply(ApplicationResult& applicationResults, MonotonicTime time, KeepInternalState keepInternalState)
{
    MonotonicTime oldLastRefreshedTime = m_lastRefreshedTime;
    Seconds oldTotalRunningTime = m_totalRunningTime;
    AnimationState oldState = m_state;

    auto maybeRestoreInternalState = makeScopeExit([&] {
        if (keepInternalState == KeepInternalState::Yes) {
            m_lastRefreshedTime = oldLastRefreshedTime;
            m_totalRunningTime = oldTotalRunningTime;
            m_state = oldState;
        }
    });

    // Even when m_state == AnimationState::Stopped && !m_fillsForwards, we should calculate the last value to avoid a flash.
    // CoordinatedGraphicsScene will soon remove the stopped animation and update the value instead of this function.

    Seconds totalRunningTime = computeTotalRunningTime(time);
    double normalizedValue = normalizedAnimationValue(totalRunningTime.seconds(), m_duration, m_direction, m_iterationCount);

    if (m_iterationCount != WebCore::Animation::IterationCountInfinite && totalRunningTime.seconds() >= m_duration * m_iterationCount) {
        m_state = AnimationState::Stopped;
        m_pauseTime = 0_s;
        normalizedValue = normalizedAnimationValueForFillsForwards(m_iterationCount, m_direction);
    }

    applicationResults.hasRunningAnimations |= (m_state == AnimationState::Playing);

    if (!normalizedValue) {
        applyInternal(applicationResults, m_keyframes.at(0), m_keyframes.at(1), 0);
        return;
    }

    if (normalizedValue == 1.0) {
        applyInternal(applicationResults, m_keyframes.at(m_keyframes.size() - 2), m_keyframes.at(m_keyframes.size() - 1), 1);
        return;
    }
    if (m_keyframes.size() == 2) {
        auto& timingFunction = timingFunctionForAnimationValue(m_keyframes.at(0), *this);
        normalizedValue = timingFunction.transformProgress(normalizedValue, m_duration);
        applyInternal(applicationResults, m_keyframes.at(0), m_keyframes.at(1), normalizedValue);
        return;
    }

    for (size_t i = 0; i < m_keyframes.size() - 1; ++i) {
        const AnimationValue& from = m_keyframes.at(i);
        const AnimationValue& to = m_keyframes.at(i + 1);
        if (from.keyTime() > normalizedValue || to.keyTime() < normalizedValue)
            continue;

        normalizedValue = (normalizedValue - from.keyTime()) / (to.keyTime() - from.keyTime());
        auto& timingFunction = timingFunctionForAnimationValue(from, *this);
        normalizedValue = timingFunction.transformProgress(normalizedValue, m_duration);
        applyInternal(applicationResults, from, to, normalizedValue);
        break;
    }
}

void Animation::pause(Seconds time)
{
    m_state = AnimationState::Paused;
    m_pauseTime = time;
}

void Animation::resume()
{
    m_state = AnimationState::Playing;
    // FIXME: This seems wrong. m_totalRunningTime is cleared.
    // https://bugs.webkit.org/show_bug.cgi?id=183113
    m_pauseTime = 0_s;
    m_totalRunningTime = m_pauseTime;
    m_lastRefreshedTime = MonotonicTime::now();
}

Seconds Animation::computeTotalRunningTime(MonotonicTime time)
{
    if (m_state == AnimationState::Paused)
        return m_pauseTime;

    MonotonicTime oldLastRefreshedTime = m_lastRefreshedTime;
    m_lastRefreshedTime = time;
    m_totalRunningTime += m_lastRefreshedTime - oldLastRefreshedTime;
    return m_totalRunningTime;
}

void Animation::applyInternal(ApplicationResult& applicationResults, const AnimationValue& from, const AnimationValue& to, float progress)
{
    switch (m_keyframes.property()) {
    case AnimatedProperty::Translate:
    case AnimatedProperty::Rotate:
    case AnimatedProperty::Scale:
    case AnimatedProperty::Transform: {
        ASSERT(applicationResults.transform);
        auto transform = applyTransformAnimation(static_cast<const TransformAnimationValue&>(from).value(), static_cast<const TransformAnimationValue&>(to).value(), progress, m_boxSize);
        applicationResults.transform->multiply(transform);
        return;
    }
    case AnimatedProperty::Opacity:
        applicationResults.opacity = applyOpacityAnimation((static_cast<const FloatAnimationValue&>(from).value()), (static_cast<const FloatAnimationValue&>(to).value()), progress);
        return;
    case AnimatedProperty::Filter:
    case AnimatedProperty::WebkitBackdropFilter:
        applicationResults.filters = applyFilterAnimation(static_cast<const FilterAnimationValue&>(from).value(), static_cast<const FilterAnimationValue&>(to).value(), progress, m_boxSize);
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

void Animations::add(const Animation& animation)
{
    // Remove the old state if we are resuming a paused animation.
    remove(animation.name(), animation.keyframes().property());

    m_animations.append(animation);
}

void Animations::remove(const String& name)
{
    m_animations.removeAllMatching([&name] (const Animation& animation) {
        return animation.name() == name;
    });
}

void Animations::remove(const String& name, AnimatedProperty property)
{
    m_animations.removeAllMatching([&name, property] (const Animation& animation) {
        return animation.name() == name && animation.keyframes().property() == property;
    });
}

void Animations::pause(const String& name, Seconds offset)
{
    for (auto& animation : m_animations) {
        if (animation.name() == name)
            animation.pause(offset);
    }
}

void Animations::suspend(MonotonicTime time)
{
    // FIXME: This seems wrong. `pause` takes time offset (Seconds), not MonotonicTime.
    // https://bugs.webkit.org/show_bug.cgi?id=183112
    for (auto& animation : m_animations)
        animation.pause(time.secondsSinceEpoch());
}

void Animations::resume()
{
    for (auto& animation : m_animations)
        animation.resume();
}

void Animations::apply(Animation::ApplicationResult& applicationResults, MonotonicTime time, Animation::KeepInternalState keepInternalState)
{
    Vector<Animation*> translateAnimations;
    Vector<Animation*> rotateAnimations;
    Vector<Animation*> scaleAnimations;
    Vector<Animation*> transformAnimations;
    Vector<Animation*> leafAnimations;

    for (auto& animation : m_animations) {
        switch (animation.keyframes().property()) {
        case AnimatedProperty::Translate:
            translateAnimations.append(&animation);
            break;
        case AnimatedProperty::Rotate:
            rotateAnimations.append(&animation);
            break;
        case AnimatedProperty::Scale:
            scaleAnimations.append(&animation);
            break;
        case AnimatedProperty::Transform:
            transformAnimations.append(&animation);
            break;
        default:
            leafAnimations.append(&animation);
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

    for (auto* animation : leafAnimations)
        animation->apply(applicationResults, time, keepInternalState);
}

bool Animations::hasActiveAnimationsOfType(AnimatedProperty type) const
{
    return std::any_of(m_animations.begin(), m_animations.end(),
        [&type](const Animation& animation) {
            return animation.keyframes().property() == type;
        });
}

bool Animations::hasRunningAnimations() const
{
    return std::any_of(m_animations.begin(), m_animations.end(),
        [](const Animation& animation) {
            return animation.state() == Animation::AnimationState::Playing;
        });
}

bool Animations::hasRunningTransformAnimations() const
{
    return std::any_of(m_animations.begin(), m_animations.end(),
        [](const Animation& animation) {
            switch (animation.keyframes().property()) {
            case AnimatedProperty::Translate:
            case AnimatedProperty::Rotate:
            case AnimatedProperty::Scale:
            case AnimatedProperty::Transform:
                break;
            default:
                return false;
            }

            switch (animation.state()) {
            case Animation::AnimationState::Playing:
            case Animation::AnimationState::Paused:
                break;
            default:
                return false;
            }

            return true;
        });
}

} // namespace Nicosia
