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

// Breakpoints where we deicde to do linear interoplation, 3-point interpolation or 5-point interpolation. See doInterpolation().
constexpr float interpolate2Point = 0.3;
constexpr float interpolate3Point = 0.16;

// Convert the detune value (in cents) to a frequency scale multiplier: 2^(d/1200).
static inline float detuneToFrequencyMultiplier(float detuneValue)
{
    return std::exp2(detuneValue / 1200);
}

// Clamp the frequency value to lie within Nyquist frequency. For NaN, arbitrarily clamp to +Nyquist.
static void clampFrequency(float* frequency, size_t framesToProcess, float nyquist)
{
    for (size_t k = 0; k < framesToProcess; ++k) {
        float f = frequency[k];
        frequency[k] = std::isnan(f) ? nyquist : clampTo(f, -nyquist, nyquist);
    }
}

ExceptionOr<Ref<OscillatorNode>> OscillatorNode::create(BaseAudioContext& context, const OscillatorOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    context.lazyInitialize();

    if (options.type == OscillatorType::Custom && !options.periodicWave)
        return Exception { InvalidStateError, "Must provide periodicWave when using custom type."_s };
    
    auto oscillator = adoptRef(*new OscillatorNode(context, options));
    
    auto result = oscillator->handleAudioNodeOptions(options, { 2, ChannelCountMode::Max, ChannelInterpretation::Speakers });
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
    : AudioScheduledSourceNode(context)
    , m_frequency(AudioParam::create(context, "frequency"_s, options.frequency, -context.sampleRate() / 2, context.sampleRate() / 2, AutomationRate::ARate))
    , m_detune(AudioParam::create(context, "detune"_s, options.detune, -1200 * log2f(std::numeric_limits<float>::max()), 1200 * log2f(std::numeric_limits<float>::max()), AutomationRate::ARate))
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
    ALWAYS_LOG(LOGIDENTIFIER, type);

    if (type == OscillatorType::Custom) {
        if (m_type != OscillatorType::Custom)
            return Exception { InvalidStateError, "OscillatorNode.type cannot be changed to 'custom'"_s };
        return { };
    }

    setPeriodicWave(context().periodicWave(type));
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

    if (m_frequency->hasSampleAccurateValues() && m_frequency->automationRate() == AutomationRate::ARate) {
        hasSampleAccurateValues = true;
        hasFrequencyChanges = true;

        // Get the sample-accurate frequency values and convert to phase increments.
        // They will be converted to phase increments below.
        m_frequency->calculateSampleAccurateValues(phaseIncrements, framesToProcess);
    } else {
        float frequency = m_frequency->finalValue();
        finalScale *= frequency;
    }

    if (m_detune->hasSampleAccurateValues() && m_detune->automationRate() == AutomationRate::ARate) {
        hasSampleAccurateValues = true;

        // Get the sample-accurate detune values.
        float* detuneValues = hasFrequencyChanges ? m_detuneValues.data() : phaseIncrements;
        m_detune->calculateSampleAccurateValues(detuneValues, framesToProcess);

        // Convert from cents to rate scalar.
        float k = 1.0 / 1200;
        vsmul(detuneValues, 1, &k, detuneValues, 1, framesToProcess);
        for (unsigned i = 0; i < framesToProcess; ++i)
            detuneValues[i] = std::exp2(detuneValues[i]);

        if (hasFrequencyChanges) {
            // Multiply frequencies by detune scalings.
            vmul(detuneValues, 1, phaseIncrements, 1, phaseIncrements, 1, framesToProcess);
        }
    } else {
        float detune = m_detune->finalValue();
        float detuneScale = detuneToFrequencyMultiplier(detune);
        finalScale *= detuneScale;
    }

    if (hasSampleAccurateValues) {
        clampFrequency(phaseIncrements, framesToProcess, context().sampleRate() / 2);
        // Convert from frequency to wave increment.
        vsmul(phaseIncrements, 1, &finalScale, phaseIncrements, 1, framesToProcess);
    }

    return hasSampleAccurateValues;
}

