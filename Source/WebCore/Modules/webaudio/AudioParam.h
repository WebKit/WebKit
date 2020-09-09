/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "AudioParamTimeline.h"
#include "AudioSummingJunction.h"
#include "WebKitAudioContext.h"
#include <JavaScriptCore/Float32Array.h>
#include <sys/types.h>
#include <wtf/LoggerHelper.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AudioNodeOutput;

enum class AutomationRate : bool { ARate, KRate };
enum class AutomationRateMode : bool { Fixed, Variable };

class AudioParam final
    : public AudioSummingJunction
    , public RefCounted<AudioParam>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static constexpr double SmoothingConstant = 0.05;
    static constexpr double SnapThreshold = 0.001;

    static Ref<AudioParam> create(BaseAudioContext& context, const String& name, float defaultValue, float minValue, float maxValue, AutomationRate automationRate, AutomationRateMode automationRateMode = AutomationRateMode::Variable)
    {
        return adoptRef(*new AudioParam(context, name, defaultValue, minValue, maxValue, automationRate, automationRateMode));
    }

    // AudioSummingJunction
    bool canUpdateState() override { return true; }
    void didUpdate() override { }

    // Intrinsic value.
    float value();
    void setValue(float);

    float valueForBindings() const;
    ExceptionOr<void> setValueForBindings(float);

    AutomationRate automationRate() const { return m_automationRate; }
    ExceptionOr<void> setAutomationRate(AutomationRate);

    // Final value for k-rate parameters, otherwise use calculateSampleAccurateValues() for a-rate.
    // Must be called in the audio thread.
    float finalValue();

    String name() const { return m_name; }

    float minValue() const { return m_minValue; }
    float maxValue() const { return m_maxValue; }
    float defaultValue() const { return m_defaultValue; }

    // Value smoothing:

    // When a new value is set with setValue(), in our internal use of the parameter we don't immediately jump to it.
    // Instead we smoothly approach this value to avoid glitching.
    float smoothedValue();

    // Smoothly exponentially approaches to (de-zippers) the desired value.
    // Returns true if smoothed value has already snapped exactly to value.
    bool smooth();

    void resetSmoothedValue() { m_smoothedValue = m_value; }

    // Parameter automation.    
    ExceptionOr<AudioParam&> setValueAtTime(float value, double startTime);
    ExceptionOr<AudioParam&> linearRampToValueAtTime(float value, double endTime);
    ExceptionOr<AudioParam&> exponentialRampToValueAtTime(float value, double endTime);
    ExceptionOr<AudioParam&> setTargetAtTime(float target, double startTime, float timeConstant);
    ExceptionOr<AudioParam&> setValueCurveAtTime(Vector<float>&& curve, double startTime, double duration);
    ExceptionOr<AudioParam&> cancelScheduledValues(double cancelTime);
    ExceptionOr<AudioParam&> cancelAndHoldAtTime(double cancelTime);

    bool hasSampleAccurateValues() const { return m_timeline.hasValues() || numberOfRenderingConnections(); }
    
    // Calculates numberOfValues parameter values starting at the context's current time.
    // Must be called in the context's render thread.
    void calculateSampleAccurateValues(float* values, unsigned numberOfValues);

    // Connect an audio-rate signal to control this parameter.
    void connect(AudioNodeOutput*);
    void disconnect(AudioNodeOutput*);

protected:
    AudioParam(BaseAudioContext&, const String&, float defaultValue, float minValue, float maxValue, AutomationRate, AutomationRateMode);

private:
    // sampleAccurate corresponds to a-rate (audio rate) vs. k-rate in the Web Audio specification.
    void calculateFinalValues(float* values, unsigned numberOfValues, bool sampleAccurate);
    void calculateTimelineValues(float* values, unsigned numberOfValues);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "AudioParam"; }
    WTFLogChannel& logChannel() const final;
#endif
    
    String m_name;
    float m_value;
    float m_defaultValue;
    float m_minValue;
    float m_maxValue;
    AutomationRate m_automationRate;
    AutomationRateMode m_automationRateMode;

    // Smoothing (de-zippering)
    float m_smoothedValue;
    
    AudioParamTimeline m_timeline;

#if !RELEASE_LOG_DISABLED
    mutable Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

} // namespace WebCore
