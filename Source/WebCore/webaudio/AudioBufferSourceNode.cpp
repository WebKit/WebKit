/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "AudioBufferSourceNode.h"

#include "AudioContext.h"
#include "AudioNodeOutput.h"
#include "Document.h"
#include "FloatConversion.h"
#include "ScriptCallStack.h"
#include <algorithm>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

const double DefaultGrainDuration = 0.020; // 20ms
const double UnknownTime = -1;

// Arbitrary upper limit on playback rate.
// Higher than expected rates can be useful when playing back oversampled buffers
// to minimize linear interpolation aliasing.
const double MaxRate = 1024;

PassRefPtr<AudioBufferSourceNode> AudioBufferSourceNode::create(AudioContext* context, float sampleRate)
{
    return adoptRef(new AudioBufferSourceNode(context, sampleRate));
}

AudioBufferSourceNode::AudioBufferSourceNode(AudioContext* context, float sampleRate)
    : AudioSourceNode(context, sampleRate)
    , m_buffer(0)
    , m_isPlaying(false)
    , m_isLooping(false)
    , m_hasFinished(false)
    , m_startTime(0.0)
    , m_endTime(UnknownTime)
    , m_virtualReadIndex(0)
    , m_isGrain(false)
    , m_grainOffset(0.0)
    , m_grainDuration(DefaultGrainDuration)
    , m_lastGain(1.0)
    , m_pannerNode(0)
{
    setNodeType(NodeTypeAudioBufferSource);

    m_gain = AudioGain::create("gain", 1.0, 0.0, 1.0);
    m_playbackRate = AudioParam::create("playbackRate", 1.0, 0.0, MaxRate);
    
    m_gain->setContext(context);
    m_playbackRate->setContext(context);

    // Default to mono.  A call to setBuffer() will set the number of output channels to that of the buffer.
    addOutput(adoptPtr(new AudioNodeOutput(this, 1)));

    initialize();
}

AudioBufferSourceNode::~AudioBufferSourceNode()
{
    uninitialize();
}

void AudioBufferSourceNode::process(size_t framesToProcess)
{
    AudioBus* outputBus = output(0)->bus();

    if (!isInitialized()) {
        outputBus->zero();
        return;
    }

    // The audio thread can't block on this lock, so we call tryLock() instead.
    // Careful - this is a tryLock() and not an autolocker, so we must unlock() before every return.
    if (m_processLock.tryLock()) {
        // Check if it's time to start playing.
        float sampleRate = this->sampleRate();
        double quantumStartTime = context()->currentTime();
        double quantumEndTime = quantumStartTime + framesToProcess / sampleRate;

        // If we know the end time and it's already passed, then don't bother doing any more rendering this cycle.
        if (m_endTime != UnknownTime && m_endTime <= quantumStartTime) {
            m_isPlaying = false;
            m_virtualReadIndex = 0;
            finish();
        }
        
        if (!m_isPlaying || m_hasFinished || !buffer() || m_startTime >= quantumEndTime) {
            // FIXME: can optimize here by propagating silent hint instead of forcing the whole chain to process silence.
            outputBus->zero();
            m_processLock.unlock();
            return;
        }

        double quantumTimeOffset = m_startTime > quantumStartTime ? m_startTime - quantumStartTime : 0;
        size_t quantumFrameOffset = static_cast<unsigned>(quantumTimeOffset * sampleRate);
        quantumFrameOffset = min(quantumFrameOffset, framesToProcess); // clamp to valid range
        size_t bufferFramesToProcess = framesToProcess - quantumFrameOffset;

        // Render by reading directly from the buffer.
        renderFromBuffer(outputBus, quantumFrameOffset, bufferFramesToProcess);

        // Apply the gain (in-place) to the output bus.
        float totalGain = gain()->value() * m_buffer->gain();
        outputBus->copyWithGainFrom(*outputBus, &m_lastGain, totalGain);

        // If the end time is somewhere in the middle of this time quantum, then simply zero out the
        // frames starting at the end time.
        if (m_endTime != UnknownTime && m_endTime >= quantumStartTime && m_endTime < quantumEndTime) {
            size_t zeroStartFrame = narrowPrecisionToFloat((m_endTime - quantumStartTime) * sampleRate);
            size_t framesToZero = framesToProcess - zeroStartFrame;

            bool isSafe = zeroStartFrame < framesToProcess && framesToZero <= framesToProcess && zeroStartFrame + framesToZero <= framesToProcess;
            ASSERT(isSafe);
            
            if (isSafe) {
                for (unsigned i = 0; i < outputBus->numberOfChannels(); ++i)
                    memset(outputBus->channel(i)->data() + zeroStartFrame, 0, sizeof(float) * framesToZero);
            }

            m_isPlaying = false;
            m_virtualReadIndex = 0;
            finish();
        }

        m_processLock.unlock();
    } else {
        // Too bad - the tryLock() failed.  We must be in the middle of changing buffers and were already outputting silence anyway.
        outputBus->zero();
    }
}

