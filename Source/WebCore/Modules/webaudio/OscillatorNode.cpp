/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "OscillatorNode.h"

#include "AudioNodeOutput.h"
#include "AudioParam.h"
#include "PeriodicWave.h"
#include "VectorMath.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace VectorMath;

WTF_MAKE_ISO_ALLOCATED_IMPL(OscillatorNode);

PeriodicWave* OscillatorNode::s_periodicWaveSine = nullptr;
PeriodicWave* OscillatorNode::s_periodicWaveSquare = nullptr;
PeriodicWave* OscillatorNode::s_periodicWaveSawtooth = nullptr;
PeriodicWave* OscillatorNode::s_periodicWaveTriangle = nullptr;

ExceptionOr<Ref<OscillatorNode>> OscillatorNode::create(BaseAudioContext& context, const OscillatorOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    context.lazyInitialize();

    if (options.type == OscillatorType::Custom && !options.periodicWave)
        return Exception { InvalidStateError, "Must provide periodicWave when using custom type."_s };
    
    auto oscillator = adoptRef(*new OscillatorNode(context, options));
    
    auto result = oscillator->setChannelCount(options.channelCount.valueOr(2));
    if (result.hasException())
        return result.releaseException();
    
    result = oscillator->setChannelCountMode(options.channelCountMode.valueOr(ChannelCountMode::Max));
    if (result.hasException())
        return result.releaseException();
    
    result = oscillator->setChannelInterpretation(options.channelInterpretation.valueOr(ChannelInterpretation::Speakers));
    if (result.hasException())
        return result.releaseException();
    
    if (options.periodicWave)
        oscillator->setPeriodicWave(*options.periodicWave);
    else {
        result = oscillator->setType(options.type);
        if (result.hasException())
            return result.releaseException();
    }

    // Because this is an AudioScheduledSourceNode, the context keeps a reference until it has finished playing.
    // When this happens, AudioScheduledSourceNode::finish() calls BaseAudioContext::notifyNodeFinishedProcessing().
    context.refNode(oscillator);

    return oscillator;
}

OscillatorNode::OscillatorNode(BaseAudioContext& context, const OscillatorOptions& options)
    : AudioScheduledSourceNode(context, context.sampleRate())
    , m_frequency(AudioParam::create(context, "frequency"_s, options.frequency, -context.sampleRate() / 2, context.sampleRate() / 2))
    , m_detune(AudioParam::create(context, "detune"_s, options.detune, -153600, 153600))
{
    setNodeType(NodeTypeOscillator);
    
    // An oscillator is always mono.
    addOutput(makeUnique<AudioNodeOutput>(this, 1));
    initialize();
}

OscillatorNode::~OscillatorNode()
{
    uninitialize();
}

ExceptionOr<void> OscillatorNode::setType(OscillatorType type)
{
    PeriodicWave* periodicWave = nullptr;

    ALWAYS_LOG(LOGIDENTIFIER, type);

    switch (type) {
    case OscillatorType::Sine:
        if (!s_periodicWaveSine)
            s_periodicWaveSine = &PeriodicWave::createSine(sampleRate()).leakRef();
        periodicWave = s_periodicWaveSine;
        break;
    case OscillatorType::Square:
        if (!s_periodicWaveSquare)
            s_periodicWaveSquare = &PeriodicWave::createSquare(sampleRate()).leakRef();
        periodicWave = s_periodicWaveSquare;
        break;
    case OscillatorType::Sawtooth:
        if (!s_periodicWaveSawtooth)
            s_periodicWaveSawtooth = &PeriodicWave::createSawtooth(sampleRate()).leakRef();
        periodicWave = s_periodicWaveSawtooth;
        break;
    case OscillatorType::Triangle:
        if (!s_periodicWaveTriangle)
            s_periodicWaveTriangle = &PeriodicWave::createTriangle(sampleRate()).leakRef();
        periodicWave = s_periodicWaveTriangle;
        break;
    case OscillatorType::Custom:
        if (m_type != OscillatorType::Custom)
            return Exception { InvalidStateError };
        return { };
    }

    setPeriodicWave(*periodicWave);
    m_type = type;

    return { };
}

