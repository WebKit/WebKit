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

#include "AudioParam.h"

#include "AudioNode.h"
#include "AudioNodeOutput.h"
#include "AudioUtilities.h"
#include "FloatConversion.h"
#include "Logging.h"
#include "VectorMath.h"
#include <wtf/MathExtras.h>

namespace WebCore {

const double AudioParam::DefaultSmoothingConstant = 0.05;
const double AudioParam::SnapThreshold = 0.001;

AudioParam::AudioParam(BaseAudioContext& context, const String& name, float defaultValue, float minValue, float maxValue, AutomationRate automationRate, AutomationRateMode automationRateMode)
    : AudioSummingJunction(context)
    , m_name(name)
    , m_value(defaultValue)
    , m_defaultValue(defaultValue)
    , m_minValue(minValue)
    , m_maxValue(maxValue)
    , m_automationRate(automationRate)
    , m_automationRateMode(automationRateMode)
    , m_smoothedValue(defaultValue)
    , m_smoothingConstant(DefaultSmoothingConstant)
#if !RELEASE_LOG_DISABLED
    , m_logger(context.logger())
    , m_logIdentifier(context.nextAudioParameterLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER, "name = ", m_name, ", value = ", m_value, ", default = ", m_defaultValue, ", min = ", m_minValue, ", max = ", m_maxValue);
}

float AudioParam::value()
{
    // Update value for timeline.
    if (context().isAudioThread()) {
        auto timelineValue = m_timeline.valueForContextTime(context(), m_value, minValue(), maxValue());
        if (timelineValue)
            m_value = *timelineValue;
    }

    return m_value;
}

void AudioParam::setValue(float value)
{
    DEBUG_LOG(LOGIDENTIFIER, value);

    m_value = std::clamp(value, minValue(), maxValue());
}

float AudioParam::valueForBindings() const
{
    ASSERT(isMainThread());
    return m_value;
}

ExceptionOr<void> AudioParam::setValueForBindings(float value)
{
    ASSERT(isMainThread());

    setValue(value);
    auto result = setValueAtTime(m_value, context().currentTime());
    if (result.hasException())
        return result.releaseException();
    return { };
}

ExceptionOr<void> AudioParam::setAutomationRate(AutomationRate automationRate)
{
    if (m_automationRateMode == AutomationRateMode::Fixed)
        return Exception { InvalidStateError, "automationRate cannot be changed for this node" };

    m_automationRate = automationRate;
    return { };
}

float AudioParam::smoothedValue()
{
    return m_smoothedValue;
}

bool AudioParam::smooth()
{
    // If values have been explicitly scheduled on the timeline, then use the exact value.
    // Smoothing effectively is performed by the timeline.
    auto timelineValue = m_timeline.valueForContextTime(context(), m_value, minValue(), maxValue());
    if (timelineValue)
        m_value = *timelineValue;

    if (m_smoothedValue == m_value) {
        // Smoothed value has already approached and snapped to value.
        return true;
    }
    
    if (timelineValue)
        m_smoothedValue = m_value;
    else {
        // Dezipper - exponential approach.
        m_smoothedValue += (m_value - m_smoothedValue) * m_smoothingConstant;

        // If we get close enough then snap to actual value.
        if (fabs(m_smoothedValue - m_value) < SnapThreshold) // FIXME: the threshold needs to be adjustable depending on range - but this is OK general purpose value.
            m_smoothedValue = m_value;
    }

    return false;
}

ExceptionOr<AudioParam&> AudioParam::setValueAtTime(float value, double startTime)
{
    if (startTime < 0)
        return Exception { RangeError, "startTime must be a positive value"_s };

    auto result = m_timeline.setValueAtTime(value, Seconds { startTime });
    if (result.hasException())
        return result.releaseException();
    return *this;
}

ExceptionOr<AudioParam&> AudioParam::linearRampToValueAtTime(float value, double endTime)
{
    if (endTime < 0)
        return Exception { RangeError, "endTime must be a positive value"_s };

    auto result = m_timeline.linearRampToValueAtTime(value, Seconds { endTime });
    if (result.hasException())
        return result.releaseException();
    return *this;
}

ExceptionOr<AudioParam&> AudioParam::exponentialRampToValueAtTime(float value, double endTime)
{
    if (!value)
        return Exception { RangeError, "value cannot be 0"_s };
    if (endTime < 0)
        return Exception { RangeError, "endTime must be a positive value"_s };

    auto result = m_timeline.exponentialRampToValueAtTime(value, Seconds { endTime });
    if (result.hasException())
        return result.releaseException();
    return *this;
}

