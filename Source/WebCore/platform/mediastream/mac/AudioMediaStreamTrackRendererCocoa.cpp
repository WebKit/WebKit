/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "AudioMediaStreamTrackRendererCocoa.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioSampleBufferList.h"
#include "AudioSampleDataSource.h"
#include "AudioSession.h"
#include "CAAudioStreamDescription.h"
#include "Logging.h"

#include <pal/cf/CoreMediaSoftLink.h>
#include <pal/spi/cocoa/AudioToolboxSPI.h>

namespace WebCore {

AudioMediaStreamTrackRendererCocoa::AudioMediaStreamTrackRendererCocoa() = default;

AudioMediaStreamTrackRendererCocoa::~AudioMediaStreamTrackRendererCocoa() = default;

void AudioMediaStreamTrackRendererCocoa::start()
{
    clear();

    CAAudioStreamDescription outputDescription;
    auto remoteIOUnit = createAudioUnit(outputDescription);
    if (!remoteIOUnit)
        return;

    if (auto error = AudioOutputUnitStart(remoteIOUnit)) {
        ERROR_LOG(LOGIDENTIFIER, "AudioOutputUnitStart failed, error = ", error, " (", (const char*)&error, ")");
        AudioComponentInstanceDispose(remoteIOUnit);
        return;
    }
    m_outputDescription = makeUnique<CAAudioStreamDescription>(outputDescription);
    m_remoteIOUnit = remoteIOUnit;
}

void AudioMediaStreamTrackRendererCocoa::stop()
{
    if (!m_remoteIOUnit)
        return;

    AudioOutputUnitStop(m_remoteIOUnit);
    AudioComponentInstanceDispose(m_remoteIOUnit);
    m_remoteIOUnit = nullptr;
}

void AudioMediaStreamTrackRendererCocoa::clear()
{
    stop();

    m_dataSource = nullptr;
    m_outputDescription = nullptr;
}

AudioComponentInstance AudioMediaStreamTrackRendererCocoa::createAudioUnit(CAAudioStreamDescription& outputDescription)
{
    AudioComponentInstance remoteIOUnit { nullptr };

    AudioComponentDescription ioUnitDescription { kAudioUnitType_Output, 0, kAudioUnitManufacturer_Apple, 0, 0 };
#if PLATFORM(IOS_FAMILY)
    ioUnitDescription.componentSubType = kAudioUnitSubType_RemoteIO;
#else
    ioUnitDescription.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif

    AudioComponent ioComponent = AudioComponentFindNext(nullptr, &ioUnitDescription);
    ASSERT(ioComponent);
    if (!ioComponent) {
        ERROR_LOG(LOGIDENTIFIER, "unable to find remote IO unit component");
        return nullptr;
    }

    OSStatus err = AudioComponentInstanceNew(ioComponent, &remoteIOUnit);
    if (err) {
        ERROR_LOG(LOGIDENTIFIER, "unable to open vpio unit, error = ", err, " (", (const char*)&err, ")");
        return nullptr;
    }

#if PLATFORM(IOS_FAMILY)
    UInt32 param = 1;
    err = AudioUnitSetProperty(remoteIOUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &param, sizeof(param));
    if (err) {
        ERROR_LOG(LOGIDENTIFIER, "unable to enable vpio unit output, error = ", err, " (", (const char*)&err, ")");
        return nullptr;
    }
#endif

    AURenderCallbackStruct callback = { inputProc, this };
    err = AudioUnitSetProperty(remoteIOUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global, 0, &callback, sizeof(callback));
    if (err) {
        ERROR_LOG(LOGIDENTIFIER, "unable to set vpio unit speaker proc, error = ", err, " (", (const char*)&err, ")");
        return nullptr;
    }

    UInt32 size = sizeof(outputDescription.streamDescription());
    err  = AudioUnitGetProperty(remoteIOUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &outputDescription.streamDescription(), &size);
    if (err) {
        ERROR_LOG(LOGIDENTIFIER, "unable to get input stream format, error = ", err, " (", (const char*)&err, ")");
        return nullptr;
    }

    outputDescription.streamDescription().mSampleRate = AudioSession::sharedSession().sampleRate();

    err = AudioUnitSetProperty(remoteIOUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &outputDescription.streamDescription(), sizeof(outputDescription.streamDescription()));
    if (err) {
        ERROR_LOG(LOGIDENTIFIER, "unable to set input stream format, error = ", err, " (", (const char*)&err, ")");
        return nullptr;
    }

    err = AudioUnitInitialize(remoteIOUnit);
    if (err) {
        ERROR_LOG(LOGIDENTIFIER, "AudioUnitInitialize() failed, error = ", err, " (", (const char*)&err, ")");
        return nullptr;
    }

    return remoteIOUnit;
}

void AudioMediaStreamTrackRendererCocoa::pushSamples(const MediaTime& sampleTime, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t sampleCount)
{
    ASSERT(!isMainThread());
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    if (!m_remoteIOUnit)
        return;

    if (!m_dataSource || !m_dataSource->inputDescription() || *m_dataSource->inputDescription() != description) {
        if (m_shouldUpdateRendererDataSource)
            return;

        auto dataSource = AudioSampleDataSource::create(description.sampleRate() * 2, *this);

        if (dataSource->setInputFormat(toCAAudioStreamDescription(description))) {
            ERROR_LOG(LOGIDENTIFIER, "Unable to set the input format of data source");
            return;
        }

        if (dataSource->setOutputFormat(*m_outputDescription)) {
            ERROR_LOG(LOGIDENTIFIER, "Unable to set the output format of data source");
            return;
        }

        dataSource->setPaused(false);
        dataSource->setVolume(volume());

        m_dataSource = WTFMove(dataSource);
        m_shouldUpdateRendererDataSource = true;
    }

    m_dataSource->pushSamples(sampleTime, audioData, sampleCount);
}

OSStatus AudioMediaStreamTrackRendererCocoa::render(UInt32 sampleCount, AudioBufferList& ioData, UInt32 /*inBusNumber*/, const AudioTimeStamp& timeStamp, AudioUnitRenderActionFlags& actionFlags)
{
    ASSERT(!isMainThread());
    if (m_shouldUpdateRendererDataSource) {
        m_rendererDataSource = m_dataSource;
        m_shouldUpdateRendererDataSource = false;
    }

    if (!m_rendererDataSource) {
        AudioSampleBufferList::zeroABL(ioData, static_cast<size_t>(sampleCount * m_outputDescription->bytesPerFrame()));
        actionFlags = kAudioUnitRenderAction_OutputIsSilence;
        return 0;
    }

    m_rendererDataSource->pullSamples(ioData, static_cast<size_t>(sampleCount), timeStamp.mSampleTime, timeStamp.mHostTime, AudioSampleDataSource::Copy);
    return 0;
}

OSStatus AudioMediaStreamTrackRendererCocoa::inputProc(void* userData, AudioUnitRenderActionFlags* actionFlags, const AudioTimeStamp* timeStamp, UInt32 inBusNumber, UInt32 sampleCount, AudioBufferList* ioData)
{
    return static_cast<AudioMediaStreamTrackRendererCocoa*>(userData)->render(sampleCount, *ioData, inBusNumber, *timeStamp, *actionFlags);
}


} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