bool OscillatorNode::calculateSampleAccuratePhaseIncrements(size_t framesToProcess)
{
    bool isGood = framesToProcess <= m_phaseIncrements.size() && framesToProcess <= m_detuneValues.size();
    ASSERT(isGood);
    if (!isGood)
        return false;

    if (m_firstRender) {
        m_firstRender = false;
        m_frequency->resetSmoothedValue();
        m_detune->resetSmoothedValue();
    }

    bool hasSampleAccurateValues = false;
    bool hasFrequencyChanges = false;
    float* phaseIncrements = m_phaseIncrements.data();

    float finalScale = m_periodicWave->rateScale();

    if (m_frequency->hasSampleAccurateValues()) {
        hasSampleAccurateValues = true;
        hasFrequencyChanges = true;

        // Get the sample-accurate frequency values and convert to phase increments.
        // They will be converted to phase increments below.
        m_frequency->calculateSampleAccurateValues(phaseIncrements, framesToProcess);
    } else {
        // Handle ordinary parameter smoothing/de-zippering if there are no scheduled changes.
        m_frequency->smooth();
        float frequency = m_frequency->smoothedValue();
        finalScale *= frequency;
    }

    if (m_detune->hasSampleAccurateValues()) {
        hasSampleAccurateValues = true;

        // Get the sample-accurate detune values.
        float* detuneValues = hasFrequencyChanges ? m_detuneValues.data() : phaseIncrements;
        m_detune->calculateSampleAccurateValues(detuneValues, framesToProcess);

        // Convert from cents to rate scalar.
        float k = 1.0 / 1200;
        vsmul(detuneValues, 1, &k, detuneValues, 1, framesToProcess);
        for (unsigned i = 0; i < framesToProcess; ++i)
            detuneValues[i] = powf(2, detuneValues[i]); // FIXME: converting to expf() will be faster.

        if (hasFrequencyChanges) {
            // Multiply frequencies by detune scalings.
            vmul(detuneValues, 1, phaseIncrements, 1, phaseIncrements, 1, framesToProcess);
        }
    } else {
        // Handle ordinary parameter smoothing/de-zippering if there are no scheduled changes.
        m_detune->smooth();
        float detune = m_detune->smoothedValue();
        float detuneScale = powf(2, detune / 1200);
        finalScale *= detuneScale;
    }

    if (hasSampleAccurateValues) {
        // Convert from frequency to wave increment.
        vsmul(phaseIncrements, 1, &finalScale, phaseIncrements, 1, framesToProcess);
    }

    return hasSampleAccurateValues;
}

