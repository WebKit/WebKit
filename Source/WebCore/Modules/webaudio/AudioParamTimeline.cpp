/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioParamTimeline.h"

#include "AudioUtilities.h"
#include "FloatConversion.h"
#include "VectorMath.h"
#include <algorithm>
#include <wtf/MathExtras.h>

namespace WebCore {

// Test that for a SetTarget event, the current value is close enough to the target value that
// we can consider the event to have converged to the target.
static bool hasSetTargetConverged(float value, float target, Seconds currentTime, Seconds startTime, double timeConstant)
{
    // For a SetTarget event, we want the event to terminate eventually so that we can stop using
    // the timeline to compute the values.
    constexpr float timeConstantsToConverge = 10;
    constexpr float setTargetThreshold = 4.539992976248485e-05;

    // Converged if enough time constants (|timeConstantsToConverge|) have passed since the start
    // of the event.
    if (currentTime.value() > startTime.value() + timeConstantsToConverge * timeConstant)
        return true;

    // If |target| is 0, converged if |value| is less than |setTargetThreshold|.
    if (!target && std::abs(value) < setTargetThreshold)
        return true;

    // If |target| is not zero, converged if relative difference between |value|
    // and |target| is small. That is |target - value| / |target| < |setTargetThreshold|.
    if (target && std::abs(target - value) < setTargetThreshold * std::abs(value))
        return true;

    return false;
}

ExceptionOr<void> AudioParamTimeline::setValueAtTime(float value, Seconds time)
{
    auto locker = holdLock(m_eventsMutex);
    return insertEvent(ParamEvent::createSetValueEvent(value, time));
}

ExceptionOr<void> AudioParamTimeline::linearRampToValueAtTime(float targetValue, Seconds endTime, float currentValue, Seconds currentTime)
{
    auto locker = holdLock(m_eventsMutex);

    // Linear ramp events need a preceding event so that they have an initial value.
    if (m_events.isEmpty())
        insertEvent(ParamEvent::createSetValueEvent(currentValue, currentTime));

    return insertEvent(ParamEvent::createLinearRampEvent(targetValue, endTime));
}

ExceptionOr<void> AudioParamTimeline::exponentialRampToValueAtTime(float targetValue, Seconds endTime, float currentValue, Seconds currentTime)
{
    auto locker = holdLock(m_eventsMutex);

    // Exponential ramp events need a preceding event so that they have an initial value.
    if (m_events.isEmpty())
        insertEvent(ParamEvent::createSetValueEvent(currentValue, currentTime));

    return insertEvent(ParamEvent::createExponentialRampEvent(targetValue, endTime));
}

ExceptionOr<void> AudioParamTimeline::setTargetAtTime(float target, Seconds time, float timeConstant)
{
    auto locker = holdLock(m_eventsMutex);
    return insertEvent(ParamEvent::createSetTargetEvent(target, time, timeConstant));
}

ExceptionOr<void> AudioParamTimeline::setValueCurveAtTime(Vector<float>&& curve, Seconds time, Seconds duration)
{
    auto locker = holdLock(m_eventsMutex);
    return insertEvent(ParamEvent::createSetValueCurveEvent(WTFMove(curve), time, duration));
}

static bool isValidNumber(float x)
{
    return !std::isnan(x) && !std::isinf(x);
}

static bool isValidNumber(Seconds s)
{
    return !std::isnan(s.value()) && !std::isinf(s.value());
}

ExceptionOr<void> AudioParamTimeline::insertEvent(UniqueRef<ParamEvent> event)
{
    // Sanity check the event. Be super careful we're not getting infected with NaN or Inf.
    bool isValid = event->type() < ParamEvent::LastType
        && isValidNumber(event->value())
        && isValidNumber(event->time())
        && isValidNumber(event->timeConstant())
        && isValidNumber(event->duration())
        && event->duration() >= 0_s;

    if (!isValid)
        return { };
        
    ASSERT(m_eventsMutex.isLocked());

    unsigned i = 0;
    auto insertTime = event->time();

    for (auto& paramEvent : m_events) {
        if (event->type() == ParamEvent::SetValueCurve) {
            if (paramEvent->type() != ParamEvent::CancelValues) {
                // If this event is a SetValueCurve, make sure it doesn't overlap any existing event.
                // It's ok if the SetValueCurve starts at the same time as the end of some other duration.
                auto endTime = event->time() + event->duration();
                if (paramEvent->type() == ParamEvent::SetValueCurve) {
                    auto paramEventEndTime = paramEvent->time() + paramEvent->duration();
                    if ((paramEvent->time() >= event->time() && paramEvent->time() < endTime)
                        || (paramEventEndTime > event->time() && paramEventEndTime < endTime)
                        || (event->time() >= paramEvent->time() && event->time() < paramEventEndTime)
                        || (endTime >= paramEvent->time() && endTime < paramEventEndTime)) {
                        return Exception { NotSupportedError, "Events are overlapping"_s };
                    }
                } else if (paramEvent->time() > event->time() && paramEvent->time() < endTime)
                    return Exception { NotSupportedError, "Events are overlapping"_s };
            }
        } else if (paramEvent->type() == ParamEvent::SetValueCurve) {
            // Otherwise, make sure this event doesn't overlap any existing SetValueCurve event.
            auto parentEventEndTime = paramEvent->time() + paramEvent->duration();
            if (event->time() >= paramEvent->time() && event->time() < parentEventEndTime)
                return Exception { NotSupportedError, "Events are overlapping" };
        }

        if (paramEvent->time() > insertTime)
            break;

        ++i;
    }

    m_events.insert(i, WTFMove(event));
    return { };
}

void AudioParamTimeline::cancelScheduledValues(Seconds cancelTime)
{
    auto locker = holdLock(m_eventsMutex);

    // Remove all events whose start time is greater than or equal to the cancel time.
    // Also handle the special case where the cancel time lies in the middle of a
    // SetValueCurve event.
    //
    // This critically depends on the fact that no event can be scheduled in the middle
    // of the curve or at the same start time. Then removing the SetValueCurve doesn't
    // remove any events that shouldn't have been.
    auto isAfter = [](const ParamEvent& event, Seconds cancelTime) {
        auto eventTime = event.time();
        if (eventTime >= cancelTime)
            return true;
        return event.type() == ParamEvent::SetValueCurve
            && eventTime <= cancelTime
            && (eventTime + event.duration() > cancelTime);
    };

    // Remove all events starting at cancelTime.
    for (unsigned i = 0; i < m_events.size(); ++i) {
        if (isAfter(m_events[i], cancelTime)) {
            m_events.remove(i, m_events.size() - i);
            break;
        }
    }
}

ExceptionOr<void> AudioParamTimeline::cancelAndHoldAtTime(Seconds cancelTime)
{
    auto locker = holdLock(m_eventsMutex);

    // Find the first event at or just past cancelTime.
    size_t i = m_events.findMatching([&](auto& event) {
        return event->time() > cancelTime;
    });
    i = (i == notFound) ? m_events.size() : i;

    // The event that is being cancelled. This is the event just past cancelTime, if any.
    size_t cancelledEventIndex = i;

    // If the event just before cancelTime is a SetTarget or SetValueCurve event, we need
    // to handle that event specially instead of the event after.
    if (i > 0 && ((m_events[i - 1]->type() == ParamEvent::SetTarget) || (m_events[i - 1]->type() == ParamEvent::SetValueCurve)))
        cancelledEventIndex = i - 1;
    else if (i >= m_events.size()) {
        // If there were no events occurring after |cancelTime| (and the
        // previous event is not SetTarget or SetValueCurve, we're done.
        return { };
    }

    // cancelledEvent is the event that is being cancelled.
    auto& cancelledEvent = m_events[cancelledEventIndex].get();
    auto eventType = cancelledEvent.type();

    // New event to be inserted, if any, and a SetValueEvent if needed.
    std::unique_ptr<ParamEvent> newEvent;
    std::unique_ptr<ParamEvent> newSetValueEvent;

    switch (eventType) {
    case ParamEvent::LinearRampToValue:
    case ParamEvent::ExponentialRampToValue: {
        // For these events we need to remember the parameters of the event
        // for a CancelValues event so that we can properly cancel the event
        // and hold the value.
        auto savedEvent = makeUniqueRef<ParamEvent>(eventType, cancelledEvent.value(), cancelledEvent.time(), cancelledEvent.timeConstant(), cancelledEvent.duration(), Vector<float> { cancelledEvent.curve() }, cancelledEvent.curvePointsPerSecond(), cancelledEvent.curveEndValue(), nullptr);
        newEvent = ParamEvent::createCancelValuesEvent(cancelTime, savedEvent.moveToUniquePtr()).moveToUniquePtr();
        break;
    }
    case ParamEvent::SetTarget: {
        if (cancelledEvent.time() < cancelTime) {
            // Don't want to remove the SetTarget event if it started before the
            // cancel time, so bump the index. But we do want to insert a
            // cancelEvent so that we stop this automation and hold the value when
            // we get there.
            ++cancelledEventIndex;

            newEvent = ParamEvent::createCancelValuesEvent(cancelTime, nullptr).moveToUniquePtr();
        }
        break;
    }
    case ParamEvent::SetValueCurve: {
        // If the setValueCurve event started strictly before the cancel time,
        // there might be something to do....
        if (cancelledEvent.time() < cancelTime) {
            if (cancelTime > cancelledEvent.time() + cancelledEvent.duration()) {
                // If the cancellation time is past the end of the curve there's
                // nothing to do except remove the following events.
                ++cancelledEventIndex;
            } else {
                // Cancellation time is in the middle of the curve. Therefore,
                // create a new SetValueCurve event with the appropriate new
                // parameters to cancel this event properly. Since it's illegal
                // to insert any event within a SetValueCurve event, we can
                // compute the new end value now instead of doing when running
                // the timeline.
                auto newDuration = cancelTime - cancelledEvent.time();
                float endValue = valueCurveAtTime(cancelTime, cancelledEvent.time(), cancelledEvent.duration(), cancelledEvent.curve().data(), cancelledEvent.curve().size());

                // Replace the existing SetValueCurve with this new one that is identical except for the duration.
                newEvent = makeUniqueRef<ParamEvent>(eventType, cancelledEvent.value(), cancelledEvent.time(), cancelledEvent.timeConstant(), newDuration, Vector<float> { cancelledEvent.curve() }, cancelledEvent.curvePointsPerSecond(), endValue, nullptr).moveToUniquePtr();
                newSetValueEvent = ParamEvent::createSetValueEvent(endValue, cancelledEvent.time() + newDuration).moveToUniquePtr();
            }
        }
        break;
    }
    case ParamEvent::SetValue:
    case ParamEvent::CancelValues:
        // Nothing needs to be done for a SetValue or CancelValues event.
        break;
    case ParamEvent::LastType:
        ASSERT_NOT_REACHED();
        break;
    }

    // Now remove all the following events from the timeline.
    if (cancelledEventIndex < m_events.size())
        removeCancelledEvents(cancelledEventIndex);

    // Insert the new event, if any.
    if (newEvent) {
        auto result = insertEvent(makeUniqueRefFromNonNullUniquePtr(WTFMove(newEvent)));
        if (result.hasException())
            return result.releaseException();
        if (newSetValueEvent) {
            insertEvent(makeUniqueRefFromNonNullUniquePtr(WTFMove(newSetValueEvent)));
            if (result.hasException())
                return result.releaseException();
        }
    }

    return { };
}

void AudioParamTimeline::removeCancelledEvents(size_t firstEventToRemove)
{
    m_events.remove(firstEventToRemove, m_events.size() - firstEventToRemove);
}

Optional<float> AudioParamTimeline::valueForContextTime(BaseAudioContext& context, float defaultValue, float minValue, float maxValue)
{
    {
        std::unique_lock<Lock> lock(m_eventsMutex, std::try_to_lock);
        if (!lock.owns_lock() || !m_events.size() || Seconds { context.currentTime() } < m_events[0]->time())
            return WTF::nullopt;
    }

    // Ask for just a single value.
    float value;
    double sampleRate = context.sampleRate();
    size_t startFrame = context.currentSampleFrame();
    size_t endFrame = startFrame + 1;
    double controlRate = sampleRate / AudioNode::ProcessingSizeInFrames; // one parameter change per render quantum
    value = valuesForTimeRange(startFrame, endFrame, defaultValue, minValue, maxValue, &value, 1, sampleRate, controlRate);
    return value;
}

float AudioParamTimeline::valuesForTimeRange(size_t startFrame, size_t endFrame, float defaultValue, float minValue, float maxValue, float* values, unsigned numberOfValues, double sampleRate, double controlRate)
{
    // We can't contend the lock in the realtime audio thread.
    std::unique_lock<Lock> lock(m_eventsMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        if (values) {
            for (unsigned i = 0; i < numberOfValues; ++i)
                values[i] = defaultValue;
        }
        return defaultValue;
    }

    float value = valuesForTimeRangeImpl(startFrame, endFrame, defaultValue, values, numberOfValues, sampleRate, controlRate);

    // Clamp values based on range allowed by AudioParam's min and max values.
    VectorMath::vclip(values, 1, &minValue, &maxValue, values, 1, numberOfValues);

    return value;
}

float AudioParamTimeline::valuesForTimeRangeImpl(size_t startFrame, size_t endFrame, float defaultValue, float* values, unsigned numberOfValues, double sampleRate, double controlRate)
{
    ASSERT(values);
    if (!values)
        return defaultValue;

    double samplingPeriod = 1. / sampleRate;

    // Return default value if there are no events matching the desired time range.
    if (!m_events.size() || endFrame * samplingPeriod <= m_events[0]->time().value()) {
        for (unsigned i = 0; i < numberOfValues; ++i)
            values[i] = defaultValue;
        return defaultValue;
    }

    // Maintain a running time and index for writing the values buffer.
    size_t currentFrame = startFrame;
    unsigned writeIndex = 0;

    // If first event is after startTime then fill initial part of values buffer with defaultValue
    // until we reach the first event time.
    auto firstEventTime = m_events[0]->time();
    if (firstEventTime.value() > startFrame * samplingPeriod) {
        size_t fillToEndFrame = endFrame;
        double firstEventFrame = ceil(firstEventTime.value() * sampleRate);
        if (endFrame > firstEventFrame)
            fillToEndFrame = firstEventFrame;
        ASSERT(fillToEndFrame >= startFrame);

        unsigned fillToFrame = static_cast<unsigned>(fillToEndFrame - startFrame);
        fillToFrame = std::min(fillToFrame, numberOfValues);
        for (; writeIndex < fillToFrame; ++writeIndex)
            values[writeIndex] = defaultValue;

        currentFrame += fillToFrame;
    }

    float value = defaultValue;

    // Go through each event and render the value buffer where the times overlap,
    // stopping when we've rendered all the requested values.
    // FIXME: could try to optimize by avoiding having to iterate starting from the very first event
    // and keeping track of a "current" event index.
    int n = m_events.size();
    for (int i = 0; i < n && writeIndex < numberOfValues; ++i) {
        auto* event = &m_events[i].get();
        auto* nextEvent = i < n - 1 ? &m_events[i + 1].get() : nullptr;

        // Wait until we get a more recent event.
        if (!isEventCurrent(*event, nextEvent, currentFrame, sampleRate))
            continue;

        auto nextEventType = nextEvent ? static_cast<ParamEvent::Type>(nextEvent->type()) : ParamEvent::LastType /* unknown */;

        processSetTargetFollowedByRamp(i, event, nextEventType, currentFrame, sampleRate, controlRate, value);

        float value1 = event->value();
        auto time1 = event->time();
        float value2 = nextEvent ? nextEvent->value() : value1;
        auto time2 = nextEvent ? nextEvent->time() : Seconds { endFrame * samplingPeriod + 1 };

        ASSERT(time2 >= time1);

        handleCancelValues(*event, nextEvent, value2, time2, nextEventType);

        auto deltaTime = time2 - time1;

        size_t fillToEndFrame = endFrame;
        if (endFrame > time2.value() * sampleRate)
            fillToEndFrame = static_cast<size_t>(ceil(time2.value() * sampleRate));

        ASSERT(fillToEndFrame >= startFrame);
        unsigned fillToFrame = static_cast<unsigned>(fillToEndFrame - startFrame);
        fillToFrame = std::min(fillToFrame, numberOfValues);

        // First handle linear and exponential ramps which require looking ahead to the next event.
        if (nextEventType == ParamEvent::LinearRampToValue)
            processLinearRamp(values, writeIndex, fillToFrame, value, value1, value2, deltaTime, time1, samplingPeriod, currentFrame);
        else if (nextEventType == ParamEvent::ExponentialRampToValue) {
            if (!value1 || value1 * value2 < 0) {
                // Per the specification:
                // If value1 and value2 have opposite signs or if value1 is zero, then v(t) = value1 for T0 <= t < T1.
                value = value1;
                for (; writeIndex < fillToFrame; ++writeIndex)
                    values[writeIndex] = value;
            } else {
                float numSampleFrames = deltaTime.value() * sampleRate;
                // The value goes exponentially from value1 to value2 in a duration of deltaTime seconds (corresponding to numSampleFrames).
                // Compute the per-sample multiplier.
                float multiplier = powf(value2 / value1, 1 / numSampleFrames);

                // Set the starting value of the exponential ramp.
                value = value1 * pow(value2 / static_cast<double>(value1), (currentFrame * samplingPeriod - time1.value()) / deltaTime.value());

                for (; writeIndex < fillToFrame; ++writeIndex) {
                    values[writeIndex] = value;
                    value *= multiplier;
                    ++currentFrame;
                }

                // |value| got updated one extra time in the above loop. Restore it to the last computed value.
                if (writeIndex >= 1)
                    value /= multiplier;
            }
        } else {
            // Handle event types not requiring looking ahead to the next event.
            switch (event->type()) {
            case ParamEvent::SetValue:
            case ParamEvent::LinearRampToValue:
            case ParamEvent::ExponentialRampToValue:
                currentFrame = fillToEndFrame;

                // Simply stay at a constant value.
                value = event->value();
                for (; writeIndex < fillToFrame; ++writeIndex)
                    values[writeIndex] = value;

                break;
            case ParamEvent::CancelValues:
                // If the previous event was a SetTarget or ExponentialRamp
                // event, the current value is one sample behind. Update
                // the sample value by one sample, but only at the start of
                // this CancelValues event.
                if (event->hasDefaultCancelledValue())
                    value = event->value();
                else {
                    double cancelFrame = time1.value() * sampleRate;
                    if (i >= 1 && cancelFrame <= currentFrame && currentFrame < cancelFrame + 1) {
                        auto lastEventType = m_events[i - 1]->type();
                        if (lastEventType == ParamEvent::SetTarget) {
                            float target = m_events[i - 1]->value();
                            float timeConstant = m_events[i - 1]->timeConstant();
                            float discreteTimeConstant = static_cast<float>(AudioUtilities::discreteTimeConstantForSampleRate(timeConstant, controlRate));
                            value += (target - value) * discreteTimeConstant;
                        }
                    }
                }

                for (; writeIndex < fillToFrame; ++writeIndex)
                    values[writeIndex] = value;

                currentFrame = fillToEndFrame;

                break;
            case ParamEvent::SetTarget: {
                // Exponential approach to target value with given time constant.
                float target = event->value();
                float timeConstant = event->timeConstant();
                float discreteTimeConstant = static_cast<float>(AudioUtilities::discreteTimeConstantForSampleRate(timeConstant, controlRate));

                // Set the starting value correctly. This is only needed when the
                // current time is "equal" to the start time of this event. This is
                // to get the sampling correct if the start time of this automation
                // isn't on a frame boundary. Otherwise, we can just continue from
                // where we left off from the previous rendering quantum.
                double rampStartFrame = time1.value() * sampleRate;
                // Condition is c - 1 < r <= c where c = currentFrame and r =
                // rampStartFrame. Compute it this way because currentFrame is
                // unsigned and could be 0.
                if (rampStartFrame <= currentFrame && currentFrame < rampStartFrame + 1)
                    value = target + (value - target) * exp(-(currentFrame * samplingPeriod - time1.value()) / timeConstant);
                else {
                    // Otherwise, need to compute a new value because |value| is the
                    // last computed value of SetTarget. Time has progressed by one
                    // frame, so we need to update the value for the new frame.
                    value += (target - value) * discreteTimeConstant;
                }

                // If the value is close enough to the target, just fill in the data
                // with the target value.
                if (hasSetTargetConverged(value, target, Seconds { currentFrame * samplingPeriod }, time1, timeConstant)) {
                    currentFrame += fillToFrame - writeIndex;
                    for (; writeIndex < fillToFrame; ++writeIndex)
                        values[writeIndex] = target;
                    value = target;
                } else {
                    processSetTarget(values, writeIndex, fillToFrame, value, target, discreteTimeConstant);
                    currentFrame = fillToEndFrame;
                }

                break;
            }
            case ParamEvent::SetValueCurve: {
                float* curveData = event->curve().data();
                unsigned numberOfCurvePoints = event->curve().size();
                float curveEndValue = event->curveEndValue();

                // Curve events have duration, so don't just use next event time.
                auto duration = event->duration();
                double curvePointsPerFrame = event->curvePointsPerSecond() * samplingPeriod;

                if (!curveData || !numberOfCurvePoints || duration <= 0_s || sampleRate <= 0) {
                    // Error condition - simply propagate previous value.
                    currentFrame = fillToEndFrame;
                    for (; writeIndex < fillToFrame; ++writeIndex)
                        values[writeIndex] = value;
                    break;
                }

                // Save old values and recalculate information based on the curve's duration
                // instead of the next event time.
                unsigned nextEventFillToFrame = fillToFrame;

                double curveEndFrame = ceil(sampleRate * (time1 + duration).value());
                if (endFrame > curveEndFrame)
                    fillToEndFrame = static_cast<size_t>(curveEndFrame);
                else
                    fillToEndFrame = endFrame;

                fillToFrame = (fillToEndFrame < startFrame) ? 0 : static_cast<unsigned>(fillToEndFrame - startFrame);
                fillToFrame = std::min(fillToFrame, numberOfValues);

                // Index into the curve data using a floating-point value.
                // We're scaling the number of curve points by the duration (see curvePointsPerFrame).
                double curveVirtualIndex = 0;
                if (time1.value() < currentFrame * samplingPeriod) {
                    // Index somewhere in the middle of the curve data.
                    // Don't use timeToSampleFrame() since we want the exact floating-point frame.
                    double frameOffset = currentFrame - time1.value() * sampleRate;
                    curveVirtualIndex = curvePointsPerFrame * frameOffset;
                }

                // Set the default value in case fillToFrame is 0.
                value = curveEndValue;

                // Render the stretched curve data using nearest neighbor sampling.
                // Oversampled curve data can be provided if smoothness is desired.
                int k = 0;
                for (; writeIndex < fillToFrame; ++writeIndex, ++k) {
                    // Compute current index this way to minimize round-off that would
                    // have occurred by incrementing the index by curvePointsPerFrame.
                    double currentVirtualIndex = curveVirtualIndex + k * curvePointsPerFrame;
                    unsigned curveIndex0;

                    // Clamp index to the last element of the array.
                    if (currentVirtualIndex < numberOfCurvePoints)
                        curveIndex0 = static_cast<unsigned>(currentVirtualIndex);
                    else
                        curveIndex0 = numberOfCurvePoints - 1;

                    unsigned curveIndex1 = std::min(curveIndex0 + 1, numberOfCurvePoints - 1);

                    // Linearly interpolate between the two nearest curve points.
                    // |delta| is clamped to 1 because currentVirtualIndex can exceed
                    // curveIndex0 by more than one. This can happen when we reached
                    // the end of the curve but still need values to fill out the
                    // current rendering quantum.
                    ASSERT(curveIndex0 < numberOfCurvePoints);
                    ASSERT(curveIndex1 < numberOfCurvePoints);
                    float c0 = curveData[curveIndex0];
                    float c1 = curveData[curveIndex1];
                    double delta = std::min(currentVirtualIndex - curveIndex0, 1.0);

                    value = c0 + (c1 - c0) * delta;

                    values[writeIndex] = value;
                }

                // If there's any time left after the duration of this event and the start
                // of the next, then just propagate the last value.
                if (writeIndex < nextEventFillToFrame) {
                    value = curveEndValue;
                    for (; writeIndex < nextEventFillToFrame; ++writeIndex)
                        values[writeIndex] = value;
                }

                // Re-adjust current time
                currentFrame += nextEventFillToFrame;

                break;
            }
            case ParamEvent::LastType:
                ASSERT_NOT_REACHED();
                break;
            }
        }
    }

    // If there's any time left after processing the last event then just propagate the last value
    // to the end of the values buffer.
    for (; writeIndex < numberOfValues; ++writeIndex)
        values[writeIndex] = value;

    return value;
}

void AudioParamTimeline::processLinearRamp(float* values, unsigned& writeIndex, unsigned fillToFrame, float& value, float value1, float value2, Seconds deltaTime, Seconds time1, double samplingPeriod, size_t& currentFrame)
{
    float valueDelta = value2 - value1;
    // Since deltaTime is a double, 1/deltaTime can easily overflow a float. Thus, if deltaTime
    // is close enough to zero (less than float min), treat it as zero.
    float k = deltaTime.value() <= std::numeric_limits<float>::min() ? 0 : 1 / deltaTime.value();

    unsigned fillToFrameTrunc = writeIndex + ((fillToFrame - writeIndex) / 4) * 4;
    if (fillToFrameTrunc > writeIndex) {
        // Minimize in-loop operations. Calculate starting value and increment.
        // Next step: value += inc.
        //  value = value1 + (currentFrame/sampleRate - time1) * k * (value2 - value1);
        //  inc = 4 / sampleRate * k * (value2 - value1);
        // Resolve recursion by expanding constants to achieve a 4-step loop unrolling.
        //  value = value1 + ((currentFrame/sampleRate - time1) + i * sampleFrameTimeIncr) * k * (value2 - value1), i in 0..3
        values[writeIndex] = 0;
        values[writeIndex + 1] = 1;
        values[writeIndex + 2] = 2;
        values[writeIndex + 3] = 3;
        float scalar = samplingPeriod;
        VectorMath::vsmul(values + writeIndex, 1, &scalar, values + writeIndex, 1, 4);
        scalar = currentFrame * samplingPeriod - time1.value();
        VectorMath::vsadd(values + writeIndex, 1, &scalar, values + writeIndex, 1, 4);
        scalar = k * valueDelta;
        VectorMath::vsmul(values + writeIndex, 1, &scalar, values + writeIndex, 1, 4);
        VectorMath::vsadd(values + writeIndex, 1, &value1, values + writeIndex, 1, 4);

        float inc = 4 * samplingPeriod * k * valueDelta;

        // Truncate loop steps to multiple of 4.
        unsigned fillToFrameTrunc = writeIndex + ((fillToFrame - writeIndex) / 4) * 4;
        // Compute final frame.
        currentFrame += fillToFrameTrunc - writeIndex;

        // Process 4 loop steps.
        writeIndex += 4;
        for (; writeIndex < fillToFrameTrunc; writeIndex += 4)
            VectorMath::vsadd(values + writeIndex - 4, 1, &inc, values + writeIndex, 1, 4);
    }
    // Update |value| with the last value computed so that the .value attribute of the AudioParam gets
    // the correct linear ramp value, in case the following loop doesn't execute.
    if (writeIndex >= 1)
        value = values[writeIndex - 1];

    // Serially process remaining values.
    for (; writeIndex < fillToFrame; ++writeIndex) {
        float x = (currentFrame * samplingPeriod - time1.value()) * k;
        value = value1 + valueDelta * x;
        values[writeIndex] = value;
        ++currentFrame;
    }
}

void AudioParamTimeline::processSetTarget(float* values, unsigned& writeIndex, unsigned fillToFrame, float& value, float target, float discreteTimeConstant)
{
    if (fillToFrame > writeIndex) {
        // Resolve recursion by expanding constants to achieve a 4-step loop unrolling.
        //
        // v1 = v0 + (t - v0) * c
        // v2 = v1 + (t - v1) * c
        // v2 = v0 + (t - v0) * c + (t - (v0 + (t - v0) * c)) * c
        // v2 = v0 + (t - v0) * c + (t - v0) * c - (t - v0) * c * c
        // v2 = v0 + (t - v0) * c * (2 - c)
        // Thus c0 = c, c1 = c*(2-c). The same logic applies to c2 and c3.
        const float c0 = discreteTimeConstant;
        const float c1 = c0 * (2 - c0);
        const float c2 = c0 * ((c0 - 3) * c0 + 3);
        const float c3 = c0 * (c0 * ((4 - c0) * c0 - 6) + 4);
        float delta;

        // Process 4 loop steps.
        unsigned fillToFrameTrunc = writeIndex + ((fillToFrame - writeIndex) / 4) * 4;
        const float cVector[4] = { 0, c0, c1, c2 };

        for (; writeIndex < fillToFrameTrunc; writeIndex += 4) {
            delta = target - value;

            VectorMath::vsmul(&cVector[0], 1, &delta, &values[writeIndex], 1, 4);
            VectorMath::vsadd(&values[writeIndex], 1, &value, &values[writeIndex], 1, 4);

            value += delta * c3;
        }
    }

    // Serially process remaining values.
    for (; writeIndex < fillToFrame; ++writeIndex) {
        values[writeIndex] = value;
        value += (target - value) * discreteTimeConstant;
    }

    // The previous loops may have updated |value| one extra time.
    // Reset it to the last computed value.
    if (writeIndex >= 1)
        value = values[writeIndex - 1];
}

void AudioParamTimeline::processSetTargetFollowedByRamp(int eventIndex, ParamEvent*& event, ParamEvent::Type nextEventType, size_t currentFrame, double sampleRate, double controlRate, float& value)
{
    // If the current event is SetTarget and the next event is a LinearRampToValue or ExponentialRampToValue,
    // special handling is needed. In this case, the linear and exponential ramp should start at wherever
    // the SetTarget processing has reached.
    if (event->type() != ParamEvent::SetTarget)
        return;

    if (nextEventType != ParamEvent::LinearRampToValue && nextEventType != ParamEvent::ExponentialRampToValue)
        return;

    // Replace the SetTarget with a SetValue to set the starting time and value for the ramp using the
    // current frame. We need to update |value| appropriately depending on whether the ramp has started
    // or not.
    //
    // If SetTarget starts somewhere between currentFrame - 1 and currentFrame, we directly compute the
    // value it would have at currentFrame. If not, we update the value from the value from currentFrame - 1.
    //
    // Can't use the condition currentFrame - 1 <= t0 * sampleRate <= currentFrame because currentFrame
    // is unsigned and could be 0. Instead, compute the condition this way, where f = currentFrame and
    // Fs = sampleRate:
    //
    //    f - 1 <= t0 * Fs <= f
    //    2 * f - 2 <= 2 * Fs * t0 <= 2 * f
    //    -2 <= 2 * Fs * t0 - 2 * f <= 0
    //    -1 <= 2 * Fs * t0 - 2 * f + 1 <= 1
    //     abs(2 * Fs * t0 - 2 * f + 1) <= 1

    if (fabs(2 * sampleRate * event->time().value() - 2 * currentFrame + 1) <= 1) {
        // SetTarget is starting somewhere between currentFrame - 1 and currentFrame. Compute the value
        // the SetTarget would have at the currentFrame.
        value = event->value() + (value - event->value()) * exp(-(currentFrame / sampleRate - event->time().value()) / event->timeConstant());
    } else {
        // SetTarget has already started. Update |value| one frame because it's the value from the previous frame.
        float discreteTimeConstant = static_cast<float>(AudioUtilities::discreteTimeConstantForSampleRate(event->timeConstant(), controlRate));
        value += (event->value() - value) * discreteTimeConstant;
    }
    // Insert a SetValueEvent to mark the starting value and time.
    // Clear the clamp check because this doesn't need it.
    m_events[eventIndex] = ParamEvent::createSetValueEvent(value, Seconds { currentFrame / sampleRate });

    // Update our pointer to the current event because we just changed it.
    event = &m_events[eventIndex].get();
}


float AudioParamTimeline::linearRampAtTime(Seconds t, float value1, Seconds time1, float value2, Seconds time2)
{
    return value1 + (value2 - value1) * (t - time1).value() / (time2 - time1).value();
}

float AudioParamTimeline::exponentialRampAtTime(Seconds t, float value1, Seconds time1, float value2, Seconds time2)
{
    return value1 * pow(value2 / value1, (t - time1).value() / (time2 - time1).value());
}

float AudioParamTimeline::valueCurveAtTime(Seconds t, Seconds time1, Seconds duration, const float* curveData, size_t curveLength)
{
    double curveIndex = (curveLength - 1) / duration.value() * (t - time1).value();
    size_t k = std::min(static_cast<size_t>(curveIndex), curveLength - 1);
    size_t k1 = std::min(k + 1, curveLength - 1);
    float c0 = curveData[k];
    float c1 = curveData[k1];
    float delta = std::min(curveIndex - k, 1.0);

    return c0 + (c1 - c0) * delta;
}

void AudioParamTimeline::handleCancelValues(ParamEvent& event, ParamEvent* nextEvent, float& value2, Seconds& time2, ParamEvent::Type& nextEventType)
{
    if (!nextEvent || nextEvent->type() != ParamEvent::CancelValues || !nextEvent->savedEvent())
        return;

    float value1 = event.value();
    auto time1 = event.time();

    switch (event.type()) {
    case ParamEvent::CancelValues:
    case ParamEvent::LinearRampToValue:
    case ParamEvent::ExponentialRampToValue:
    case ParamEvent::SetValue: {
        // These three events potentially establish a starting value for
        // the following event, so we need to examine the cancelled
        // event to see what to do.
        auto* savedEvent = nextEvent->savedEvent();

        // Update the end time and type to pretend that we're running
        // this saved event type.
        time2 = nextEvent->time();
        nextEventType = savedEvent->type();

        if (nextEvent->hasDefaultCancelledValue()) {
            // We've already established a value for the cancelled
            // event, so just return it.
            value2 = nextEvent->value();
        } else {
            // If the next event would have been a LinearRamp or
            // ExponentialRamp, we need to compute a new end value for
            // the event so that the curve works continues as if it were
            // not cancelled.
            switch (savedEvent->type()) {
            case ParamEvent::LinearRampToValue:
                value2 = linearRampAtTime(nextEvent->time(), value1, time1, savedEvent->value(), savedEvent->time());
                break;
            case ParamEvent::ExponentialRampToValue:
                value2 = exponentialRampAtTime(nextEvent->time(), value1, time1, savedEvent->value(), savedEvent->time());
                break;
            case ParamEvent::SetValueCurve:
            case ParamEvent::SetValue:
            case ParamEvent::SetTarget:
            case ParamEvent::CancelValues:
                // These cannot be possible types for the saved event because they can't be created.
                // createCancelValuesEvent doesn't allow them (SetValue, SetTarget, CancelValues) or
                // cancelScheduledValues() doesn't create such an event (SetValueCurve).
                ASSERT_NOT_REACHED();
                break;
            case ParamEvent::LastType:
                ASSERT_NOT_REACHED();
                break;
            }

            // Cache the new value so we don't keep computing it over and over.
            nextEvent->setCancelledValue(value2);
        }
    } break;
    case ParamEvent::SetValueCurve:
        // Everything needed for this was handled when cancelling was
        // done.
        break;
    case ParamEvent::SetTarget:
        // Nothing special needs to be done for SetTarget
        // followed by CancelValues.
        break;
    case ParamEvent::LastType:
        ASSERT_NOT_REACHED();
        break;
    }
}

auto AudioParamTimeline::ParamEvent::createSetValueEvent(float value, Seconds time) -> UniqueRef<ParamEvent>
{
    return makeUniqueRef<ParamEvent>(ParamEvent::SetValue, value, time, 0, Seconds { }, Vector<float> { }, 0, 0, nullptr);
}

auto AudioParamTimeline::ParamEvent::createLinearRampEvent(float value, Seconds time) -> UniqueRef<ParamEvent>
{
    return makeUniqueRef<ParamEvent>(ParamEvent::LinearRampToValue, value, time, 0, Seconds { }, Vector<float> { }, 0, 0, nullptr);
}

auto AudioParamTimeline::ParamEvent::createExponentialRampEvent(float value, Seconds time) -> UniqueRef<ParamEvent>
{
    return makeUniqueRef<ParamEvent>(ParamEvent::ExponentialRampToValue, value, time, 0, Seconds { }, Vector<float> { }, 0, 0, nullptr);
}

auto AudioParamTimeline::ParamEvent::createSetTargetEvent(float target, Seconds time, float timeConstant) -> UniqueRef<ParamEvent>
{
    // The time line code does not expect a timeConstant of 0. (It returns NaN or Infinity due to division by zero. The caller
    // should have converted this to a SetValueEvent.
    ASSERT(!!timeConstant);
    return makeUniqueRef<ParamEvent>(ParamEvent::SetTarget, target, time, timeConstant, Seconds { }, Vector<float> { }, 0, 0, nullptr);
}

auto AudioParamTimeline::ParamEvent::createSetValueCurveEvent(Vector<float>&& curve, Seconds time, Seconds duration) -> UniqueRef<ParamEvent>
{
    double curvePointsPerSecond = (curve.size() - 1) / duration.value();
    float curveEndValue = curve.last();
    return makeUniqueRef<ParamEvent>(ParamEvent::SetValueCurve, 0, time, 0, duration, WTFMove(curve), curvePointsPerSecond, curveEndValue, nullptr);
}

auto AudioParamTimeline::ParamEvent::createCancelValuesEvent(Seconds cancelTime, std::unique_ptr<ParamEvent> savedEvent) -> UniqueRef<ParamEvent>
{
#if ASSERT_ENABLED
    if (savedEvent) {
        // The savedEvent can only have certain event types. Verify that.
        auto savedEventType = savedEvent->type();

        ASSERT(savedEventType != ParamEvent::LastType);
        ASSERT(savedEventType == ParamEvent::LinearRampToValue
            || savedEventType == ParamEvent::ExponentialRampToValue
            || savedEventType == ParamEvent::SetValueCurve);
    }
#endif
    return makeUniqueRef<ParamEvent>(ParamEvent::CancelValues, 0, cancelTime, 0, Seconds { }, Vector<float> { }, 0, 0, WTFMove(savedEvent));
}

bool AudioParamTimeline::isEventCurrent(const ParamEvent& event, const ParamEvent* nextEvent, size_t currentFrame, double sampleRate) const
{
    // WARNING: due to round-off it might happen that nextEvent->time() is
    // just larger than currentFrame/sampleRate. This means that we will end
    // up running the |event| again. The code below had better be prepared
    // for this case! What should happen is the fillToFrame should be 0 so
    // that while the event is actually run again, nothing actually gets
    // computed, and we move on to the next event.
    //
    // An example of this case is setValueCurveAtTime. The time at which
    // setValueCurveAtTime ends (and the setValueAtTime begins) might be
    // just past currentTime/sampleRate. Then setValueCurveAtTime will be
    // processed again before advancing to setValueAtTime. The number of
    // frames to be processed should be zero in this case.
    if (nextEvent && nextEvent->time().value() < currentFrame / sampleRate) {
        // But if the current event is a SetValue event and the event time is
        // between currentFrame - 1 and curentFrame (in time). we don't want to
        // skip it. If we do skip it, the SetValue event is completely skipped
        // and not applied, which is wrong. Other events don't have this problem.
        // (Because currentFrame is unsigned, we do the time check in this funny,
        // but equivalent way.)
        double eventFrame = event.time().value() * sampleRate;

        // Condition is currentFrame - 1 < eventFrame <= currentFrame, but
        // currentFrame is unsigned and could be 0, so use
        // currentFrame < eventFrame + 1 instead.
        if (!((event.type() == ParamEvent::SetValue && (eventFrame <= currentFrame) && (currentFrame < eventFrame + 1)))) {
            // This is not the special SetValue event case, and nextEvent is
            // in the past. We can skip processing of this event since it's
            // in past.
            return false;
        }
    }
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
