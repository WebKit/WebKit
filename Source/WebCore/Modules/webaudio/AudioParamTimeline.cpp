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
    return insertEvent(ParamEvent(ParamEvent::SetValue, value, time, 0, { }, { }));
}

ExceptionOr<void> AudioParamTimeline::linearRampToValueAtTime(float value, Seconds time)
{
    return insertEvent(ParamEvent(ParamEvent::LinearRampToValue, value, time, 0, { }, { }));
}

ExceptionOr<void> AudioParamTimeline::exponentialRampToValueAtTime(float value, Seconds time)
{
    return insertEvent(ParamEvent(ParamEvent::ExponentialRampToValue, value, time, 0, { }, { }));
}

ExceptionOr<void> AudioParamTimeline::setTargetAtTime(float target, Seconds time, float timeConstant)
{
    return insertEvent(ParamEvent(ParamEvent::SetTarget, target, time, timeConstant, { }, { }));
}

ExceptionOr<void> AudioParamTimeline::setValueCurveAtTime(Vector<float>&& curve, Seconds time, Seconds duration)
{
    return insertEvent(ParamEvent(ParamEvent::SetValueCurve, 0, time, 0, duration, WTFMove(curve)));
}

static bool isValidNumber(float x)
{
    return !std::isnan(x) && !std::isinf(x);
}

static bool isValidNumber(Seconds s)
{
    return !std::isnan(s.value()) && !std::isinf(s.value());
}

ExceptionOr<void> AudioParamTimeline::insertEvent(const ParamEvent& event)
{
    // Sanity check the event. Be super careful we're not getting infected with NaN or Inf.
    bool isValid = event.type() < ParamEvent::LastType
        && isValidNumber(event.value())
        && isValidNumber(event.time())
        && isValidNumber(event.timeConstant())
        && isValidNumber(event.duration())
        && event.duration() >= 0_s;

    if (!isValid)
        return { };
        
    auto locker = holdLock(m_eventsMutex);

    unsigned i = 0;
    auto insertTime = event.time();
    for (auto& paramEvent : m_events) {
        if (event.type() == ParamEvent::SetValueCurve) {
            // If this event is a SetValueCurve, make sure it doesn't overlap any existing event.
            // It's ok if the SetValueCurve starts at the same time as the end of some other duration.
            auto endTime = event.time() + event.duration();
            if (paramEvent.type() == ParamEvent::SetValueCurve) {
                auto paramEventEndTime = paramEvent.time() + paramEvent.duration();
                if ((paramEvent.time() >= event.time() && paramEvent.time() < endTime)
                    || (paramEventEndTime > event.time() && paramEventEndTime < endTime)
                    || (event.time() >= paramEvent.time() && event.time() < paramEventEndTime)
                    || (endTime >= paramEvent.time() && endTime < paramEventEndTime)) {
                    return Exception { NotSupportedError, "Events are overlapping"_s };
                }
            } else if (paramEvent.time() > event.time() && paramEvent.time() < endTime)
                return Exception { NotSupportedError, "Events are overlapping"_s };
        } else if (paramEvent.type() == ParamEvent::SetValueCurve) {
            // Otherwise, make sure this event doesn't overlap any existing SetValueCurve event.
            auto parentEventEndTime = paramEvent.time() + paramEvent.duration();
            if (event.time() >= paramEvent.time() && event.time() < parentEventEndTime)
                return Exception { NotSupportedError, "Events are overlapping" };
        }

        if (paramEvent.time() > insertTime)
            break;

        ++i;
    }

    m_events.insert(i, event);
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

Optional<float> AudioParamTimeline::valueForContextTime(BaseAudioContext& context, float defaultValue, float minValue, float maxValue)
{
    {
        std::unique_lock<Lock> lock(m_eventsMutex, std::try_to_lock);
        if (!lock.owns_lock() || !m_events.size() || Seconds { context.currentTime() } < m_events[0].time())
            return WTF::nullopt;
    }

    // Ask for just a single value.
    float value;
    double sampleRate = context.sampleRate();
    Seconds startTime = Seconds { context.currentTime() };
    Seconds endTime = startTime + Seconds { 1.1 / sampleRate }; // time just beyond one sample-frame
    double controlRate = sampleRate / AudioNode::ProcessingSizeInFrames; // one parameter change per render quantum
    value = valuesForTimeRange(startTime, endTime, defaultValue, minValue, maxValue, &value, 1, sampleRate, controlRate);
    return value;
}

float AudioParamTimeline::valuesForTimeRange(Seconds startTime, Seconds endTime, float defaultValue, float minValue, float maxValue, float* values, unsigned numberOfValues, double sampleRate, double controlRate)
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

    float value = valuesForTimeRangeImpl(startTime, endTime, defaultValue, values, numberOfValues, sampleRate, controlRate);

    // Clamp values based on range allowed by AudioParam's min and max values.
    VectorMath::vclip(values, 1, &minValue, &maxValue, values, 1, numberOfValues);

    return value;
}

