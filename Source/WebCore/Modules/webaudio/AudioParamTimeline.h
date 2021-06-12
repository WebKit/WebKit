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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#pragma once

#include "AudioContext.h"
#include <JavaScriptCore/Float32Array.h>
#include <wtf/Lock.h>
#include <wtf/Vector.h>

namespace WebCore {

class AudioParamTimeline {
    WTF_MAKE_NONCOPYABLE(AudioParamTimeline);
    WTF_MAKE_FAST_ALLOCATED;
public:
    AudioParamTimeline() = default;

    ExceptionOr<void> setValueAtTime(float value, Seconds time);
    ExceptionOr<void> linearRampToValueAtTime(float targetValue, Seconds endTime, float currentValue, Seconds currentTime);
    ExceptionOr<void> exponentialRampToValueAtTime(float targetValue, Seconds endTime, float currentValue, Seconds currentTime);
    ExceptionOr<void> setTargetAtTime(float target, Seconds time, float timeConstant);
    ExceptionOr<void> setValueCurveAtTime(Vector<float>&& curve, Seconds time, Seconds duration);
    void cancelScheduledValues(Seconds cancelTime);
    ExceptionOr<void> cancelAndHoldAtTime(Seconds cancelTime);

    // hasValue is set to true if a valid timeline value is returned.
    // otherwise defaultValue is returned.
    std::optional<float> valueForContextTime(BaseAudioContext&, float defaultValue, float minValue, float maxValue);

    // Given the time range, calculates parameter values into the values buffer
    // and returns the last parameter value calculated for "values" or the defaultValue if none were calculated.
    // controlRate is the rate (number per second) at which parameter values will be calculated.
    // It should equal sampleRate for sample-accurate parameter changes, and otherwise will usually match
    // the render quantum size such that the parameter value changes once per render quantum.
    float valuesForFrameRange(size_t startFrame, size_t endFrame, float defaultValue, float minValue, float maxValue, float* values, unsigned numberOfValues, double sampleRate, double controlRate);

    bool hasValues(size_t startFrame, double sampleRate) const;

private:
    class ParamEvent {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        enum Type {
            SetValue,
            LinearRampToValue,
            ExponentialRampToValue,
            SetTarget,
            SetValueCurve,
            CancelValues,
            LastType
        };

        struct SavedEvent {
            Type type;
            float value;
            Seconds time;
        };

        static ParamEvent createSetValueEvent(float value, Seconds time);
        static ParamEvent createLinearRampEvent(float value, Seconds time);
        static ParamEvent createExponentialRampEvent(float value, Seconds time);
        static ParamEvent createSetTargetEvent(float target, Seconds time, float timeConstant);
        static ParamEvent createSetValueCurveEvent(Vector<float>&& curve, Seconds time, Seconds duration);
        static ParamEvent createCancelValuesEvent(Seconds cancelTime, std::optional<SavedEvent>&&);

        ParamEvent(Type type, float value, Seconds time, float timeConstant, Seconds duration, Vector<float>&& curve, double curvePointsPerSecond, float curveEndValue, std::optional<SavedEvent>&& savedEvent)
            : m_type(type)
            , m_value(value)
            , m_time(time)
            , m_timeConstant(timeConstant)
            , m_duration(duration)
            , m_curve(WTFMove(curve))
            , m_curvePointsPerSecond(curvePointsPerSecond)
            , m_curveEndValue(curveEndValue)
            , m_savedEvent(WTFMove(savedEvent))
        {
        }

        Type type() const { return m_type; }
        float value() const { return m_value; }
        Seconds time() const { return m_time; }
        float timeConstant() const { return m_timeConstant; }
        Seconds duration() const { return m_duration; }
        const Vector<float>& curve() const { return m_curve; }
        SavedEvent* savedEvent() { return m_savedEvent ? &m_savedEvent.value() : nullptr; }

        void setCancelledValue(float cancelledValue)
        {
            ASSERT(m_type == Type::CancelValues);
            m_value = cancelledValue;
            m_hasDefaultCancelledValue = true;
        }
        bool hasDefaultCancelledValue() const
        {
            ASSERT(m_type == Type::CancelValues);
            return m_hasDefaultCancelledValue;
        }

