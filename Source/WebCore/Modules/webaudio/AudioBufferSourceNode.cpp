/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2020, Apple Inc. All rights reserved.
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

#include "AudioBuffer.h"
#include "AudioContext.h"
#include "AudioNodeOutput.h"
#include "AudioParam.h"
#include "AudioUtilities.h"
#include "FloatConversion.h"
#include "PannerNode.h"
#include "ScriptExecutionContext.h"
#include "WebKitAudioBufferSourceNode.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioBufferSourceNode);
WTF_MAKE_ISO_ALLOCATED_IMPL(WebKitAudioBufferSourceNode);

const double DefaultGrainDuration = 0.020; // 20ms

// Arbitrary upper limit on playback rate.
// Higher than expected rates can be useful when playing back oversampled buffers
// to minimize linear interpolation aliasing.
const double MaxRate = 1024;

ExceptionOr<Ref<AudioBufferSourceNode>> AudioBufferSourceNode::create(BaseAudioContext& context, AudioBufferSourceOptions&& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    context.lazyInitialize();

    auto node = adoptRef(*new AudioBufferSourceNode(context));

    node->setBuffer(WTFMove(options.buffer));
    node->detune().setValue(options.detune);
    node->setLoop(options.loop);
    node->setLoopEnd(options.loopEnd);
    node->setLoopStart(options.loopStart);
    node->playbackRate().setValue(options.playbackRate);

    // Because this is an AudioScheduledSourceNode, the context keeps a reference until it has finished playing.
    // When this happens, AudioScheduledSourceNode::finish() calls BaseAudioContext::notifyNodeFinishedProcessing().
    context.refNode(node);

    return node;
}

AudioBufferSourceNode::AudioBufferSourceNode(BaseAudioContext& context)
    : AudioScheduledSourceNode(context)
    , m_detune(AudioParam::create(context, "detune"_s, 0.0, -FLT_MAX, FLT_MAX, AutomationRate::KRate, AutomationRateMode::Fixed))
    , m_playbackRate(AudioParam::create(context, "playbackRate"_s, 1.0, -FLT_MAX, FLT_MAX, AutomationRate::KRate, AutomationRateMode::Fixed))
    , m_grainDuration(DefaultGrainDuration)
{
    setNodeType(NodeTypeAudioBufferSource);

    // Default to mono.  A call to setBuffer() will set the number of output channels to that of the buffer.
    addOutput(makeUnique<AudioNodeOutput>(this, 1));

    initialize();
}

AudioBufferSourceNode::~AudioBufferSourceNode()
{
    clearPannerNode();
    uninitialize();
}

void AudioBufferSourceNode::process(size_t framesToProcess)
{
    auto& outputBus = *output(0)->bus();

    if (!isInitialized()) {
        outputBus.zero();
        return;
    }

    // The audio thread can't block on this lock, so we use std::try_to_lock instead.
    std::unique_lock<Lock> lock(m_processMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        // Too bad - the try_lock() failed. We must be in the middle of changing buffers and were already outputting silence anyway.
        outputBus.zero();
        return;
    }

    if (!buffer()) {
        outputBus.zero();
        return;
    }

    // After calling setBuffer() with a buffer having a different number of channels, there can in rare cases be a slight delay
    // before the output bus is updated to the new number of channels because of use of tryLocks() in the context's updating system.
    // In this case, if the buffer has just been changed and we're not quite ready yet, then just output silence.
    if (numberOfChannels() != buffer()->numberOfChannels()) {
        outputBus.zero();
        return;
    }

    size_t quantumFrameOffset = 0;
    size_t bufferFramesToProcess = 0;
    double startFrameOffset = 0;
    updateSchedulingInfo(framesToProcess, outputBus, quantumFrameOffset, bufferFramesToProcess, startFrameOffset);

    if (!bufferFramesToProcess) {
        outputBus.zero();
        return;
    }

    for (unsigned i = 0; i < outputBus.numberOfChannels(); ++i)
        m_destinationChannels[i] = outputBus.channel(i)->mutableData();

    // Render by reading directly from the buffer.
    if (!renderFromBuffer(&outputBus, quantumFrameOffset, bufferFramesToProcess, startFrameOffset)) {
        outputBus.zero();
        return;
    }

    // Apply the gain (in-place) to the output bus.
    float totalGain = legacyGainValue();
    outputBus.copyWithGainFrom(outputBus, totalGain);
    outputBus.clearSilentFlag();
}