// Returns true if we're finished.
bool AudioBufferSourceNode::renderSilenceAndFinishIfNotLooping(float* destinationL, float* destinationR, size_t framesToProcess)
{
    if (!loop()) {
        // If we're not looping, then stop playing when we get to the end.
        m_isPlaying = false;

        if (framesToProcess > 0) {
            // We're not looping and we've reached the end of the sample data, but we still need to provide more output,
            // so generate silence for the remaining.
            memset(destinationL, 0, sizeof(float) * framesToProcess);

            if (destinationR)
                memset(destinationR, 0, sizeof(float) * framesToProcess);
        }

        finish();
        return true;
    }
    return false;
}

void AudioBufferSourceNode::renderFromBuffer(AudioBus* bus, unsigned destinationFrameOffset, size_t numberOfFrames)
{
    ASSERT(context()->isAudioThread());
    
    // Basic sanity checking
    ASSERT(bus);
    ASSERT(buffer());
    if (!bus || !buffer())
        return;

    unsigned numberOfChannels = this->numberOfChannels();
    unsigned busNumberOfChannels = bus->numberOfChannels();

    // FIXME: we can add support for sources with more than two channels, but this is not a common case.
    bool channelCountGood = numberOfChannels == busNumberOfChannels && (numberOfChannels == 1 || numberOfChannels == 2);
    ASSERT(channelCountGood);
    if (!channelCountGood)
        return;

    // Get the destination pointers.
    float* destinationL = bus->channel(0)->data();
    ASSERT(destinationL);
    if (!destinationL)
        return;
    float* destinationR = (numberOfChannels < 2) ? 0 : bus->channel(1)->data();
    
    bool isStereo = destinationR;
    
    // Sanity check destinationFrameOffset, numberOfFrames.
    size_t destinationLength = bus->length();

    bool isLengthGood = destinationLength <= 4096 && numberOfFrames <= 4096;
    ASSERT(isLengthGood);
    if (!isLengthGood)
        return;

    bool isOffsetGood = destinationFrameOffset <= destinationLength && destinationFrameOffset + numberOfFrames <= destinationLength;
    ASSERT(isOffsetGood);
    if (!isOffsetGood)
        return;
        
    // Potentially zero out initial frames leading up to the offset.
    if (destinationFrameOffset) {
        memset(destinationL, 0, sizeof(float) * destinationFrameOffset);
        if (destinationR)
            memset(destinationR, 0, sizeof(float) * destinationFrameOffset);
    }

    // Offset the pointers to the correct offset frame.
    destinationL += destinationFrameOffset;
    if (destinationR)
        destinationR += destinationFrameOffset;

    size_t bufferLength = buffer()->length();
    double bufferSampleRate = buffer()->sampleRate();

    // Calculate the start and end frames in our buffer that we want to play.
    // If m_isGrain is true, then we will be playing a portion of the total buffer.
    unsigned startFrame = m_isGrain ? static_cast<unsigned>(m_grainOffset * bufferSampleRate) : 0;
    unsigned endFrame = m_isGrain ? static_cast<unsigned>(startFrame + m_grainDuration * bufferSampleRate) : bufferLength;
    
    ASSERT(endFrame >= startFrame);
    if (endFrame < startFrame)
        return;
    
    unsigned deltaFrames = endFrame - startFrame;
    
    // This is a HACK to allow for HRTF tail-time - avoids glitch at end.
    // FIXME: implement tailTime for each AudioNode for a more general solution to this problem.
    if (m_isGrain)
        endFrame += 512;

    // Do some sanity checking.
    if (startFrame >= bufferLength)
        startFrame = !bufferLength ? 0 : bufferLength - 1;
    if (endFrame > bufferLength)
        endFrame = bufferLength;
    if (m_virtualReadIndex >= endFrame)
        m_virtualReadIndex = startFrame; // reset to start

    // Get pointers to the start of the sample buffer.
    float* sourceL = m_buffer->getChannelData(0)->data();
    float* sourceR = m_buffer->numberOfChannels() == 2 ? m_buffer->getChannelData(1)->data() : 0;
    
    double pitchRate = totalPitchRate();

    // Get local copy.
    double virtualReadIndex = m_virtualReadIndex;

    // Render loop - reading from the source buffer to the destination using linear interpolation.
    int framesToProcess = numberOfFrames;

    // Optimize for the very common case of playing back with pitchRate == 1.
    // We can avoid the linear interpolation.
    if (pitchRate == 1 && virtualReadIndex == floor(virtualReadIndex)) {
        unsigned readIndex = static_cast<unsigned>(virtualReadIndex);
        while (framesToProcess > 0) {
            int framesToEnd = endFrame - readIndex;
            int framesThisTime = min(framesToProcess, framesToEnd);
            framesThisTime = max(0, framesThisTime);

            memcpy(destinationL, sourceL + readIndex, sizeof(*sourceL) * framesThisTime);
            destinationL += framesThisTime;
            if (isStereo) {
                memcpy(destinationR, sourceR + readIndex, sizeof(*sourceR) * framesThisTime);
                destinationR += framesThisTime;
            }

            readIndex += framesThisTime;
            framesToProcess -= framesThisTime;

            // Wrap-around.
            if (readIndex >= endFrame) {
                readIndex -= deltaFrames;
                if (renderSilenceAndFinishIfNotLooping(destinationL, destinationR, framesToProcess))
                    break;
            }
        }
        virtualReadIndex = readIndex;
    } else {
        while (framesToProcess--) {
            unsigned readIndex = static_cast<unsigned>(virtualReadIndex);
            double interpolationFactor = virtualReadIndex - readIndex;

            // For linear interpolation we need the next sample-frame too.
            unsigned readIndex2 = readIndex + 1;
            if (readIndex2 >= endFrame) {
                if (loop()) {
                    // Make sure to wrap around at the end of the buffer.
                    readIndex2 -= deltaFrames;
                } else
                    readIndex2 = readIndex;
            }

            // Final sanity check on buffer access.
            // FIXME: as an optimization, try to get rid of this inner-loop check and put assertions and guards before the loop.
            if (readIndex >= bufferLength || readIndex2 >= bufferLength)
                break;

            // Linear interpolation.
            double sampleL1 = sourceL[readIndex];
            double sampleL2 = sourceL[readIndex2];
            double sampleL = (1.0 - interpolationFactor) * sampleL1 + interpolationFactor * sampleL2;
            *destinationL++ = narrowPrecisionToFloat(sampleL);

            if (isStereo) {
                double sampleR1 = sourceR[readIndex];
                double sampleR2 = sourceR[readIndex2];
                double sampleR = (1.0 - interpolationFactor) * sampleR1 + interpolationFactor * sampleR2;
                *destinationR++ = narrowPrecisionToFloat(sampleR);
            }

            virtualReadIndex += pitchRate;

            // Wrap-around, retaining sub-sample position since virtualReadIndex is floating-point.
            if (virtualReadIndex >= endFrame) {
                virtualReadIndex -= deltaFrames;

                if (renderSilenceAndFinishIfNotLooping(destinationL, destinationR, framesToProcess))
                    break;
            }
        }
    } 
    m_virtualReadIndex = virtualReadIndex;
}


