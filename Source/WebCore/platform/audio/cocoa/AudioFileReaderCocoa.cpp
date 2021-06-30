/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)
#include "AudioFileReaderCocoa.h"

#include "AudioBus.h"
#include "AudioFileReader.h"
#include "FloatConversion.h"
#include "Logging.h"
#include <AudioToolbox/ExtendedAudioFile.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pal/cf/AudioToolboxSoftLink.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/FastMalloc.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

static WARN_UNUSED_RETURN AudioBufferList* tryCreateAudioBufferList(size_t numberOfBuffers)
{
    CheckedSize bufferListSize = sizeof(AudioBufferList) - sizeof(AudioBuffer);
    bufferListSize += numberOfBuffers * sizeof(AudioBuffer);
    if (bufferListSize.hasOverflowed())
        return nullptr;

    auto allocated = tryFastCalloc(1, bufferListSize);
    AudioBufferList* bufferList;
    if (!allocated.getValue(bufferList))
        return nullptr;

    bufferList->mNumberBuffers = numberOfBuffers;
    return bufferList;
}

static inline void destroyAudioBufferList(AudioBufferList* bufferList)
{
    fastFree(bufferList);
}

static bool validateAudioBufferList(AudioBufferList* bufferList)
{
    if (!bufferList)
        return false;

    std::optional<unsigned> expectedDataSize;
    const AudioBuffer* buffer = bufferList->mBuffers;
    const AudioBuffer* bufferEnd = buffer + bufferList->mNumberBuffers;
    for ( ; buffer < bufferEnd; ++buffer) {
        if (!buffer->mData)
            return false;

        unsigned dataSize = buffer->mDataByteSize;
        if (!expectedDataSize)
            expectedDataSize = dataSize;
        else if (*expectedDataSize != dataSize)
            return false;
    }
    return true;
}

AudioFileReader::AudioFileReader(const void* data, size_t dataSize)
    : m_data(data)
    , m_dataSize(dataSize)
{
    if (PAL::AudioFileOpenWithCallbacks(this, readProc, 0, getSizeProc, 0, 0, &m_audioFileID) != noErr)
        return;

    if (PAL::ExtAudioFileWrapAudioFileID(m_audioFileID, false, &m_extAudioFileRef) != noErr)
        m_extAudioFileRef = 0;
}

AudioFileReader::~AudioFileReader()
{
    if (m_extAudioFileRef)
        PAL::ExtAudioFileDispose(m_extAudioFileRef);

    m_extAudioFileRef = 0;

    if (m_audioFileID)
        PAL::AudioFileClose(m_audioFileID);

    m_audioFileID = 0;
}

OSStatus AudioFileReader::readProc(void* clientData, SInt64 position, UInt32 requestCount, void* buffer, UInt32* actualCount)
{
    auto* audioFileReader = static_cast<AudioFileReader*>(clientData);

    auto dataSize = audioFileReader->dataSize();
    auto* data = audioFileReader->data();
    size_t bytesToRead = 0;

    if (static_cast<UInt64>(position) < dataSize) {
        size_t bytesAvailable = dataSize - static_cast<size_t>(position);
        bytesToRead = requestCount <= bytesAvailable ? requestCount : bytesAvailable;
        memcpy(buffer, static_cast<const uint8_t*>(data) + position, bytesToRead);
    }

    if (actualCount)
        *actualCount = bytesToRead;

    return noErr;
}

SInt64 AudioFileReader::getSizeProc(void* clientData)
{
    return static_cast<AudioFileReader*>(clientData)->dataSize();
}