// Returns true if we're finished.
bool AudioBufferSourceNode::renderSilenceAndFinishIfNotLooping(AudioBus*, unsigned index, size_t framesToProcess)
{
    if (!loop()) {
        // If we're not looping, then stop playing when we get to the end.

        if (framesToProcess > 0) {
            // We're not looping and we've reached the end of the sample data, but we still need to provide more output,
            // so generate silence for the remaining.
            for (unsigned i = 0; i < numberOfChannels(); ++i) 
                memset(m_destinationChannels[i] + index, 0, sizeof(float) * framesToProcess);
        }

        finish();
        return true;
    }
    return false;
}

bool AudioBufferSourceNode::renderFromBuffer(AudioBus* bus, unsigned destinationFrameOffset, size_t numberOfFrames, double startFrameOffset)
{
    ASSERT(context().isAudioThread());

    // Basic sanity checking
    ASSERT(bus);
    ASSERT(buffer());
    if (!bus || !buffer())
        return false;

    unsigned numberOfChannels = this->numberOfChannels();
    unsigned busNumberOfChannels = bus->numberOfChannels();

    bool channelCountGood = numberOfChannels && numberOfChannels == busNumberOfChannels;
    ASSERT(channelCountGood);
    if (!channelCountGood)
        return false;

    // Sanity check destinationFrameOffset, numberOfFrames.
    size_t destinationLength = bus->length();

    bool isLengthGood = destinationLength <= 4096 && numberOfFrames <= 4096;
    ASSERT(isLengthGood);
    if (!isLengthGood)
        return false;

    bool isOffsetGood = destinationFrameOffset <= destinationLength && destinationFrameOffset + numberOfFrames <= destinationLength;
    ASSERT(isOffsetGood);
    if (!isOffsetGood)
        return false;

    // Potentially zero out initial frames leading up to the offset.
    if (destinationFrameOffset) {
        for (unsigned i = 0; i < numberOfChannels; ++i) 
            memset(m_destinationChannels[i], 0, sizeof(float) * destinationFrameOffset);
    }

    // Offset the pointers to the correct offset frame.
    unsigned writeIndex = destinationFrameOffset;

    size_t bufferLength = buffer()->length();
    double bufferSampleRate = buffer()->sampleRate();
    double pitchRate = totalPitchRate();
    bool reverse = pitchRate < 0;

    // Avoid converting from time to sample-frames twice by computing
    // the grain end time first before computing the sample frame.
    unsigned maxFrame;
    if (m_isGrain)
        maxFrame = AudioUtilities::timeToSampleFrame(m_grainOffset + m_grainDuration, bufferSampleRate);
    else
        maxFrame = bufferLength;

    // Do some sanity checking.
    if (maxFrame > bufferLength)
        maxFrame = bufferLength;

    // If the .loop attribute is true, then values of m_loopStart == 0 && m_loopEnd == 0 implies
    // that we should use the entire buffer as the loop, otherwise use the loop values in m_loopStart and m_loopEnd.
    double virtualMaxFrame = maxFrame;
    double virtualMinFrame = 0;
    double virtualDeltaFrames = maxFrame;

    if (loop() && (m_loopStart || m_loopEnd) && m_loopStart >= 0 && m_loopEnd > 0 && m_loopStart < m_loopEnd) {
        // Convert from seconds to sample-frames.
        double loopMinFrame = m_loopStart * buffer()->sampleRate();
        double loopMaxFrame = m_loopEnd * buffer()->sampleRate();

        virtualMaxFrame = std::min(loopMaxFrame, virtualMaxFrame);
        virtualMinFrame = std::max(loopMinFrame, virtualMinFrame);
        virtualDeltaFrames = virtualMaxFrame - virtualMinFrame;
    }

    // Sanity check that our playback rate isn't larger than the loop size.
    if (std::abs(pitchRate) > virtualDeltaFrames)
        return false;

    // Get local copy.
    double virtualReadIndex = m_virtualReadIndex;

    // Adjust the read index by the startFrameOffset (compensated by the pitch rate) because
    // we always start output on a frame boundary with interpolation if necessary.
    if (startFrameOffset < 0 && pitchRate)
        virtualReadIndex += std::abs(startFrameOffset * pitchRate);

    bool needsInterpolation = virtualReadIndex != floor(virtualReadIndex)
        || virtualDeltaFrames != floor(virtualDeltaFrames)
        || virtualMaxFrame != floor(virtualMaxFrame)
        || virtualMinFrame != floor(virtualMinFrame);

    // Render loop - reading from the source buffer to the destination using linear interpolation.
    int framesToProcess = numberOfFrames;

    const float** sourceChannels = m_sourceChannels.get();
    float** destinationChannels = m_destinationChannels.get();

    // Optimize for the very common case of playing back with pitchRate == 1.
    // We can avoid the linear interpolation.
    if (pitchRate == 1 && !needsInterpolation) {
        unsigned readIndex = static_cast<unsigned>(virtualReadIndex);
        unsigned deltaFrames = static_cast<unsigned>(virtualDeltaFrames);
        maxFrame = static_cast<unsigned>(virtualMaxFrame);
        while (framesToProcess > 0) {
            int framesToEnd = maxFrame - readIndex;
            int framesThisTime = std::min(framesToProcess, framesToEnd);
            framesThisTime = std::max(0, framesThisTime);

            for (unsigned i = 0; i < numberOfChannels; ++i) 
                memcpy(destinationChannels[i] + writeIndex, sourceChannels[i] + readIndex, sizeof(float) * framesThisTime);

            writeIndex += framesThisTime;
            readIndex += framesThisTime;
            framesToProcess -= framesThisTime;

            // Wrap-around.
            if (readIndex >= maxFrame) {
                readIndex -= deltaFrames;
                if (renderSilenceAndFinishIfNotLooping(bus, writeIndex, framesToProcess))
                    break;
            }
        }
        virtualReadIndex = readIndex;
    } else if (pitchRate == -1 && !needsInterpolation) {
        int readIndex = static_cast<int>(virtualReadIndex);
        int deltaFrames = static_cast<int>(virtualDeltaFrames);
        int minFrame = static_cast<int>(virtualMinFrame) - 1;
        while (framesToProcess > 0) {
            int framesToEnd = readIndex - minFrame;
            int framesThisTime = std::min<int>(framesToProcess, framesToEnd);
            framesThisTime = std::max<int>(0, framesThisTime);

            while (framesThisTime--) {
                for (unsigned i = 0; i < numberOfChannels; ++i) {
                    float* destination = destinationChannels[i];
                    const float* source = sourceChannels[i];

                    destination[writeIndex] = source[readIndex];
                }

                ++writeIndex;
                --readIndex;
                --framesToProcess;
            }

            // Wrap-around.
            if (readIndex <= minFrame) {
                readIndex += deltaFrames;
                if (renderSilenceAndFinishIfNotLooping(bus, writeIndex, framesToProcess))
                    break;
            }
        }
        virtualReadIndex = readIndex;
    } else if (!pitchRate) {
        unsigned readIndex = static_cast<unsigned>(virtualReadIndex);

        for (unsigned i = 0; i < numberOfChannels; ++i)
            std::fill_n(destinationChannels[i], framesToProcess, sourceChannels[i][readIndex]);
    } else if (reverse) {
        unsigned maxFrame = static_cast<unsigned>(virtualMaxFrame);
        unsigned minFrame = static_cast<unsigned>(floorf(virtualMinFrame));

        while (framesToProcess--) {
            unsigned readIndex = static_cast<unsigned>(floorf(virtualReadIndex));
            double interpolationFactor = virtualReadIndex - readIndex;

            unsigned readIndex2 = readIndex + 1;
            if (readIndex2 >= maxFrame)
                readIndex2 = loop() ? minFrame : maxFrame - 1;

            // Linear interpolation.
            for (unsigned i = 0; i < numberOfChannels; ++i) {
                float* destination = destinationChannels[i];
                const float* source = sourceChannels[i];

                double sample1 = source[readIndex];
                double sample2 = source[readIndex2];
                double sample = (1.0 - interpolationFactor) * sample1 + interpolationFactor * sample2;

                destination[writeIndex] = narrowPrecisionToFloat(sample);
            }

            writeIndex++;

            virtualReadIndex += pitchRate;

            // Wrap-around, retaining sub-sample position since virtualReadIndex is floating-point.
            if (virtualReadIndex < virtualMinFrame) {
                virtualReadIndex += virtualDeltaFrames;
                if (renderSilenceAndFinishIfNotLooping(bus, writeIndex, framesToProcess))
                    break;
            }
        }
    } else {
        while (framesToProcess--) {
            unsigned readIndex = static_cast<unsigned>(virtualReadIndex);
            double interpolationFactor = virtualReadIndex - readIndex;

            // For linear interpolation we need the next sample-frame too.
            unsigned readIndex2 = readIndex + 1;
            if (readIndex2 >= bufferLength) {
                if (loop()) {
                    // Make sure to wrap around at the end of the buffer.
                    readIndex2 = static_cast<unsigned>(virtualReadIndex + 1 - virtualDeltaFrames);
                } else
                    readIndex2 = readIndex;
            }

            // Final sanity check on buffer access.
            // FIXME: as an optimization, try to get rid of this inner-loop check and put assertions and guards before the loop.
            if (readIndex >= bufferLength || readIndex2 >= bufferLength)
                break;

            // Linear interpolation.
            for (unsigned i = 0; i < numberOfChannels; ++i) {
                float* destination = destinationChannels[i];
                const float* source = sourceChannels[i];

                double sample1 = source[readIndex];
                double sample2 = source[readIndex2];
                double sample = (1.0 - interpolationFactor) * sample1 + interpolationFactor * sample2;

                destination[writeIndex] = narrowPrecisionToFloat(sample);
            }
            writeIndex++;

            virtualReadIndex += pitchRate;

            // Wrap-around, retaining sub-sample position since virtualReadIndex is floating-point.
            if (virtualReadIndex >= virtualMaxFrame) {
                virtualReadIndex -= virtualDeltaFrames;
                if (renderSilenceAndFinishIfNotLooping(bus, writeIndex, framesToProcess))
                    break;
            }
        }
    }

    bus->clearSilentFlag();

    m_virtualReadIndex = virtualReadIndex;

    return true;
}


