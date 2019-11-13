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
#include "Event.h"
#include "EventNames.h"
#include "ScriptController.h"
#include "ScriptExecutionContext.h"
#include <algorithm>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/Scope.h>

#if PLATFORM(IOS_FAMILY)
#include "ScriptController.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioScheduledSourceNode);

const double AudioScheduledSourceNode::UnknownTime = -1;

AudioScheduledSourceNode::AudioScheduledSourceNode(AudioContext& context, float sampleRate)
    : AudioNode(context, sampleRate)
    , ActiveDOMObject(context.scriptExecutionContext())
    , m_endTime(UnknownTime)
{
    suspendIfNeeded();
    m_pendingActivity = makePendingActivity(*this);
}

void AudioScheduledSourceNode::updateSchedulingInfo(size_t quantumFrameSize, AudioBus& outputBus, size_t& quantumFrameOffset, size_t& nonSilentFramesToProcess)
{
    nonSilentFramesToProcess = 0;
    quantumFrameOffset = 0;

    ASSERT(quantumFrameSize == AudioNode::ProcessingSizeInFrames);
    if (quantumFrameSize != AudioNode::ProcessingSizeInFrames)
        return;

    double sampleRate = this->sampleRate();

    // quantumStartFrame     : Start frame of the current time quantum.
    // quantumEndFrame       : End frame of the current time quantum.
    // startFrame            : Start frame for this source.
    // endFrame              : End frame for this source.
    size_t quantumStartFrame = context().currentSampleFrame();
    size_t quantumEndFrame = quantumStartFrame + quantumFrameSize;
    size_t startFrame = AudioUtilities::timeToSampleFrame(m_startTime, sampleRate);
    size_t endFrame = m_endTime == UnknownTime ? 0 : AudioUtilities::timeToSampleFrame(m_endTime, sampleRate);

    // If we know the end time and it's already passed, then don't bother doing any more rendering this cycle.
    if (m_endTime != UnknownTime && endFrame <= quantumStartFrame)
        finish();

    if (m_playbackState == UNSCHEDULED_STATE || m_playbackState == FINISHED_STATE || startFrame >= quantumEndFrame) {
        // Output silence.
        outputBus.zero();
        return;
    }

    // Check if it's time to start playing.
    if (m_playbackState == SCHEDULED_STATE) {
        // Increment the active source count only if we're transitioning from SCHEDULED_STATE to PLAYING_STATE.
        m_playbackState = PLAYING_STATE;
        context().incrementActiveSourceCount();
    }

    quantumFrameOffset = startFrame > quantumStartFrame ? startFrame - quantumStartFrame : 0;
    quantumFrameOffset = std::min(quantumFrameOffset, quantumFrameSize); // clamp to valid range
    nonSilentFramesToProcess = quantumFrameSize - quantumFrameOffset;

    if (!nonSilentFramesToProcess) {
        // Output silence.
        outputBus.zero();
        return;
    }

    // Handle silence before we start playing.
    // Zero any initial frames representing silence leading up to a rendering start time in the middle of the quantum.
    if (quantumFrameOffset) {
        for (unsigned i = 0; i < outputBus.numberOfChannels(); ++i)
            memset(outputBus.channel(i)->mutableData(), 0, sizeof(float) * quantumFrameOffset);
    }

    // Handle silence after we're done playing.
    // If the end time is somewhere in the middle of this time quantum, then zero out the
    // frames from the end time to the very end of the quantum.
    if (m_endTime != UnknownTime && endFrame >= quantumStartFrame && endFrame < quantumEndFrame) {
        size_t zeroStartFrame = endFrame - quantumStartFrame;
        size_t framesToZero = quantumFrameSize - zeroStartFrame;

        bool isSafe = zeroStartFrame < quantumFrameSize && framesToZero <= quantumFrameSize && zeroStartFrame + framesToZero <= quantumFrameSize;
        ASSERT(isSafe);

        if (isSafe) {
            if (framesToZero > nonSilentFramesToProcess)
                nonSilentFramesToProcess = 0;
            else
                nonSilentFramesToProcess -= framesToZero;

            for (unsigned i = 0; i < outputBus.numberOfChannels(); ++i)
                memset(outputBus.channel(i)->mutableData() + zeroStartFrame, 0, sizeof(float) * framesToZero);
        }

        finish();
    }
}

ExceptionOr<void> AudioScheduledSourceNode::startLater(double when)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, when);

    context().nodeWillBeginPlayback();

    if (m_playbackState != UNSCHEDULED_STATE)
        return Exception { InvalidStateError };
    if (!std::isfinite(when) || when < 0)
        return Exception { InvalidStateError };

    m_startTime = when;
    m_playbackState = SCHEDULED_STATE;

    return { };
}

ExceptionOr<void> AudioScheduledSourceNode::stopLater(double when)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, when);

    if (m_playbackState == UNSCHEDULED_STATE || m_endTime != UnknownTime)
        return Exception { InvalidStateError };
    if (!std::isfinite(when) || when < 0)
        return Exception { InvalidStateError };

    m_endTime = when;

    return { };
}

void AudioScheduledSourceNode::didBecomeMarkedForDeletion()
{
    ASSERT(context().isGraphOwner());
    m_pendingActivity = nullptr;
    ASSERT(!hasPendingActivity());
}

void AudioScheduledSourceNode::finish()
{
    ASSERT(!hasFinished());
    // Let the context dereference this AudioNode.
    context().notifyNodeFinishedProcessing(this);
    m_playbackState = FINISHED_STATE;
    context().decrementActiveSourceCount();

    context().postTask([this, protectedThis = makeRef(*this)] {
        auto release = makeScopeExit([&] () {
            AudioContext::AutoLocker locker(context());
            m_pendingActivity = nullptr;
            ASSERT(!hasPendingActivity());
        });
        if (context().isStopped())
            return;
        this->dispatchEvent(Event::create(eventNames().endedEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
