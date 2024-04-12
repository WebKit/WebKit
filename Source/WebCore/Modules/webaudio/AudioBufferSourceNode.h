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

#pragma once

#include "AudioBufferSourceOptions.h"
#include "AudioScheduledSourceNode.h"
#include <wtf/Lock.h>
#include <wtf/UniqueArray.h>

namespace WebCore {

class AudioBuffer;
struct AudioBufferSourceOptions;

// AudioBufferSourceNode is an AudioNode representing an audio source from an in-memory audio asset represented by an AudioBuffer.
// It generally will be used for short sounds which require a high degree of scheduling flexibility (can playback in rhythmically perfect ways).

class AudioBufferSourceNode final : public AudioScheduledSourceNode {
    WTF_MAKE_ISO_ALLOCATED(AudioBufferSourceNode);
public:
    static ExceptionOr<Ref<AudioBufferSourceNode>> create(BaseAudioContext&, AudioBufferSourceOptions&& = { });

    virtual ~AudioBufferSourceNode();

    // AudioNode
    void process(size_t framesToProcess) final;

    // setBufferForBindings() is called on the main thread. This is the buffer we use for playback.
    ExceptionOr<void> setBufferForBindings(RefPtr<AudioBuffer>&&);

    AudioBuffer* buffer() WTF_REQUIRES_LOCK(m_processLock) { return m_buffer.get(); }
    Lock& processLock() WTF_RETURNS_LOCK(m_processLock) { return m_processLock; }

    // This function does not lock before accessing the buffer and should therefore only be called on the main thread.
    AudioBuffer* bufferForBindings() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_buffer.get(); }

    // numberOfChannels() returns the number of output channels.  This value equals the number of channels from the buffer.
    // If a new buffer is set with a different number of channels, then this value will dynamically change.
    unsigned numberOfChannels();

    // Play-state
    ExceptionOr<void> startLater(double when, double grainOffset, std::optional<double> grainDuration);

    // This function doesn't grab the lock before accessing m_isLooping and is thus only safe to call on the main thread.
    bool loopForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_isLooping; }
    void setLoopForBindings(bool);

    // Loop times in seconds.
    double loopStartForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_loopStart; } // This function doesn't grab the lock and is thus only safe to call on the main thread.
    double loopEndForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_loopEnd; } // This function doesn't grab the lock and is thus only safe to call on the main thread.
    void setLoopStartForBindings(double);
    void setLoopEndForBindings(double);

    AudioParam& detune() { return m_detune.get(); }
    AudioParam& playbackRate() { return m_playbackRate.get(); }

    // If we are no longer playing, propogate silence ahead to downstream nodes.
    bool propagatesSilence() const final;

    const char* activeDOMObjectName() const override { return "AudioBufferSourceNode"; }

private:
    AudioBufferSourceNode(BaseAudioContext&);

    double tailTime() const final { return 0; }
    double latencyTime() const final { return 0; }

    float noiseInjectionMultiplier() const final;

    ExceptionOr<void> startPlaying(double when, double grainOffset, std::optional<double> grainDuration);
    void adjustGrainParameters() WTF_REQUIRES_LOCK(m_processLock);

    // Returns true on success.
    bool renderFromBuffer(AudioBus*, unsigned destinationFrameOffset, size_t numberOfFrames, double startFrameOffset) WTF_REQUIRES_LOCK(m_processLock);

    // Render silence starting from "index" frame in AudioBus.
    inline bool renderSilenceAndFinishIfNotLooping(AudioBus*, unsigned index, size_t framesToProcess) WTF_REQUIRES_LOCK(m_processLock);

    // m_buffer holds the sample data which this node outputs.
    RefPtr<AudioBuffer> m_buffer WTF_GUARDED_BY_LOCK(m_processLock); // Only modified on the main thread but used on the audio thread.

    // Pointers for the buffer and destination.
    UniqueArray<const float*> m_sourceChannels;
    UniqueArray<float*> m_destinationChannels;

    Ref<AudioParam> m_detune;
    Ref<AudioParam> m_playbackRate;

    // If m_isLooping is false, then this node will be done playing and become inactive after it reaches the end of the sample data in the buffer.
    // If true, it will wrap around to the start of the buffer each time it reaches the end.
    bool m_isLooping WTF_GUARDED_BY_LOCK(m_processLock) { false }; // Only modified on the main thread but queried on the audio thread.

    bool m_wasBufferSet { false };

    double m_loopStart WTF_GUARDED_BY_LOCK(m_processLock) { 0 }; // Only modified on the main thread but queried on the audio thread.
    double m_loopEnd WTF_GUARDED_BY_LOCK(m_processLock) { 0 }; // Only modified on the main thread but queried on the audio thread.

    // m_virtualReadIndex is a sample-frame index into our buffer representing the current playback position.
    // Since it's floating-point, it has sub-sample accuracy.
    double m_virtualReadIndex { 0 };

    // Granular playback
    bool m_isGrain WTF_GUARDED_BY_LOCK(m_processLock) { false };
    double m_grainOffset WTF_GUARDED_BY_LOCK(m_processLock) { 0 }; // in seconds
    double m_grainDuration WTF_GUARDED_BY_LOCK(m_processLock); // in seconds
    double m_wasGrainDurationGiven WTF_GUARDED_BY_LOCK(m_processLock) { false };

    // totalPitchRate() returns the instantaneous pitch rate (non-time preserving).
    // It incorporates the base pitch rate, any sample-rate conversion factor from the buffer.
    double totalPitchRate() WTF_REQUIRES_LOCK(m_processLock);

    // This synchronizes process() with setBuffer() which can cause dynamic channel count changes.
    mutable Lock m_processLock;
};

} // namespace WebCore
