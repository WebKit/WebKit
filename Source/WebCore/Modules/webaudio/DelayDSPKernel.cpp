/*
 * Copyright (C) 2010-2016 Google Inc. All rights reserved.
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

#include "DelayDSPKernel.h"

#include "AudioUtilities.h"
#include "VectorMath.h"
#include <algorithm>

namespace WebCore {

static size_t bufferLengthForDelay(double maxDelayTime, double sampleRate)
{
    // Compute the length of the buffer needed to handle a max delay of |maxDelayTime|. Add an additional render quantum frame size so we can
    // vectorize the delay processing. The extra space is needed so that writes to the buffer won't overlap reads from the buffer.
    return AudioUtilities::renderQuantumSize + AudioUtilities::timeToSampleFrame(maxDelayTime, sampleRate, AudioUtilities::SampleFrameRounding::Up);
}

// Returns (a - b) if a is greater than b, 0 otherwise.
template<typename T> static inline size_t positiveSubtract(T a, T b)
{
    return a <= b ? 0 : static_cast<size_t>(a - b);
}

static void copyToCircularBuffer(float* buffer, size_t writeIndex, size_t bufferLength, const float* source, size_t framesToProcess)
{
    // The algorithm below depends on this being true because we don't expect to have to fill the entire buffer more than once.
    RELEASE_ASSERT(bufferLength >= framesToProcess);

    // Copy |framesToProcess| values from |source| to the circular buffer that starts at |buffer| of length |bufferLength|. The
    // copy starts at index |writeIndex| into the buffer.
    auto* writePointer = &buffer[writeIndex];
    size_t remainder = positiveSubtract(bufferLength, writeIndex);

    // Copy the frames over, carefully handling the case where we need to wrap around to the beginning of the buffer.
    memcpy(writePointer, source, sizeof(*writePointer) * std::min(framesToProcess, remainder));
    memcpy(buffer, source + remainder, sizeof(*buffer) * positiveSubtract(framesToProcess, remainder));
}

DelayDSPKernel::DelayDSPKernel(DelayProcessor* processor)
    : AudioDSPKernel(processor)
    , m_delayTimes(AudioUtilities::renderQuantumSize)
    , m_tempBuffer(AudioUtilities::renderQuantumSize)
{
    ASSERT(processor && processor->sampleRate() > 0);
    if (!(processor && processor->sampleRate() > 0))
        return;

    m_maxDelayTime = processor->maxDelayTime();
    ASSERT(m_maxDelayTime >= 0 && !std::isnan(m_maxDelayTime));
    if (m_maxDelayTime < 0 || std::isnan(m_maxDelayTime))
        return;

    m_buffer.resize(bufferLengthForDelay(m_maxDelayTime, processor->sampleRate()));
}

DelayDSPKernel::DelayDSPKernel(double maxDelayTime, float sampleRate)
    : AudioDSPKernel(sampleRate)
    , m_maxDelayTime(maxDelayTime)
    , m_tempBuffer(AudioUtilities::renderQuantumSize)
{
    ASSERT(maxDelayTime > 0.0);
    if (maxDelayTime <= 0.0)
        return;

    size_t bufferLength = bufferLengthForDelay(maxDelayTime, sampleRate);
    ASSERT(bufferLength);
    if (!bufferLength)
        return;

    m_buffer.resize(bufferLength);
}

void DelayDSPKernel::process(const float* source, float* destination, size_t framesToProcess)
{
    ASSERT(m_buffer.size());
    ASSERT(source && destination);
    if (UNLIKELY(m_buffer.isEmpty() || !source || !destination))
        return;

    bool sampleAccurate = delayProcessor() && delayProcessor()->delayTime().hasSampleAccurateValues();
    bool shouldUseARate = delayProcessor() && delayProcessor()->delayTime().automationRate() == AutomationRate::ARate;
    if (sampleAccurate && shouldUseARate)
        processARate(source, destination, framesToProcess);
    else
        processKRate(source, destination, framesToProcess);
}

void DelayDSPKernel::processARate(const float* source, float* destination, size_t framesToProcess)
{
    size_t bufferLength = m_buffer.size();
    auto* buffer = m_buffer.data();

    delayProcessor()->delayTime().calculateSampleAccurateValues(m_delayTimes.data(), framesToProcess);

    copyToCircularBuffer(buffer, m_writeIndex, bufferLength, source, framesToProcess);

    for (unsigned i = 0; i < framesToProcess; ++i) {
        double delayTime = m_delayTimes[i];
        if (std::isnan(delayTime))
            delayTime = maxDelayTime();
        else
            delayTime = std::clamp<double>(delayTime, 0.0, maxDelayTime());

        double desiredDelayFrames = delayTime * sampleRate();

        double readPosition = m_writeIndex + bufferLength - desiredDelayFrames;
        if (readPosition >= bufferLength)
            readPosition -= bufferLength;

        // Linearly interpolate in-between delay times.
        size_t readIndex1 = static_cast<size_t>(readPosition);
        size_t readIndex2 = (readIndex1 + 1) % bufferLength;
        float interpolationFactor = readPosition - readIndex1;

        m_writeIndex = (m_writeIndex + 1) % bufferLength;

        float sample1 = buffer[readIndex1];
        float sample2 = buffer[readIndex2];
        destination[i] = sample1 + interpolationFactor * (sample2 - sample1);
    }
}

// Optimized version of processARate() when the delayTime is constant.
void DelayDSPKernel::processKRate(const float* source, float* destination, size_t framesToProcess)
{
    size_t bufferLength = m_buffer.size();
    auto* buffer = m_buffer.data();

    double delayTime = delayProcessor() ? delayProcessor()->delayTime().finalValue() : m_desiredDelayFrames / sampleRate();
    // Make sure the delay time is in a valid range.
    delayTime = std::clamp(delayTime, 0.0, maxDelayTime());
    double desiredDelayFrames = delayTime * sampleRate();

    double readPosition = m_writeIndex + bufferLength - desiredDelayFrames;
    if (readPosition >= bufferLength)
        readPosition -= bufferLength;

    // Linearly interpolate in-between delay times. |readIndex1| and |readIndex2| are the indices of the frames to be used
    // for interpolation.
    size_t readIndex1 = static_cast<size_t>(readPosition);
    float interpolationFactor = readPosition - readIndex1;
    auto* bufferEnd = &buffer[bufferLength];
    ASSERT(static_cast<unsigned>(bufferLength) >= framesToProcess);

    // sample1 and sample2 hold the current and next samples in the buffer. These are used for interoplating the delay value.
    // To reduce memory usage and an extra memcpy, sample1 can be the same as destination.
    // VectorMath::interpolate() below has an optimization in the case where the input buffer is the same as the output one.
    auto* sample1 = destination;

    // Copy data from the source into the buffer, starting at the write index. The buffer is circular, so carefully handle
    // the wrapping of the write pointer.
    copyToCircularBuffer(buffer, m_writeIndex, bufferLength, source, framesToProcess);
    m_writeIndex = (m_writeIndex + framesToProcess) % bufferLength;

    // Now copy out the samples from the buffer, starting at the read pointer, carefully handling wrapping of the read pointer.
    auto* readPointer = &buffer[readIndex1];

    size_t remainder = positiveSubtract(bufferEnd, readPointer);
    memcpy(sample1, readPointer, sizeof(*sample1) * std::min(framesToProcess, remainder));
    memcpy(sample1 + remainder, buffer, sizeof(*sample1) * positiveSubtract(framesToProcess, remainder));

    // If interpolationFactor is 0, we don't need to do any interpolation and sample1 contains the desired values.
    if (!interpolationFactor)
        return;

    ASSERT(framesToProcess <= m_tempBuffer.size());

    size_t readIndex2 = (readIndex1 + 1) % bufferLength;
    auto* sample2 = m_tempBuffer.data();

    readPointer = &buffer[readIndex2];
    remainder = positiveSubtract(bufferEnd, readPointer);
    memcpy(sample2, readPointer, sizeof(*sample2) * std::min(framesToProcess, remainder));
    memcpy(sample2 + remainder, buffer, sizeof(*sample2) * positiveSubtract(framesToProcess, remainder));

    // Interpolate samples.
    // destination[k] = sample1[k] + interpolationFactor * (sample2[k] - sample1[k]);
    VectorMath::interpolate(sample1, sample2, interpolationFactor, destination, framesToProcess);
}

void DelayDSPKernel::processOnlyAudioParams(size_t framesToProcess)
{
    if (!delayProcessor())
        return;

    float values[AudioUtilities::renderQuantumSize];
    ASSERT(framesToProcess <= AudioUtilities::renderQuantumSize);

    delayProcessor()->delayTime().calculateSampleAccurateValues(values, framesToProcess);
}

void DelayDSPKernel::reset()
{
    m_buffer.zero();
}

double DelayDSPKernel::tailTime() const
{
    return m_maxDelayTime;
}

double DelayDSPKernel::latencyTime() const
{
    return 0;
}

bool DelayDSPKernel::requiresTailProcessing() const
{
    // Always return true even if the tail time and latency might both
    // be zero. This is for simplicity; most interesting delay nodes
    // have non-zero delay times anyway. And it's ok to return true. It
    // just means the node lives a little longer than strictly
    // necessary.
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