static float doInterpolation(double virtualReadIndex, float incr, unsigned readIndexMask, float tableInterpolationFactor, const float* lowerWaveData, const float* higherWaveData)
{
    ASSERT(incr >= 0);
    ASSERT(std::isfinite(virtualReadIndex));

    double sampleLower = 0;
    double sampleHigher = 0;

    unsigned readIndex0 = static_cast<unsigned>(virtualReadIndex);

    // Consider a typical sample rate of 44100 Hz and max periodic wave
    // size of 4096. The relationship between |incr| and the frequency
    // of the oscillator is |incr| = freq * 4096/44100. Or freq =
    // |incr|*44100/4096 = 10.8*|incr|.
    //
    // For the |incr| thresholds below, this means that we use linear
    // interpolation for all freq >= 3.2 Hz, 3-point Lagrange
    // for freq >= 1.7 Hz and 5-point Lagrange for every thing else.
    //
    // We use Lagrange interpolation because it's relatively simple to
    // implement and fairly inexpensive, and the interpolator always
    // passes through known points.
    if (incr >= interpolate2Point) {
        // Increment is fairly large, so we're doing no more than about 3
        // points between each wave table entry. Assume linear
        // interpolation between points is good enough.
        unsigned readIndex2 = readIndex0 + 1;

        // Contain within valid range.
        readIndex0 = readIndex0 & readIndexMask;
        readIndex2 = readIndex2 & readIndexMask;

        float sample1Lower = lowerWaveData[readIndex0];
        float sample2Lower = lowerWaveData[readIndex2];
        float sample1Higher = higherWaveData[readIndex0];
        float sample2Higher = higherWaveData[readIndex2];

        // Linearly interpolate within each table (lower and higher).
        double interpolationFactor = static_cast<float>(virtualReadIndex) - readIndex0;
        sampleHigher = (1 - interpolationFactor) * sample1Higher + interpolationFactor * sample2Higher;
        sampleLower = (1 - interpolationFactor) * sample1Lower + interpolationFactor * sample2Lower;

    } else if (incr >= interpolate3Point) {
        // We're doing about 6 interpolation values between each wave
        // table sample. Just use a 3-point Lagrange interpolator to get a
        // better estimate than just linear.
        //
        // See 3-point formula in http://dlmf.nist.gov/3.3#ii
        unsigned readIndex[3];

        for (int k = -1; k <= 1; ++k)
            readIndex[k + 1] = (readIndex0 + k) & readIndexMask;

        double a[3];
        double t = virtualReadIndex - readIndex0;

        a[0] = 0.5 * t * (t - 1);
        a[1] = 1 - t * t;
        a[2] = 0.5 * t * (t + 1);

        for (int k = 0; k < 3; ++k) {
            sampleLower += a[k] * lowerWaveData[readIndex[k]];
            sampleHigher += a[k] * higherWaveData[readIndex[k]];
        }
    } else {
        // For everything else (more than 6 points per entry), we'll do a
        // 5-point Lagrange interpolator. This is a trade-off between
        // quality and speed.
        //
        // See 5-point formula in http://dlmf.nist.gov/3.3#ii
        unsigned readIndex[5];
        for (int k = -2; k <= 2; ++k)
            readIndex[k + 2] = (readIndex0 + k) & readIndexMask;

        double a[5];
        double t = virtualReadIndex - readIndex0;
        double t2 = t * t;

        a[0] = t * (t2 - 1) * (t - 2) / 24;
        a[1] = -t * (t - 1) * (t2 - 4) / 6;
        a[2] = (t2 - 1) * (t2 - 4) / 4;
        a[3] = -t * (t + 1) * (t2 - 4) / 6;
        a[4] = t * (t2 - 1) * (t + 2) / 24;

        for (int k = 0; k < 5; ++k) {
            sampleLower += a[k] * lowerWaveData[readIndex[k]];
            sampleHigher += a[k] * higherWaveData[readIndex[k]];
        }
    }

    // Then interpolate between the two tables.
    float sample = (1 - tableInterpolationFactor) * sampleHigher + tableInterpolationFactor * sampleLower;
    return sample;
}

double OscillatorNode::processARate(int n, float* destP, double virtualReadIndex, float* phaseIncrements)
{
    float rateScale = m_periodicWave->rateScale();
    float invRateScale = 1 / rateScale;
    unsigned periodicWaveSize = m_periodicWave->periodicWaveSize();
    double invPeriodicWaveSize = 1.0 / periodicWaveSize;
    unsigned readIndexMask = periodicWaveSize - 1;

    float* higherWaveData = nullptr;
    float* lowerWaveData = nullptr;
    float tableInterpolationFactor = 0;

    for (int k = 0; k < n; ++k) {
        float incr = *phaseIncrements++;

        float frequency = invRateScale * incr;
        m_periodicWave->waveDataForFundamentalFrequency(frequency, lowerWaveData, higherWaveData, tableInterpolationFactor);

        float sample = doInterpolation(virtualReadIndex, fabs(incr), readIndexMask, tableInterpolationFactor, lowerWaveData, higherWaveData);

        *destP++ = sample;

        // Increment virtual read index and wrap virtualReadIndex into the range
        // 0 -> periodicWaveSize.
        virtualReadIndex += incr;
        virtualReadIndex -= floor(virtualReadIndex * invPeriodicWaveSize) * periodicWaveSize;
    }

    return virtualReadIndex;
}