void OscillatorNode::process(size_t framesToProcess)
{
    auto& outputBus = *output(0)->bus();

    if (!isInitialized() || !outputBus.numberOfChannels()) {
        outputBus.zero();
        return;
    }

    ASSERT(framesToProcess <= m_phaseIncrements.size());
    if (framesToProcess > m_phaseIncrements.size())
        return;

    // The audio thread can't block on this lock, so we use std::try_to_lock instead.
    std::unique_lock<Lock> lock(m_processMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        // Too bad - the try_lock() failed. We must be in the middle of changing wave-tables.
        outputBus.zero();
        return;
    }

    // We must access m_periodicWave only inside the lock.
    if (!m_periodicWave.get()) {
        outputBus.zero();
        return;
    }

    size_t quantumFrameOffset = 0;
    size_t nonSilentFramesToProcess = 0;
    updateSchedulingInfo(framesToProcess, outputBus, quantumFrameOffset, nonSilentFramesToProcess);

    if (!nonSilentFramesToProcess) {
        outputBus.zero();
        return;
    }

    unsigned periodicWaveSize = m_periodicWave->periodicWaveSize();
    double invPeriodicWaveSize = 1.0 / periodicWaveSize;

    float* destP = outputBus.channel(0)->mutableData();

    ASSERT(quantumFrameOffset <= framesToProcess);

    // We keep virtualReadIndex double-precision since we're accumulating values.
    double virtualReadIndex = m_virtualReadIndex;

    float rateScale = m_periodicWave->rateScale();
    float invRateScale = 1 / rateScale;
    bool hasSampleAccurateValues = calculateSampleAccuratePhaseIncrements(framesToProcess);

    float frequency = 0;
    float* higherWaveData = nullptr;
    float* lowerWaveData = nullptr;
    float tableInterpolationFactor = 0;

    if (!hasSampleAccurateValues) {
        frequency = m_frequency->smoothedValue();
        float detune = m_detune->smoothedValue();
        float detuneScale = powf(2, detune / 1200);
        frequency *= detuneScale;
        m_periodicWave->waveDataForFundamentalFrequency(frequency, lowerWaveData, higherWaveData, tableInterpolationFactor);
    }

    float incr = frequency * rateScale;
    float* phaseIncrements = m_phaseIncrements.data();

    unsigned readIndexMask = periodicWaveSize - 1;

    // Start rendering at the correct offset.
    destP += quantumFrameOffset;
    int n = nonSilentFramesToProcess;

    while (n--) {
        unsigned readIndex = static_cast<unsigned>(virtualReadIndex);
        unsigned readIndex2 = readIndex + 1;

        // Contain within valid range.
        readIndex = readIndex & readIndexMask;
        readIndex2 = readIndex2 & readIndexMask;

        if (hasSampleAccurateValues) {
            incr = *phaseIncrements++;

            frequency = invRateScale * incr;
            m_periodicWave->waveDataForFundamentalFrequency(frequency, lowerWaveData, higherWaveData, tableInterpolationFactor);
        }

        float sample1Lower = lowerWaveData[readIndex];
        float sample2Lower = lowerWaveData[readIndex2];
        float sample1Higher = higherWaveData[readIndex];
        float sample2Higher = higherWaveData[readIndex2];

        // Linearly interpolate within each table (lower and higher).
        float interpolationFactor = static_cast<float>(virtualReadIndex) - readIndex;
        float sampleHigher = (1 - interpolationFactor) * sample1Higher + interpolationFactor * sample2Higher;
        float sampleLower = (1 - interpolationFactor) * sample1Lower + interpolationFactor * sample2Lower;

        // Then interpolate between the two tables.
        float sample = (1 - tableInterpolationFactor) * sampleHigher + tableInterpolationFactor * sampleLower;

        *destP++ = sample;

        // Increment virtual read index and wrap virtualReadIndex into the range 0 -> periodicWaveSize.
        virtualReadIndex += incr;
        virtualReadIndex -= floor(virtualReadIndex * invPeriodicWaveSize) * periodicWaveSize;
    }

    m_virtualReadIndex = virtualReadIndex;

    outputBus.clearSilentFlag();
}

void OscillatorNode::reset()
{
    m_virtualReadIndex = 0;
}

void OscillatorNode::setPeriodicWave(PeriodicWave& periodicWave)
{
    ALWAYS_LOG(LOGIDENTIFIER, "sample rate = ", periodicWave.sampleRate(), ", wave size = ", periodicWave.periodicWaveSize(), ", rate scale = ", periodicWave.rateScale());
    ASSERT(isMainThread());
    
    // This synchronizes with process().
    auto locker = holdLock(m_processMutex);
    m_periodicWave = &periodicWave;
    m_type = OscillatorType::Custom;
}

bool OscillatorNode::propagatesSilence() const
{
    return !isPlayingOrScheduled() || hasFinished() || !m_periodicWave.get();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
