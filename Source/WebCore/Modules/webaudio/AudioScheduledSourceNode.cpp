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

#include "AudioScheduledSourceNode.h"

#include "AudioContext.h"
#include "AudioUtilities.h"
#include <algorithm>
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

const double AudioScheduledSourceNode::UnknownTime = -1;

AudioScheduledSourceNode::AudioScheduledSourceNode(AudioContext* context, float sampleRate)
    : AudioSourceNode(context, sampleRate)
    , m_playbackState(UNSCHEDULED_STATE)
    , m_startTime(0)
    , m_endTime(UnknownTime)
{
}

void AudioScheduledSourceNode::updateSchedulingInfo(size_t quantumFrameSize,
                                                    size_t& quantumStartFrame,
                                                    size_t& quantumEndFrame,
                                                    size_t& startFrame,
                                                    size_t& endFrame,
                                                    size_t& quantumFrameOffset,
                                                    size_t& nonSilentFramesToProcess)
{
    ASSERT(quantumFrameSize == AudioNode::ProcessingSizeInFrames);
    if (quantumFrameSize != AudioNode::ProcessingSizeInFrames)
        return;
    
    // Check if it's time to start playing.
    double sampleRate = this->sampleRate();
    quantumStartFrame = context()->currentSampleFrame();
    quantumEndFrame = quantumStartFrame + quantumFrameSize;
    startFrame = AudioUtilities::timeToSampleFrame(m_startTime, sampleRate);
    endFrame = m_endTime == UnknownTime ? 0 : AudioUtilities::timeToSampleFrame(m_endTime, sampleRate);

    // If we know the end time and it's already passed, then don't bother doing any more rendering this cycle.
    if (m_endTime != UnknownTime && endFrame <= quantumStartFrame)
        finish();

    if (m_playbackState == UNSCHEDULED_STATE || m_playbackState == FINISHED_STATE || startFrame >= quantumEndFrame) {
        // Output silence.
        nonSilentFramesToProcess = 0;
        return;
    }

    if (m_playbackState == SCHEDULED_STATE) {
        // Increment the active source count only if we're transitioning from SCHEDULED_STATE to PLAYING_STATE.
        m_playbackState = PLAYING_STATE;
        context()->incrementActiveSourceCount();
    }

    quantumFrameOffset = startFrame > quantumStartFrame ? startFrame - quantumStartFrame : 0;
    quantumFrameOffset = min(quantumFrameOffset, quantumFrameSize); // clamp to valid range
    nonSilentFramesToProcess = quantumFrameSize - quantumFrameOffset;

    return;
}

void AudioScheduledSourceNode::noteOn(double when)
{
    ASSERT(isMainThread());
    if (m_playbackState != UNSCHEDULED_STATE)
        return;

    m_startTime = when;
    m_playbackState = SCHEDULED_STATE;
}

void AudioScheduledSourceNode::noteOff(double when)
{
    ASSERT(isMainThread());
    if (!(m_playbackState == SCHEDULED_STATE || m_playbackState == PLAYING_STATE))
        return;
    
    when = max(0.0, when);
    m_endTime = when;
}

void AudioScheduledSourceNode::finish()
{
    if (m_playbackState != FINISHED_STATE) {
        // Let the context dereference this AudioNode.
        context()->notifyNodeFinishedProcessing(this);
        m_playbackState = FINISHED_STATE;
        context()->decrementActiveSourceCount();
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