void AudioBufferSourceNode::reset()
{
    m_virtualReadIndex = 0;
    m_lastGain = gain()->value();
}

void AudioBufferSourceNode::finish()
{
    if (!m_hasFinished) {
        // Let the context dereference this AudioNode.
        context()->notifyNodeFinishedProcessing(this);
        m_hasFinished = true;
    }
}

bool AudioBufferSourceNode::setBuffer(AudioBuffer* buffer)
{
    ASSERT(isMainThread());
    
    // The context must be locked since changing the buffer can re-configure the number of channels that are output.
    AudioContext::AutoLocker contextLocker(context());
    
    // This synchronizes with process().
    MutexLocker processLocker(m_processLock);
    
    if (buffer) {
        // Do any necesssary re-configuration to the buffer's number of channels.
        unsigned numberOfChannels = buffer->numberOfChannels();
        if (!numberOfChannels || numberOfChannels > 2) {
            // FIXME: implement multi-channel greater than stereo.
            return false;
        }
        output(0)->setNumberOfChannels(numberOfChannels);
    }

    m_virtualReadIndex = 0;
    m_buffer = buffer;
    
    return true;
}

unsigned AudioBufferSourceNode::numberOfChannels()
{
    return output(0)->numberOfChannels();
}

void AudioBufferSourceNode::noteOn(double when)
{
    ASSERT(isMainThread());
    if (m_isPlaying)
        return;

    m_isGrain = false;
    m_startTime = when;
    m_virtualReadIndex = 0;
    m_isPlaying = true;
}