double OscillatorNode::processKRate(int n, float* destP, double virtualReadIndex)
{
    unsigned periodicWaveSize = m_periodicWave->periodicWaveSize();
    double invPeriodicWaveSize = 1.0 / periodicWaveSize;
    unsigned readIndexMask = periodicWaveSize - 1;

    float frequency = 0;
    float* higherWaveData = nullptr;
    float* lowerWaveData = nullptr;
    float tableInterpolationFactor = 0;

    frequency = m_frequency->finalValue();
    float detune = m_detune->finalValue();
    float detuneScale = detuneToFrequencyMultiplier(detune);
    frequency *= detuneScale;
    clampFrequency(&frequency, 1, context().sampleRate() / 2);
    m_periodicWave->waveDataForFundamentalFrequency(frequency, lowerWaveData, higherWaveData, tableInterpolationFactor);

    float rateScale = m_periodicWave->rateScale();
    float incr = frequency * rateScale;

    for (int k = 0; k < n; ++k) {
        float sample = doInterpolation(virtualReadIndex, fabs(incr), readIndexMask, tableInterpolationFactor, lowerWaveData, higherWaveData);

        *destP++ = sample;

        // Increment virtual read index and wrap virtualReadIndex into the range
        // 0 -> periodicWaveSize.
        virtualReadIndex += incr;
        virtualReadIndex -= floor(virtualReadIndex * invPeriodicWaveSize) * periodicWaveSize;
    }

    return virtualReadIndex;
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
    double startFrameOffset = 0;
    updateSchedulingInfo(framesToProcess, outputBus, quantumFrameOffset, nonSilentFramesToProcess, startFrameOffset);

    if (!nonSilentFramesToProcess) {
        outputBus.zero();
        return;
    }

    float* destP = outputBus.channel(0)->mutableData();

    ASSERT(quantumFrameOffset <= framesToProcess);

    // We keep virtualReadIndex double-precision since we're accumulating values.
    double virtualReadIndex = m_virtualReadIndex;

    float rateScale = m_periodicWave->rateScale();
    bool hasSampleAccurateValues = calculateSampleAccuratePhaseIncrements(framesToProcess);

    float frequency = 0;
    float* higherWaveData = nullptr;
    float* lowerWaveData = nullptr;
    float tableInterpolationFactor = 0;

    if (!hasSampleAccurateValues) {
        frequency = m_frequency->finalValue();
        float detune = m_detune->finalValue();
        float detuneScale = detuneToFrequencyMultiplier(detune);
        frequency *= detuneScale;
        clampFrequency(&frequency, 1, context().sampleRate() / 2);
        m_periodicWave->waveDataForFundamentalFrequency(frequency, lowerWaveData, higherWaveData, tableInterpolationFactor);
    }

    float* phaseIncrements = m_phaseIncrements.data();

    // Start rendering at the correct offset.
    destP += quantumFrameOffset;
    int n = nonSilentFramesToProcess;

    // If startFrameOffset is not 0, that means the oscillator doesn't actually
    // start at quantumFrameOffset, but just past that time. Adjust destP and n
    // to reflect that, and adjust virtualReadIndex to start the value at
    // startFrameOffset.
    if (startFrameOffset > 0) {
        ++destP;
        --n;
        virtualReadIndex += (1 - startFrameOffset) * frequency * rateScale;
        ASSERT(virtualReadIndex < m_periodicWave->periodicWaveSize());
    } else if (startFrameOffset < 0)
        virtualReadIndex = -startFrameOffset * frequency * rateScale;

    if (hasSampleAccurateValues)
        virtualReadIndex = processARate(n, destP, virtualReadIndex, phaseIncrements);
    else
        virtualReadIndex = processKRate(n, destP, virtualReadIndex);

    m_virtualReadIndex = virtualReadIndex;

    outputBus.clearSilentFlag();
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
