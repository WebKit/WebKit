/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include "AudioFileReaderMac.h"

#include "AudioBus.h"
#include "AudioFileReader.h"
#include "AudioFileReaderCocoa.h"
#include "FloatConversion.h"
#include "Logging.h"
#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

AudioFileReader::AudioFileReader(const char* filePath)
    : m_data(nullptr)
    , m_dataSize(0)
    , m_audioFileID(0)
    , m_extAudioFileRef(0)
{
    RetainPtr<CFStringRef> filePathString = adoptCF(CFStringCreateWithCString(kCFAllocatorDefault, filePath, kCFStringEncodingUTF8));
    RetainPtr<CFURLRef> url = adoptCF(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, filePathString.get(), kCFURLPOSIXPathStyle, false));
    if (!url)
        return;

    ExtAudioFileOpenURL(url.get(), &m_extAudioFileRef);
}

AudioFileReader::AudioFileReader(const void* data, size_t dataSize)
    : m_data(data)
    , m_dataSize(dataSize)
    , m_audioFileID(0)
    , m_extAudioFileRef(0)
{
    OSStatus result = AudioFileOpenWithCallbacks(this, readProc, 0, getSizeProc, 0, 0, &m_audioFileID);

    if (result != noErr)
        return;

    result = ExtAudioFileWrapAudioFileID(m_audioFileID, false, &m_extAudioFileRef);
    if (result != noErr)
        m_extAudioFileRef = 0;
}

AudioFileReader::~AudioFileReader()
{
    if (m_extAudioFileRef)
        ExtAudioFileDispose(m_extAudioFileRef);

    m_extAudioFileRef = 0;

    if (m_audioFileID)
        AudioFileClose(m_audioFileID);
        
    m_audioFileID = 0;
}

OSStatus AudioFileReader::readProc(void* clientData, SInt64 position, UInt32 requestCount, void* buffer, UInt32* actualCount)
{
    AudioFileReader* audioFileReader = static_cast<AudioFileReader*>(clientData);

    size_t dataSize = audioFileReader->dataSize();
    const void* data = audioFileReader->data();
    size_t bytesToRead = 0;

    if (static_cast<UInt64>(position) < dataSize) {
        size_t bytesAvailable = dataSize - static_cast<size_t>(position);
        bytesToRead = requestCount <= bytesAvailable ? requestCount : bytesAvailable;
        memcpy(buffer, static_cast<const char*>(data) + position, bytesToRead);
    } else
        bytesToRead = 0;

    if (actualCount)
        *actualCount = bytesToRead;

    return noErr;
}

SInt64 AudioFileReader::getSizeProc(void* clientData)
{
    AudioFileReader* audioFileReader = static_cast<AudioFileReader*>(clientData);
    return audioFileReader->dataSize();
}