void AudioBufferSourceNode::noteGrainOn(double when, double grainOffset, double grainDuration)
{
    ASSERT(isMainThread());
    if (m_isPlaying)
        return;

    if (!buffer())
        return;
        
    // Do sanity checking of grain parameters versus buffer size.
    double bufferDuration = buffer()->duration();

    if (grainDuration > bufferDuration)
        return; // FIXME: maybe should throw exception - consider in specification.
    
    double maxGrainOffset = bufferDuration - grainDuration;
    maxGrainOffset = max(0.0, maxGrainOffset);

    grainOffset = max(0.0, grainOffset);
    grainOffset = min(maxGrainOffset, grainOffset);
    m_grainOffset = grainOffset;

    m_grainDuration = grainDuration;
    
    m_isGrain = true;
    m_startTime = when;

    // We call floor() here since at playbackRate == 1 we don't want to go through linear interpolation
    // at a sub-sample position since it will degrade the quality.
    // When aligned to the sample-frame the playback will be identical to the PCM data stored in the buffer.
    // Since playbackRate == 1 is very common, it's worth considering quality.
    m_virtualReadIndex = floor(m_grainOffset * buffer()->sampleRate());
    
    m_isPlaying = true;
}

void AudioBufferSourceNode::noteOff(double when)
{
    ASSERT(isMainThread());
    if (!m_isPlaying)
        return;
    
    when = max(0.0, when);
    m_endTime = when;
}

double AudioBufferSourceNode::totalPitchRate()
{
    double dopplerRate = 1.0;
    if (m_pannerNode.get())
        dopplerRate = m_pannerNode->dopplerRate();
    
    // Incorporate buffer's sample-rate versus AudioContext's sample-rate.
    // Normally it's not an issue because buffers are loaded at the AudioContext's sample-rate, but we can handle it in any case.
    double sampleRateFactor = 1.0;
    if (buffer())
        sampleRateFactor = buffer()->sampleRate() / sampleRate();
    
    double basePitchRate = playbackRate()->value();

    double totalRate = dopplerRate * sampleRateFactor * basePitchRate;

    // Sanity check the total rate.  It's very important that the resampler not get any bad rate values.
    totalRate = max(0.0, totalRate);
    if (!totalRate)
        totalRate = 1; // zero rate is considered illegal
    totalRate = min(MaxRate, totalRate);
    
    bool isTotalRateValid = !isnan(totalRate) && !isinf(totalRate);
    ASSERT(isTotalRateValid);
    if (!isTotalRateValid)
        totalRate = 1.0;

    return totalRate;
}

bool AudioBufferSourceNode::looping()
{
    static bool firstTime = true;
    if (firstTime && context() && context()->document()) {
        context()->document()->addConsoleMessage(JSMessageSource, LogMessageType, WarningMessageLevel, "AudioBufferSourceNode 'looping' attribute is deprecated.  Use 'loop' instead.");
        firstTime = false;
    }

    return m_isLooping;
}

void AudioBufferSourceNode::setLooping(bool looping)
{
    static bool firstTime = true;
    if (firstTime && context() && context()->document()) {
        context()->document()->addConsoleMessage(JSMessageSource, LogMessageType, WarningMessageLevel, "AudioBufferSourceNode 'looping' attribute is deprecated.  Use 'loop' instead.");
        firstTime = false;
    }

    m_isLooping = looping;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
