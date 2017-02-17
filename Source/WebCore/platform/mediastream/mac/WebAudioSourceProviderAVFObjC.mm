/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "WebAudioSourceProviderAVFObjC.h"

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

#import "AudioBus.h"
#import "AudioChannel.h"
#import "AudioSourceProviderClient.h"
#import "CARingBuffer.h"
#import "Logging.h"
#import "MediaTimeAVFoundation.h"
#import "WebAudioBufferList.h"
#import <AudioToolbox/AudioToolbox.h>
#import <objc/runtime.h>
#import <wtf/MainThread.h>

#if !LOG_DISABLED
#import <wtf/StringPrintStream.h>
#endif

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK(AudioToolbox)

SOFT_LINK(AudioToolbox, AudioConverterConvertComplexBuffer, OSStatus, (AudioConverterRef inAudioConverter, UInt32 inNumberPCMFrames, const AudioBufferList* inInputData, AudioBufferList* outOutputData), (inAudioConverter, inNumberPCMFrames, inInputData, outOutputData))
SOFT_LINK(AudioToolbox, AudioConverterNew, OSStatus, (const AudioStreamBasicDescription* inSourceFormat, const AudioStreamBasicDescription* inDestinationFormat, AudioConverterRef* outAudioConverter), (inSourceFormat, inDestinationFormat, outAudioConverter))

namespace WebCore {

static const double kRingBufferDuration = 1;

Ref<WebAudioSourceProviderAVFObjC> WebAudioSourceProviderAVFObjC::create(RealtimeMediaSource& source)
{
    return adoptRef(*new WebAudioSourceProviderAVFObjC(source));
}

WebAudioSourceProviderAVFObjC::WebAudioSourceProviderAVFObjC(RealtimeMediaSource& source)
    : m_captureSource(&source)
{
}

WebAudioSourceProviderAVFObjC::~WebAudioSourceProviderAVFObjC()
{
    std::lock_guard<Lock> lock(m_mutex);

    if (m_converter) {
        // FIXME: make and use a smart pointer for AudioConverter
        AudioConverterDispose(m_converter);
        m_converter = nullptr;
    }
    if (m_connected)
        m_captureSource->removeObserver(*this);
}

void WebAudioSourceProviderAVFObjC::provideInput(AudioBus* bus, size_t framesToProcess)
{
    std::unique_lock<Lock> lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock() || !m_ringBuffer) {
        bus->zero();
        return;
    }

    uint64_t startFrame = 0;
    uint64_t endFrame = 0;
    m_ringBuffer->getCurrentFrameBounds(startFrame, endFrame);

    if (m_writeCount <= m_readCount) {
        bus->zero();
        return;
    }

    uint64_t framesAvailable = endFrame - m_readCount;
    if (framesAvailable < framesToProcess) {
        framesToProcess = static_cast<size_t>(framesAvailable);
        bus->zero();
    }

    ASSERT(bus->numberOfChannels() == m_ringBuffer->channelCount());
    if (bus->numberOfChannels() != m_ringBuffer->channelCount()) {
        bus->zero();
        return;
    }

    for (unsigned i = 0; i < m_list->mNumberBuffers; ++i) {
        AudioChannel& channel = *bus->channel(i);
        auto& buffer = m_list->mBuffers[i];
        buffer.mNumberChannels = 1;
        buffer.mData = channel.mutableData();
        buffer.mDataByteSize = channel.length() * sizeof(float);
    }

    m_ringBuffer->fetch(m_list.get(), framesToProcess, m_readCount);
    m_readCount += framesToProcess;

    if (m_converter)
        AudioConverterConvertComplexBuffer(m_converter, framesToProcess, m_list.get(), m_list.get());
}

void WebAudioSourceProviderAVFObjC::setClient(AudioSourceProviderClient* client)
{
    if (m_client == client)
        return;

    m_client = client;

    if (m_client && !m_connected) {
        m_connected = true;
        m_captureSource->addObserver(*this);
        m_captureSource->startProducingData();
    } else if (!m_client && m_connected) {
        m_captureSource->removeObserver(*this);
        m_connected = false;
    }
}