void AudioBufferSourceNode::reset()
{
    m_virtualReadIndex = 0;
}

ExceptionOr<void> AudioBufferSourceNode::setBuffer(RefPtr<AudioBuffer>&& buffer)
{
    ASSERT(isMainThread());
    DEBUG_LOG(LOGIDENTIFIER);

    if (buffer && m_wasBufferSet && shouldThrowOnAttemptToOverwriteBuffer())
        return Exception { InvalidStateError, "The buffer was already set"_s };

    // The context must be locked since changing the buffer can re-configure the number of channels that are output.
    BaseAudioContext::AutoLocker contextLocker(context());
    
    // This synchronizes with process().
    auto locker = holdLock(m_processMutex);
    
    if (buffer) {
        m_wasBufferSet = true;

        // Do any necesssary re-configuration to the buffer's number of channels.
        unsigned numberOfChannels = buffer->numberOfChannels();
        ASSERT(numberOfChannels <= AudioContext::maxNumberOfChannels());

        output(0)->setNumberOfChannels(numberOfChannels);

        m_sourceChannels = makeUniqueArray<const float*>(numberOfChannels);
        m_destinationChannels = makeUniqueArray<float*>(numberOfChannels);

        for (unsigned i = 0; i < numberOfChannels; ++i) 
            m_sourceChannels[i] = buffer->channelData(i)->data();
    }

    m_virtualReadIndex = 0;
    m_buffer = WTFMove(buffer);
    return { };
}

