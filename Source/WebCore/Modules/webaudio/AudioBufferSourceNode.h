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

#ifndef AudioBufferSourceNode_h
#define AudioBufferSourceNode_h

#include "AudioBuffer.h"
#include "AudioBus.h"
#include "AudioGain.h"
#include "AudioPannerNode.h"
#include "AudioSourceNode.h"
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

class AudioContext;

// AudioBufferSourceNode is an AudioNode representing an audio source from an in-memory audio asset represented by an AudioBuffer.
// It generally will be used for short sounds which require a high degree of scheduling flexibility (can playback in rhythmically perfect ways).

class AudioBufferSourceNode : public AudioSourceNode {
public:
    // These are the possible states an AudioBufferSourceNode can be in:
    //
    // UNSCHEDULED_STATE - Initial playback state. Created, but not yet scheduled.
    // SCHEDULED_STATE - Scheduled to play (via noteOn() or noteGrainOn()), but not yet playing.
    // PLAYING_STATE - Generating sound.
    // FINISHED_STATE - Finished generating sound.
    //
    // The state can only transition to the next state, except for the FINISHED_STATE which can
    // never be changed.
    enum PlaybackState {
        // These must be defined with the same names and values as in the .idl file.
        UNSCHEDULED_STATE = 0,
        SCHEDULED_STATE = 1,
        PLAYING_STATE = 2,
        FINISHED_STATE = 3
    };
    
    static PassRefPtr<AudioBufferSourceNode> create(AudioContext*, float sampleRate);

    virtual ~AudioBufferSourceNode();
    
    // AudioNode
    virtual void process(size_t framesToProcess);
    virtual void reset();
    
    // setBuffer() is called on the main thread.  This is the buffer we use for playback.
    // returns true on success.
    bool setBuffer(AudioBuffer*);
    AudioBuffer* buffer() { return m_buffer.get(); }
                    
    // numberOfChannels() returns the number of output channels.  This value equals the number of channels from the buffer.
    // If a new buffer is set with a different number of channels, then this value will dynamically change.
    unsigned numberOfChannels();
                    
    // Play-state
    // noteOn(), noteGrainOn(), and noteOff() must all be called from the main thread.
    void noteOn(double when);
    void noteGrainOn(double when, double grainOffset, double grainDuration);
    void noteOff(double when);

    unsigned short playbackState() const { return static_cast<unsigned short>(m_playbackState); }
    bool isPlayingOrScheduled() const { return m_playbackState == PLAYING_STATE || m_playbackState == SCHEDULED_STATE; }
    bool hasFinished() const { return m_playbackState == FINISHED_STATE; }

    // Note: the attribute was originally exposed as .looping, but to be more consistent in naming with <audio>
    // and with how it's described in the specification, the proper attribute name is .loop
    // The old attribute is kept for backwards compatibility.
    bool loop() const { return m_isLooping; }
    void setLoop(bool looping) { m_isLooping = looping; }

    // Deprecated.
    bool looping();
    void setLooping(bool);
    
    AudioGain* gain() { return m_gain.get(); }                                        
    AudioParam* playbackRate() { return m_playbackRate.get(); }

    // If a panner node is set, then we can incorporate doppler shift into the playback pitch rate.
    void setPannerNode(PassRefPtr<AudioPannerNode> pannerNode) { m_pannerNode = pannerNode; }

    // If we are no longer playing, propogate silence ahead to downstream nodes.
    virtual bool propagatesSilence() const;

private:
    AudioBufferSourceNode(AudioContext*, float sampleRate);

    void renderFromBuffer(AudioBus*, unsigned destinationFrameOffset, size_t numberOfFrames);

    // Render silence starting from "index" frame in AudioBus.
    inline bool renderSilenceAndFinishIfNotLooping(AudioBus*, unsigned index, size_t framesToProcess);

    // m_buffer holds the sample data which this node outputs.
    RefPtr<AudioBuffer> m_buffer;

    // Pointers for the buffer and destination.
    OwnArrayPtr<const float*> m_sourceChannels;
    OwnArrayPtr<float*> m_destinationChannels;

    // Used for the "gain" and "playbackRate" attributes.
    RefPtr<AudioGain> m_gain;
    RefPtr<AudioParam> m_playbackRate;

    // If m_isLooping is false, then this node will be done playing and become inactive after it reaches the end of the sample data in the buffer.
    // If true, it will wrap around to the start of the buffer each time it reaches the end.
    bool m_isLooping;

    // m_startTime is the time to start playing based on the context's timeline (0.0 or a time less than the context's current time means "now").
    double m_startTime; // in seconds

    // m_endTime is the time to stop playing based on the context's timeline (0.0 or a time less than the context's current time means "now").
    // If it hasn't been set explicitly, then the sound will not stop playing (if looping) or will stop when the end of the AudioBuffer
    // has been reached.
    double m_endTime; // in seconds
    
    // m_virtualReadIndex is a sample-frame index into our buffer representing the current playback position.
    // Since it's floating-point, it has sub-sample accuracy.
    double m_virtualReadIndex;

    // Granular playback
    bool m_isGrain;
    double m_grainOffset; // in seconds
    double m_grainDuration; // in seconds

    // totalPitchRate() returns the instantaneous pitch rate (non-time preserving).
    // It incorporates the base pitch rate, any sample-rate conversion factor from the buffer, and any doppler shift from an associated panner node.
    double totalPitchRate();

    // m_lastGain provides continuity when we dynamically adjust the gain.
    float m_lastGain;
    
    // We optionally keep track of a panner node which has a doppler shift that is incorporated into the pitch rate.
    RefPtr<AudioPannerNode> m_pannerNode;

    // This synchronizes process() with setBuffer() which can cause dynamic channel count changes.
    mutable Mutex m_processLock;

    // Handles the time when we reach the end of sample data (non-looping) or the noteOff() time has been reached.
    void finish();

    PlaybackState m_playbackState;
};

} // namespace WebCore

#endif // AudioBufferSourceNode_h