RefPtr<AudioBus> AudioFileReader::createBus(float sampleRate, bool mixToMono)
{
    if (!m_extAudioFileRef)
        return nullptr;

    // Get file's data format
    UInt32 size = sizeof(m_fileDataFormat);
    OSStatus result = ExtAudioFileGetProperty(m_extAudioFileRef, kExtAudioFileProperty_FileDataFormat, &size, &m_fileDataFormat);
    if (result != noErr)
        return nullptr;

    // Number of channels
    size_t numberOfChannels = m_fileDataFormat.mChannelsPerFrame;

    // Number of frames
    SInt64 numberOfFrames64 = 0;
    size = sizeof(numberOfFrames64);
    result = ExtAudioFileGetProperty(m_extAudioFileRef, kExtAudioFileProperty_FileLengthFrames, &size, &numberOfFrames64);
    if (result != noErr || numberOfFrames64 <= 0)
        return nullptr;

    // Sample-rate
    double fileSampleRate = m_fileDataFormat.mSampleRate;

    // Make client format same number of channels as file format, but tweak a few things.
    // Client format will be linear PCM (canonical), and potentially change sample-rate.
    m_clientDataFormat = m_fileDataFormat;

    m_clientDataFormat.mFormatID = kAudioFormatLinearPCM;
    m_clientDataFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    m_clientDataFormat.mBitsPerChannel = 8 * sizeof(Float32);
    m_clientDataFormat.mChannelsPerFrame = numberOfChannels;
    m_clientDataFormat.mFramesPerPacket = 1;
    m_clientDataFormat.mBytesPerPacket = sizeof(Float32);
    m_clientDataFormat.mBytesPerFrame = sizeof(Float32);
    m_clientDataFormat.mFormatFlags |= kAudioFormatFlagIsNonInterleaved;

    if (sampleRate)
        m_clientDataFormat.mSampleRate = sampleRate;

    result = ExtAudioFileSetProperty(m_extAudioFileRef, kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription), &m_clientDataFormat);
    if (result != noErr)
        return nullptr;

    // Change numberOfFrames64 to destination sample-rate
    numberOfFrames64 = numberOfFrames64 * (m_clientDataFormat.mSampleRate / fileSampleRate);
    size_t numberOfFrames = static_cast<size_t>(numberOfFrames64);

    size_t busChannelCount = mixToMono ? 1 : numberOfChannels;

    // Create AudioBus where we'll put the PCM audio data
    auto audioBus = AudioBus::create(busChannelCount, numberOfFrames);
    audioBus->setSampleRate(narrowPrecisionToFloat(m_clientDataFormat.mSampleRate)); // save for later

    // Only allocated in the mixToMono case
    AudioFloatArray bufL;
    AudioFloatArray bufR;
    float* bufferL = 0;
    float* bufferR = 0;
    
    // Setup AudioBufferList in preparation for reading
    AudioBufferList* bufferList = tryCreateAudioBufferList(numberOfChannels);
    if (!bufferList) {
        RELEASE_LOG_FAULT(WebAudio, "tryCreateAudioBufferList(%ld) returned null", numberOfChannels);
        return nullptr;
    }

    RELEASE_ASSERT(bufferList->mNumberBuffers == numberOfChannels);
    if (mixToMono && numberOfChannels == 2) {
        bufL.resize(numberOfFrames);
        bufR.resize(numberOfFrames);
        bufferL = bufL.data();
        bufferR = bufR.data();

        bufferList->mBuffers[0].mNumberChannels = 1;
        bufferList->mBuffers[0].mDataByteSize = numberOfFrames * sizeof(float);
        bufferList->mBuffers[0].mData = bufferL;

        bufferList->mBuffers[1].mNumberChannels = 1;
        bufferList->mBuffers[1].mDataByteSize = numberOfFrames * sizeof(float);
        bufferList->mBuffers[1].mData = bufferR;
    } else {
        RELEASE_ASSERT(!mixToMono || numberOfChannels == 1);

        // for True-stereo (numberOfChannels == 4)
        for (size_t i = 0; i < numberOfChannels; ++i) {
            bufferList->mBuffers[i].mNumberChannels = 1;
            bufferList->mBuffers[i].mDataByteSize = numberOfFrames * sizeof(float);
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
    result = ExtAudioFileRead(m_extAudioFileRef, &framesToRead, bufferList);
    if (result != noErr) {
        destroyAudioBufferList(bufferList);
        return nullptr;
    }

    if (mixToMono && numberOfChannels == 2) {
        // Mix stereo down to mono
        float* destL = audioBus->channel(0)->mutableData();
        for (size_t i = 0; i < numberOfFrames; i++)
            destL[i] = 0.5f * (bufferL[i] + bufferR[i]);
    }

    // Cleanup
    destroyAudioBufferList(bufferList);

    return audioBus;
}

RefPtr<AudioBus> createBusFromAudioFile(const char* filePath, bool mixToMono, float sampleRate)
{
    return AudioFileReader(filePath).createBus(sampleRate, mixToMono);
}

RefPtr<AudioBus> createBusFromInMemoryAudioFile(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    return AudioFileReader(data, dataSize).createBus(sampleRate, mixToMono);
}

} // namespace WebCore

#endif // PLATFORM(MAC)

#endif // ENABLE(WEB_AUDIO)