        double curvePointsPerSecond() const { return m_curvePointsPerSecond; }
        float curveEndValue() const { return m_curveEndValue; }

    private:
        Type m_type;
        float m_value { 0 };
        Seconds m_time;

        // Only used for SetTarget events.
        float m_timeConstant { 0 };

        // The duration of the curve.
        Seconds m_duration;

        // The array of curve points.
        Vector<float> m_curve;

        // The number of curve points per second. it is used to compute
        // the curve index step when running the automation.
        double m_curvePointsPerSecond { 0 };

        // The default value to use at the end of the curve. Normally
        // it's the last entry in m_curve, but cancelling a SetValueCurve
        // will set this to a new value.
        float m_curveEndValue { 0 };

        // True if a default value has been assigned to the CancelValues event.
        bool m_hasDefaultCancelledValue { false };

        // For CancelValues. If CancelValues is in the middle of an event, this
        // holds the event that is being cancelled, so that processing can
        // continue as if the event still existed up until we reach the actual
        // scheduled cancel time.
        std::optional<SavedEvent> m_savedEvent;
    };

    // State of the timeline for the current event.
    struct AutomationState {
        // Parameters for the current automation request. Number of
        // values to be computed for the automation request
        const unsigned numberOfValues;
        // Start and end frames for this automation request
        const size_t startFrame;
        const size_t endFrame;

        // Sample rate and control rate for this request
        const double sampleRate;
        const double controlRate;
        const double samplingPeriod;

        // Parameters needed for processing the current event.
        const unsigned fillToFrame;
        const size_t fillToEndFrame;

        // Value and time for the current event
        const float value1;
        const Seconds time1;

        // Value and time for the next event, if any.
        const float value2;
        const Seconds time2;

        // The current event, and it's index in the event vector.
        const ParamEvent* event;
        const int eventIndex;
    };

    void removeCancelledEvents(size_t firstEventToRemove) WTF_REQUIRES_LOCK(m_eventsLock);
    ExceptionOr<void> insertEvent(ParamEvent&&) WTF_REQUIRES_LOCK(m_eventsLock);
    float valuesForFrameRangeImpl(size_t startFrame, size_t endFrame, float defaultValue, float* values, unsigned numberOfValues, double sampleRate, double controlRate) WTF_REQUIRES_LOCK(m_eventsLock);
    float linearRampAtTime(Seconds t, float value1, Seconds time1, float value2, Seconds time2);
    float exponentialRampAtTime(Seconds t, float value1, Seconds time1, float value2, Seconds time2);
    float valueCurveAtTime(Seconds t, Seconds time1, Seconds duration, const float* curveData, size_t curveLength);
    void handleCancelValues(ParamEvent&, ParamEvent* nextEvent, float& value2, Seconds& time2, ParamEvent::Type& nextEventType);
    bool isEventCurrent(const ParamEvent&, const ParamEvent* nextEvent, size_t currentFrame, double sampleRate) const;

    void processLinearRamp(const AutomationState&, float* values, size_t& currentFrame, float& value, unsigned& writeIndex);
    void processExponentialRamp(const AutomationState&, float* values, size_t& currentFrame, float& value, unsigned& writeIndex);
    void processCancelValues(const AutomationState&, float* values, size_t& currentFrame, float& value, unsigned& writeIndex) WTF_REQUIRES_LOCK(m_eventsLock);
    void processSetTarget(const AutomationState&, float* values, size_t& currentFrame, float& value, unsigned& writeIndex);
    void processSetValueCurve(const AutomationState&, float* values, size_t& currentFrame, float& value, unsigned& writeIndex);
    void processSetTargetFollowedByRamp(int eventIndex, ParamEvent*&, ParamEvent::Type nextEventType, size_t currentFrame, double samplingPeriod, double controlRate, float& value) WTF_REQUIRES_LOCK(m_eventsLock);

    Vector<ParamEvent> m_events WTF_GUARDED_BY_LOCK(m_eventsLock);

    mutable Lock m_eventsLock;
};

} // namespace WebCore