unsigned AudioBufferSourceNode::numberOfChannels()
{
    return output(0)->numberOfChannels();
}

ExceptionOr<void> AudioBufferSourceNode::startLater(double when, double grainOffset, Optional<double> optionalGrainDuration)
{
    double grainDuration = 0;
    if (optionalGrainDuration)
        grainDuration = optionalGrainDuration.value();
    else if (buffer())
        grainDuration = buffer()->duration() - grainOffset;

    return startPlaying(when, grainOffset, grainDuration);
}

ExceptionOr<void> AudioBufferSourceNode::startPlaying(double when, double grainOffset, double grainDuration)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "when = ", when, ", offset = ", grainOffset, ", duration = ", grainDuration);

    context().nodeWillBeginPlayback();

    if (m_playbackState != UNSCHEDULED_STATE)
        return Exception { InvalidStateError, "Cannot call start more than once."_s };

    if (!std::isfinite(when) || (when < 0))
        return Exception { RangeError, "when value should be positive"_s };

    if (!std::isfinite(grainOffset) || (grainOffset < 0))
        return Exception { RangeError, "offset value should be positive"_s };

    if (!std::isfinite(grainDuration) || (grainDuration < 0))
        return Exception { RangeError, "duration value should be positive"_s };

    m_isGrain = true;
    m_grainOffset = grainOffset;
    m_grainDuration = grainDuration;
    m_startTime = when;

    if (buffer()) {
        // Do sanity checking of grain parameters versus buffer size.
        double bufferDuration = buffer()->duration();

        m_grainOffset = std::min(bufferDuration, grainOffset);

        double maxDuration = bufferDuration - m_grainOffset;
        m_grainDuration = std::min(maxDuration, grainDuration);

        // We call timeToSampleFrame here since at playbackRate == 1 we don't want to go through linear interpolation
        // at a sub-sample position since it will degrade the quality.
        // When aligned to the sample-frame the playback will be identical to the PCM data stored in the buffer.
        // Since playbackRate == 1 is very common, it's worth considering quality.
        if (playbackRate().value() < 0)
            m_virtualReadIndex = AudioUtilities::timeToSampleFrame(m_grainOffset + m_grainDuration, buffer()->sampleRate()) - 1;
        else
            m_virtualReadIndex = AudioUtilities::timeToSampleFrame(m_grainOffset, buffer()->sampleRate());
    }
    m_playbackState = SCHEDULED_STATE;

    return { };
}