RefPtr<AudioBus> AudioFileReader::createBus(float sampleRate, bool mixToMono)
{
    if (!m_extAudioFileRef)
        return nullptr;

    // Get file's data format
    UInt32 size = sizeof(m_fileDataFormat);
    if (PAL::ExtAudioFileGetProperty(m_extAudioFileRef, kExtAudioFileProperty_FileDataFormat, &size, &m_fileDataFormat) != noErr)
        return nullptr;

    size_t numberOfChannels = m_fileDataFormat.mChannelsPerFrame;

    // Number of frames
    SInt64 numberOfFrames64 = 0;
    size = sizeof(numberOfFrames64);
    if (PAL::ExtAudioFileGetProperty(m_extAudioFileRef, kExtAudioFileProperty_FileLengthFrames, &size, &numberOfFrames64) != noErr || numberOfFrames64 <= 0)
        return nullptr;

    double fileSampleRate = m_fileDataFormat.mSampleRate;

    // Make client format same number of channels as file format, but tweak a few things.
    // Client format will be linear PCM (canonical), and potentially change sample-rate.
    m_clientDataFormat = m_fileDataFormat;

    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    m_clientDataFormat.mFormatID = kAudioFormatLinearPCM;
    m_clientDataFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    m_clientDataFormat.mBytesPerPacket = bytesPerFloat;
    m_clientDataFormat.mFramesPerPacket = 1;
    m_clientDataFormat.mBytesPerFrame = bytesPerFloat;
    m_clientDataFormat.mChannelsPerFrame = numberOfChannels;
    m_clientDataFormat.mBitsPerChannel = bitsPerByte * bytesPerFloat;

    if (sampleRate)
        m_clientDataFormat.mSampleRate = sampleRate;

    if (PAL::ExtAudioFileSetProperty(m_extAudioFileRef, kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription), &m_clientDataFormat) != noErr)
        return nullptr;

    // Change numberOfFrames64 to destination sample-rate
    numberOfFrames64 = numberOfFrames64 * (m_clientDataFormat.mSampleRate / fileSampleRate);
    size_t numberOfFrames = static_cast<size_t>(numberOfFrames64);

    size_t busChannelCount = mixToMono ? 1 : numberOfChannels;

    // Create AudioBus where we'll put the PCM audio data
    auto audioBus = AudioBus::create(busChannelCount, numberOfFrames);
    audioBus->setSampleRate(narrowPrecisionToFloat(m_clientDataFormat.mSampleRate)); // save for later

    // Only allocated in the mixToMono case; deallocated on destruction.
    AudioFloatArray leftChannel;
    AudioFloatArray rightChannel;

    AudioBufferList* bufferList = tryCreateAudioBufferList(numberOfChannels);
    if (!bufferList) {
        RELEASE_LOG_FAULT(WebAudio, "tryCreateAudioBufferList(%ld) returned null", numberOfChannels);
        return nullptr;
    }
    const size_t bufferSize = numberOfFrames * sizeof(float);

    RELEASE_ASSERT(bufferList->mNumberBuffers == numberOfChannels);
    if (mixToMono && numberOfChannels == 2) {
        leftChannel.resize(numberOfFrames);
        rightChannel.resize(numberOfFrames);

        bufferList->mBuffers[0].mNumberChannels = 1;
        bufferList->mBuffers[0].mDataByteSize = bufferSize;
        bufferList->mBuffers[0].mData = leftChannel.data();

        bufferList->mBuffers[1].mNumberChannels = 1;
        bufferList->mBuffers[1].mDataByteSize = bufferSize;
        bufferList->mBuffers[1].mData = rightChannel.data();
    } else {
        RELEASE_ASSERT(!mixToMono || numberOfChannels == 1);

        // For True-stereo (numberOfChannels == 4)
        for (size_t i = 0; i < numberOfChannels; ++i) {
            audioBus->channel(i)->zero();
            bufferList->mBuffers[i].mNumberChannels = 1;
            bufferList->mBuffers[i].mDataByteSize = bufferSize;
            bufferList->mBuffers[i].mData = audioBus->channel(i)->mutableData();
            ASSERT(bufferList->mBuffers[i].mData);
        }
    }

    if (!validateAudioBufferList(bufferList)) {
        RELEASE_LOG_FAULT(WebAudio, "Generated buffer in AudioFileReader::createBus() did not pass validation");
        ASSERT_NOT_REACHED();
        destroyAudioBufferList(bufferList);
        return nullptr;
    }

    // Read from the file (or in-memory version)
    UInt32 framesToRead = numberOfFrames;
    if (PAL::ExtAudioFileRead(m_extAudioFileRef, &framesToRead, bufferList) != noErr) {
        destroyAudioBufferList(bufferList);
        return nullptr;
    }

    if (mixToMono && numberOfChannels == 2) {
        // Mix stereo down to mono
        float* destL = audioBus->channel(0)->mutableData();
        for (size_t i = 0; i < numberOfFrames; ++i)
            destL[i] = 0.5f * (leftChannel[i] + rightChannel[i]);
    }

    // Cleanup
    destroyAudioBufferList(bufferList);

    return audioBus;
}

RefPtr<AudioBus> createBusFromInMemoryAudioFile(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    AudioFileReader reader(data, dataSize);
    return reader.createBus(sampleRate, mixToMono);
}

} // WebCore

#endif // ENABLE(WEB_AUDIO)