ExceptionOr<AudioParam&> AudioParam::setTargetAtTime(float target, double startTime, float timeConstant)
{
    if (startTime < 0)
        return Exception { RangeError, "startTime must be a positive value"_s };
    if (timeConstant < 0)
        return Exception { RangeError, "timeConstant must be a positive value"_s };

    auto result = m_timeline.setTargetAtTime(target, Seconds { startTime }, timeConstant);
    if (result.hasException())
        return result.releaseException();
    return *this;
}

ExceptionOr<AudioParam&> AudioParam::setValueCurveAtTime(Vector<float>&& curve, double startTime, double duration)
{
    if (curve.size() < 2)
        return Exception { InvalidStateError, "Array must have a length of at least 2"_s };
    if (startTime < 0)
        return Exception { RangeError, "startTime must be a positive value"_s };
    if (duration <= 0)
        return Exception { RangeError, "duration must be a strictly positive value"_s };

    auto result = m_timeline.setValueCurveAtTime(WTFMove(curve), Seconds { startTime }, Seconds { duration });
    if (result.hasException())
        return result.releaseException();
    return *this;
}

ExceptionOr<AudioParam&> AudioParam::cancelScheduledValues(double cancelTime)
{
    if (cancelTime < 0)
        return Exception { RangeError, "cancelTime must be a positive value"_s };

    m_timeline.cancelScheduledValues(Seconds { cancelTime });
    return *this;
}

float AudioParam::finalValue()
{
    float value;
    calculateFinalValues(&value, 1, false);
    return value;
}

void AudioParam::calculateSampleAccurateValues(float* values, unsigned numberOfValues)
{
    bool isSafe = context().isAudioThread() && values && numberOfValues;
    ASSERT(isSafe);
    if (!isSafe)
        return;

    calculateFinalValues(values, numberOfValues, automationRate() == AutomationRate::ARate);
}

void AudioParam::calculateFinalValues(float* values, unsigned numberOfValues, bool sampleAccurate)
{
    bool isGood = context().isAudioThread() && values && numberOfValues;
    ASSERT(isGood);
    if (!isGood)
        return;

    // The calculated result will be the "intrinsic" value summed with all audio-rate connections.

    if (sampleAccurate) {
        // Calculate sample-accurate (a-rate) intrinsic values.
        calculateTimelineValues(values, numberOfValues);
    } else {
        // Calculate control-rate (k-rate) intrinsic value.
        auto timelineValue = m_timeline.valueForContextTime(context(), m_value, minValue(), maxValue());

        if (timelineValue)
            m_value = *timelineValue;

        values[0] = m_value;
    }

    // Now sum all of the audio-rate connections together (unity-gain summing junction).
    // Note that connections would normally be mono, but we mix down to mono if necessary.
    auto summingBus = AudioBus::create(1, numberOfValues, false);
    summingBus->setChannelMemory(0, values, numberOfValues);

    for (auto& output : m_renderingOutputs) {
        ASSERT(output);

        // Render audio from this output.
        AudioBus* connectionBus = output->pull(0, AudioNode::ProcessingSizeInFrames);

        // Sum, with unity-gain.
        summingBus->sumFrom(*connectionBus);
    }

    // Clamp values based on range allowed by AudioParam's min and max values.
    float minValue = this->minValue();
    float maxValue = this->maxValue();
    VectorMath::vclip(values, 1, &minValue, &maxValue, values, 1, numberOfValues);
}

void AudioParam::calculateTimelineValues(float* values, unsigned numberOfValues)
{
    // Calculate values for this render quantum.
    // Normally numberOfValues will equal AudioNode::ProcessingSizeInFrames (the render quantum size).
    double sampleRate = context().sampleRate();
    Seconds startTime = Seconds { context().currentTime() };
    Seconds endTime = startTime + Seconds { numberOfValues / sampleRate };

    // Note we're running control rate at the sample-rate.
    // Pass in the current value as default value.
    m_value = m_timeline.valuesForTimeRange(startTime, endTime, m_value, minValue(), maxValue(), values, numberOfValues, sampleRate, sampleRate);
}

void AudioParam::connect(AudioNodeOutput* output)
{
    ASSERT(context().isGraphOwner());

    ASSERT(output);
    if (!output)
        return;

    if (!m_outputs.add(output).isNewEntry)
        return;

    INFO_LOG(LOGIDENTIFIER, output->node()->nodeType());

    output->addParam(this);
    changedOutputs();
}

void AudioParam::disconnect(AudioNodeOutput* output)
{
    ASSERT(context().isGraphOwner());

    ASSERT(output);
    if (!output)
        return;

    INFO_LOG(LOGIDENTIFIER, output->node()->nodeType());

    if (m_outputs.remove(output)) {
        changedOutputs();
        output->removeParam(this);
    }
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& AudioParam::logChannel() const
{
    return LogMedia;
}
#endif
    

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