double AudioBufferSourceNode::totalPitchRate()
{
    double dopplerRate = 1.0;
    if (m_pannerNode)
        dopplerRate = m_pannerNode->dopplerRate();
    
    // Incorporate buffer's sample-rate versus AudioContext's sample-rate.
    // Normally it's not an issue because buffers are loaded at the AudioContext's sample-rate, but we can handle it in any case.
    double sampleRateFactor = 1.0;
    if (buffer())
        sampleRateFactor = buffer()->sampleRate() / static_cast<double>(sampleRate());
    
    double basePitchRate = playbackRate().finalValue();
    double detune = pow(2, m_detune->finalValue() / 1200);

    double totalRate = dopplerRate * sampleRateFactor * basePitchRate * detune;

    totalRate = std::clamp(totalRate, -MaxRate, MaxRate);
    
    bool isTotalRateValid = !std::isnan(totalRate) && !std::isinf(totalRate);
    ASSERT(isTotalRateValid);
    if (!isTotalRateValid)
        totalRate = 1.0;

    return totalRate;
}

bool AudioBufferSourceNode::propagatesSilence() const
{
    return !isPlayingOrScheduled() || hasFinished() || !m_buffer;
}

void AudioBufferSourceNode::setPannerNode(PannerNodeBase* pannerNode)
{
    if (m_pannerNode != pannerNode && !hasFinished()) {
        if (pannerNode)
            pannerNode->ref(AudioNode::RefTypeConnection);
        if (m_pannerNode)
            m_pannerNode->deref(AudioNode::RefTypeConnection);

        m_pannerNode = pannerNode;
    }
}

void AudioBufferSourceNode::clearPannerNode()
{
    if (m_pannerNode) {
        m_pannerNode->deref(AudioNode::RefTypeConnection);
        m_pannerNode = nullptr;
    }
}

void AudioBufferSourceNode::finish()
{
    clearPannerNode();
    ASSERT(!m_pannerNode);
    AudioScheduledSourceNode::finish();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
