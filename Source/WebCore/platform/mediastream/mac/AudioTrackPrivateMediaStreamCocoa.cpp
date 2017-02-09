/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AudioTrackPrivateMediaStreamCocoa.h"

#include "AudioSampleBufferList.h"
#include "AudioSampleDataSource.h"
#include "AudioSession.h"
#include "CAAudioStreamDescription.h"
#include "Logging.h"

#include "CoreMediaSoftLink.h"

#if ENABLE(VIDEO_TRACK)

namespace WebCore {

const int renderBufferSize = 128;

AudioTrackPrivateMediaStreamCocoa::AudioTrackPrivateMediaStreamCocoa(MediaStreamTrackPrivate& track)
    : AudioTrackPrivateMediaStream(track)
{
    track.source().addObserver(*this);
}

AudioTrackPrivateMediaStreamCocoa::~AudioTrackPrivateMediaStreamCocoa()
{
    std::lock_guard<Lock> lock(m_internalStateLock);

    streamTrack().source().removeObserver(*this);

    if (m_dataSource)
        m_dataSource->setPaused(true);

    if (m_remoteIOUnit) {
        AudioOutputUnitStop(m_remoteIOUnit);
        AudioComponentInstanceDispose(m_remoteIOUnit);
        m_remoteIOUnit = nullptr;
    }

    m_dataSource = nullptr;
    m_inputDescription = nullptr;
    m_outputDescription = nullptr;
}

void AudioTrackPrivateMediaStreamCocoa::playInternal()
{
    ASSERT(m_internalStateLock.isHeld());

    if (m_isPlaying)
        return;

    if (m_remoteIOUnit) {
        ASSERT(m_dataSource);
        m_dataSource->setPaused(false);
        if (!AudioOutputUnitStart(m_remoteIOUnit))
            m_isPlaying = true;
    }

    m_autoPlay = !m_isPlaying;
}

void AudioTrackPrivateMediaStreamCocoa::play()
{
    std::lock_guard<Lock> lock(m_internalStateLock);
    playInternal();
}

void AudioTrackPrivateMediaStreamCocoa::pause()
{
    std::lock_guard<Lock> lock(m_internalStateLock);

    m_isPlaying = false;
    m_autoPlay = false;

    if (m_remoteIOUnit)
        AudioOutputUnitStop(m_remoteIOUnit);
    if (m_dataSource)
        m_dataSource->setPaused(true);
}

void AudioTrackPrivateMediaStreamCocoa::setVolume(float volume)
{
    m_volume = volume;
    if (m_dataSource)
        m_dataSource->setVolume(m_volume);
}

OSStatus AudioTrackPrivateMediaStreamCocoa::setupAudioUnit()
{
    ASSERT(m_internalStateLock.isHeld());

    AudioComponentDescription ioUnitDescription { kAudioUnitType_Output, 0, kAudioUnitManufacturer_Apple, 0, 0 };
#if PLATFORM(IOS)
    ioUnitDescription.componentSubType = kAudioUnitSubType_RemoteIO;
#else
    ioUnitDescription.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif

    AudioComponent ioComponent = AudioComponentFindNext(nullptr, &ioUnitDescription);
    ASSERT(ioComponent);
    if (!ioComponent) {
        LOG(Media, "AudioTrackPrivateMediaStreamCocoa::setupAudioUnit(%p) unable to find remote IO unit component", this);
        return -1;
    }

    OSStatus err = AudioComponentInstanceNew(ioComponent, &m_remoteIOUnit);
    if (err) {
        LOG(Media, "AudioTrackPrivateMediaStreamCocoa::setupAudioUnit(%p) unable to open vpio unit, error %d (%.4s)", this, (int)err, (char*)&err);
        return -1;
    }

#if PLATFORM(IOS)
    UInt32 param = 1;
    err = AudioUnitSetProperty(m_remoteIOUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &param, sizeof(param));
    if (err) {
        LOG(Media, "AudioTrackPrivateMediaStreamCocoa::setupAudioUnit(%p) unable to enable vpio unit output, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
#endif

    AURenderCallbackStruct callback = { inputProc, this };
    err = AudioUnitSetProperty(m_remoteIOUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global, 0, &callback, sizeof(callback));
    if (err) {
        LOG(Media, "AudioTrackPrivateMediaStreamCocoa::setupAudioUnit(%p) unable to set vpio unit speaker proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription outputDescription = { };
    UInt32 size = sizeof(outputDescription);
    err  = AudioUnitGetProperty(m_remoteIOUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &outputDescription, &size);
    if (err) {
        LOG(Media, "AudioTrackPrivateMediaStreamCocoa::setupAudioUnits(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    outputDescription = m_inputDescription->streamDescription();
    outputDescription.mSampleRate = AudioSession::sharedSession().sampleRate();

    err = AudioUnitSetProperty(m_remoteIOUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &outputDescription, sizeof(outputDescription));
    if (err) {
        LOG(Media, "AudioTrackPrivateMediaStreamCocoa::setupAudioUnits(%p) unable to set input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
    m_outputDescription = std::make_unique<CAAudioStreamDescription>(outputDescription);

    err = AudioUnitInitialize(m_remoteIOUnit);
    if (err) {
        LOG(Media, "AudioTrackPrivateMediaStreamCocoa::setupAudioUnits(%p) AudioUnitInitialize() failed, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioSession::sharedSession().setPreferredBufferSize(renderBufferSize);

    return err;
}

void AudioTrackPrivateMediaStreamCocoa::audioSamplesAvailable(const MediaTime& sampleTime, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t sampleCount)
{
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);

    std::lock_guard<Lock> lock(m_internalStateLock);

    CAAudioStreamDescription streamDescription = toCAAudioStreamDescription(description);
    if (!m_inputDescription || *m_inputDescription != description) {

        m_inputDescription = nullptr;
        m_outputDescription = nullptr;

        if (m_remoteIOUnit) {
            AudioOutputUnitStop(m_remoteIOUnit);
            AudioComponentInstanceDispose(m_remoteIOUnit);
            m_remoteIOUnit = nullptr;
        }

        m_inputDescription = std::make_unique<CAAudioStreamDescription>(streamDescription);
        if (setupAudioUnit()) {
            m_inputDescription = nullptr;
            return;
        }

        if (!m_dataSource)
            m_dataSource = AudioSampleDataSource::create(description.sampleRate() * 2);
        if (!m_dataSource)
            return;

        if (m_dataSource->setInputFormat(streamDescription))
            return;
        if (m_dataSource->setOutputFormat(*m_outputDescription.get()))
            return;

        m_dataSource->setVolume(m_volume);
    }

    m_dataSource->pushSamples(sampleTime, audioData, sampleCount);

    if (m_autoPlay)
        playInternal();
}

void AudioTrackPrivateMediaStreamCocoa::sourceStopped()
{
    pause();
}

OSStatus AudioTrackPrivateMediaStreamCocoa::render(UInt32 sampleCount, AudioBufferList& ioData, UInt32 /*inBusNumber*/, const AudioTimeStamp& timeStamp, AudioUnitRenderActionFlags& actionFlags)
{
    std::unique_lock<Lock> lock(m_internalStateLock, std::try_to_lock);
    if (!lock.owns_lock())
        return kAudioConverterErr_UnspecifiedError;

    if (!m_isPlaying || m_muted || !m_dataSource || streamTrack().muted() || streamTrack().ended() || !streamTrack().enabled()) {
        AudioSampleBufferList::zeroABL(ioData, static_cast<size_t>(sampleCount));
        actionFlags = kAudioUnitRenderAction_OutputIsSilence;
        return 0;
    }

    m_dataSource->pullSamples(ioData, static_cast<size_t>(sampleCount), timeStamp.mSampleTime, timeStamp.mHostTime, AudioSampleDataSource::Copy);

    return 0;
}

OSStatus AudioTrackPrivateMediaStreamCocoa::inputProc(void* userData, AudioUnitRenderActionFlags* actionFlags, const AudioTimeStamp* timeStamp, UInt32 inBusNumber, UInt32 sampleCount, AudioBufferList* ioData)
{
    return static_cast<AudioTrackPrivateMediaStreamCocoa*>(userData)->render(sampleCount, *ioData, inBusNumber, *timeStamp, *actionFlags);
}


} // namespace WebCore

#endif // ENABLE(VIDEO_TRACK)