void WebAudioSourceProviderAVFObjC::prepare(const AudioStreamBasicDescription* format)
{
    std::lock_guard<Lock> lock(m_mutex);

    LOG(Media, "WebAudioSourceProviderAVFObjC::prepare(%p)", this);

    m_inputDescription = std::make_unique<AudioStreamBasicDescription>(*format);
    int numberOfChannels = format->mChannelsPerFrame;
    double sampleRate = format->mSampleRate;
    ASSERT(sampleRate >= 0);

    m_outputDescription = std::make_unique<AudioStreamBasicDescription>();
    m_outputDescription->mSampleRate = sampleRate;
    m_outputDescription->mFormatID = kAudioFormatLinearPCM;
    m_outputDescription->mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    m_outputDescription->mBitsPerChannel = 8 * sizeof(Float32);
    m_outputDescription->mChannelsPerFrame = numberOfChannels;
    m_outputDescription->mFramesPerPacket = 1;
    m_outputDescription->mBytesPerPacket = sizeof(Float32);
    m_outputDescription->mBytesPerFrame = sizeof(Float32);
    m_outputDescription->mFormatFlags |= kAudioFormatFlagIsNonInterleaved;

    if (m_converter) {
        // FIXME: make and use a smart pointer for AudioConverter
        AudioConverterDispose(m_converter);
        m_converter = nullptr;
    }

    if (*m_inputDescription != *m_outputDescription) {
        AudioConverterRef outConverter = nullptr;
        OSStatus err = AudioConverterNew(m_inputDescription.get(), m_outputDescription.get(), &outConverter);
        if (err) {
            LOG(Media, "WebAudioSourceProviderAVFObjC::prepare(%p) - AudioConverterNew returned error %i", this, err);
            return;
        }
        m_converter = outConverter;
    }

    // Make the ringbuffer large enough to store 1 second.
    uint64_t capacity = kRingBufferDuration * sampleRate;
    ASSERT(capacity <= SIZE_MAX);
    if (capacity > SIZE_MAX)
        return;

    // AudioBufferList is a variable-length struct, so create on the heap with a generic new() operator
    // with a custom size, and initialize the struct manually.
    uint64_t bufferListSize = offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * std::max(1, numberOfChannels));
    ASSERT(bufferListSize <= SIZE_MAX);
    if (bufferListSize > SIZE_MAX)
        return;

    m_ringBuffer = std::make_unique<CARingBuffer>();
    m_ringBuffer->allocate(CAAudioStreamDescription(*format), static_cast<size_t>(capacity));

    m_listBufferSize = static_cast<size_t>(bufferListSize);
    m_list = std::unique_ptr<AudioBufferList>(static_cast<AudioBufferList*>(::operator new (m_listBufferSize)));
    memset(m_list.get(), 0, m_listBufferSize);
    m_list->mNumberBuffers = numberOfChannels;

    RefPtr<WebAudioSourceProviderAVFObjC> protectedThis = this;
    callOnMainThread([protectedThis = WTFMove(protectedThis), numberOfChannels, sampleRate] {
        if (protectedThis->m_client)
            protectedThis->m_client->setFormat(numberOfChannels, sampleRate);
    });
}

void WebAudioSourceProviderAVFObjC::unprepare()
{
    std::lock_guard<Lock> lock(m_mutex);

    m_inputDescription = nullptr;
    m_outputDescription = nullptr;
    m_ringBuffer = nullptr;
    m_list = nullptr;
    m_listBufferSize = 0;
    if (m_converter) {
        // FIXME: make and use a smart pointer for AudioConverter
        AudioConverterDispose(m_converter);
        m_converter = nullptr;
    }
}

void WebAudioSourceProviderAVFObjC::audioSamplesAvailable(const MediaTime&, const PlatformAudioData& data, const AudioStreamDescription&, size_t frameCount)
{
    if (!m_ringBuffer || !is<WebAudioBufferList>(data))
        return;

    auto& bufferList = downcast<WebAudioBufferList>(data);

    m_ringBuffer->store(bufferList.list(), frameCount, m_writeCount);
    m_writeCount += frameCount;
}

}

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