float AudioParamTimeline::valuesForTimeRangeImpl(Seconds startTime, Seconds endTime, float defaultValue, float* values, unsigned numberOfValues, double sampleRate, double controlRate)
{
    ASSERT(values);
    if (!values)
        return defaultValue;

    // Return default value if there are no events matching the desired time range.
    if (!m_events.size() || endTime <= m_events[0].time()) {
        for (unsigned i = 0; i < numberOfValues; ++i)
            values[i] = defaultValue;
        return defaultValue;
    }

    // Maintain a running time and index for writing the values buffer.
    auto currentTime = startTime;
    unsigned writeIndex = 0;

    // If first event is after startTime then fill initial part of values buffer with defaultValue
    // until we reach the first event time.
    auto firstEventTime = m_events[0].time();
    if (firstEventTime > startTime) {
        auto fillToTime = std::min(endTime, firstEventTime);
        unsigned fillToFrame = AudioUtilities::timeToSampleFrame((fillToTime - startTime).value(), sampleRate);
        fillToFrame = std::min(fillToFrame, numberOfValues);
        for (; writeIndex < fillToFrame; ++writeIndex)
            values[writeIndex] = defaultValue;

        currentTime = fillToTime;
    }

    float value = defaultValue;

    // Go through each event and render the value buffer where the times overlap,
    // stopping when we've rendered all the requested values.
    // FIXME: could try to optimize by avoiding having to iterate starting from the very first event
    // and keeping track of a "current" event index.
    int n = m_events.size();
    for (int i = 0; i < n && writeIndex < numberOfValues; ++i) {
        ParamEvent& event = m_events[i];
        ParamEvent* nextEvent = i < n - 1 ? &(m_events[i + 1]) : nullptr;

        // Wait until we get a more recent event.
        if (nextEvent && nextEvent->time() < currentTime)
            continue;

        float value1 = event.value();
        auto time1 = event.time();
        float value2 = nextEvent ? nextEvent->value() : value1;
        auto time2 = nextEvent ? nextEvent->time() : endTime + 1_s;

        auto deltaTime = time2 - time1;
        auto sampleFrameTimeIncr = Seconds { 1 / sampleRate };

        auto fillToTime = std::min(endTime, time2);
        unsigned fillToFrame = AudioUtilities::timeToSampleFrame((fillToTime - startTime).value(), sampleRate);
        fillToFrame = std::min(fillToFrame, numberOfValues);

        ParamEvent::Type nextEventType = nextEvent ? static_cast<ParamEvent::Type>(nextEvent->type()) : ParamEvent::LastType /* unknown */;

        // First handle linear and exponential ramps which require looking ahead to the next event.
        if (nextEventType == ParamEvent::LinearRampToValue) {
            for (; writeIndex < fillToFrame; ++writeIndex) {
                // Since deltaTime is a double, 1/deltaTime can easily overflow a float. Thus, if deltaTime
                // is close enough to zero (less than float min), treat it as zero.
                float k = deltaTime.value() <= std::numeric_limits<float>::min() ? 0 : 1 / deltaTime.value();
                float x = static_cast<float>((currentTime - time1).value()) * k;
                float valueDelta = value2 - value1;
                value = value1 + valueDelta * x;
                values[writeIndex] = value;
                currentTime += sampleFrameTimeIncr;
            }
        } else if (nextEventType == ParamEvent::ExponentialRampToValue) {
            if (value1 <= 0 || value2 <= 0) {
                // Handle negative values error case by propagating previous value.
                for (; writeIndex < fillToFrame; ++writeIndex)
                    values[writeIndex] = value;
            } else {
                float numSampleFrames = deltaTime.value() * sampleRate;
                // The value goes exponentially from value1 to value2 in a duration of deltaTime seconds (corresponding to numSampleFrames).
                // Compute the per-sample multiplier.
                float multiplier = powf(value2 / value1, 1 / numSampleFrames);

                // Set the starting value of the exponential ramp. This is the same as multiplier ^
                // AudioUtilities::timeToSampleFrame(currentTime - time1, sampleRate), but is more
                // accurate, especially if multiplier is close to 1.
                value = value1 * powf(value2 / value1,
                                      AudioUtilities::timeToSampleFrame((currentTime - time1).value(), sampleRate) / numSampleFrames);

                for (; writeIndex < fillToFrame; ++writeIndex) {
                    values[writeIndex] = value;
                    value *= multiplier;
                    currentTime += sampleFrameTimeIncr;
                }
            }
        } else {
            // Handle event types not requiring looking ahead to the next event.
            switch (event.type()) {
            case ParamEvent::SetValue:
            case ParamEvent::LinearRampToValue:
            case ParamEvent::ExponentialRampToValue:
                {
                    currentTime = fillToTime;

                    // Simply stay at a constant value.
                    value = event.value();
                    for (; writeIndex < fillToFrame; ++writeIndex)
                        values[writeIndex] = value;

                    break;
                }

            case ParamEvent::SetTarget:
                {
                    currentTime = fillToTime;

                    // Exponential approach to target value with given time constant.
                    float target = event.value();
                    float timeConstant = event.timeConstant();
                    float discreteTimeConstant = static_cast<float>(AudioUtilities::discreteTimeConstantForSampleRate(timeConstant, controlRate));

                    // If the value is close enough to the target, just fill in the data
                    // with the target value.
                    if (hasSetTargetConverged(value, target, currentTime, time1, timeConstant)) {
                        for (; writeIndex < fillToFrame; ++writeIndex)
                            values[writeIndex] = target;
                        value = target;
                    } else {
                        // Serially process remaining values.
                        for (; writeIndex < fillToFrame; ++writeIndex) {
                            values[writeIndex] = value;
                            value += (target - value) * discreteTimeConstant;
                        }
                    }

                    break;
                }

            case ParamEvent::SetValueCurve:
                {
                    float* curveData = event.curve().data();
                    unsigned numberOfCurvePoints = event.curve().size();

                    // Curve events have duration, so don't just use next event time.
                    auto duration = event.duration();
                    auto durationFrames = duration.value() * sampleRate;
                    float curvePointsPerFrame = static_cast<float>(numberOfCurvePoints) / durationFrames;

                    if (!curveData || !numberOfCurvePoints || duration <= 0_s || sampleRate <= 0) {
                        // Error condition - simply propagate previous value.
                        currentTime = fillToTime;
                        for (; writeIndex < fillToFrame; ++writeIndex)
                            values[writeIndex] = value;
                        break;
                    }

                    // Save old values and recalculate information based on the curve's duration
                    // instead of the next event time.
                    unsigned nextEventFillToFrame = fillToFrame;
                    auto nextEventFillToTime = fillToTime;
                    fillToTime = std::min(endTime, time1 + duration);
                    fillToFrame = AudioUtilities::timeToSampleFrame((fillToTime - startTime).value(), sampleRate);
                    fillToFrame = std::min(fillToFrame, numberOfValues);

                    // Index into the curve data using a floating-point value.
                    // We're scaling the number of curve points by the duration (see curvePointsPerFrame).
                    float curveVirtualIndex = 0;
                    if (time1 < currentTime) {
                        // Index somewhere in the middle of the curve data.
                        // Don't use timeToSampleFrame() since we want the exact floating-point frame.
                        float frameOffset = (currentTime - time1).value() * sampleRate;
                        curveVirtualIndex = curvePointsPerFrame * frameOffset;
                    }

                    // Render the stretched curve data using nearest neighbor sampling.
                    // Oversampled curve data can be provided if smoothness is desired.
                    for (; writeIndex < fillToFrame; ++writeIndex) {
                        // Ideally we'd use round() from MathExtras, but we're in a tight loop here
                        // and we're trading off precision for extra speed.
                        unsigned curveIndex = static_cast<unsigned>(0.5 + curveVirtualIndex);

                        curveVirtualIndex += curvePointsPerFrame;

                        // Bounds check.
                        if (curveIndex < numberOfCurvePoints)
                            value = curveData[curveIndex];

                        values[writeIndex] = value;
                    }

                    // If there's any time left after the duration of this event and the start
                    // of the next, then just propagate the last value.
                    for (; writeIndex < nextEventFillToFrame; ++writeIndex)
                        values[writeIndex] = value;

                    // Re-adjust current time
                    currentTime = nextEventFillToTime;

                    break;
                }
            }
        }
    }

    // If there's any time left after processing the last event then just propagate the last value
    // to the end of the values buffer.
    for (; writeIndex < numberOfValues; ++writeIndex)
        values[writeIndex] = value;

    return value;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
