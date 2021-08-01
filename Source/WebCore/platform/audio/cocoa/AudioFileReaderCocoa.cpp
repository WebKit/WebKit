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
#include "AudioSampleDataSource.h"
#include "AudioTrackPrivate.h"
#include "FloatConversion.h"
#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include "MediaSampleAVFObjC.h"
#include "VideoTrackPrivate.h"
#include "WebMAudioUtilitiesCocoa.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SourceBufferParserWebM.h>
#include <limits>
#include <wtf/CheckedArithmetic.h>
#include <wtf/FastMalloc.h>
#include <wtf/Function.h>
#include <wtf/RetainPtr.h>
#include <wtf/Scope.h>
#include <wtf/Vector.h>
#include <pal/cf/AudioToolboxSoftLink.h>
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

static WARN_UNUSED_RETURN AudioBufferList* tryCreateAudioBufferList(size_t numberOfBuffers)
{
    if (!numberOfBuffers)
        return nullptr;
    CheckedSize bufferListSize = sizeof(AudioBufferList) - sizeof(AudioBuffer);
    bufferListSize += WTF::checkedProduct<size_t>(numberOfBuffers, sizeof(AudioBuffer));
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

// On stack RAII class that will free the allocated AudioBufferList* as needed.
class AudioBufferListHolder {
public:
    explicit AudioBufferListHolder(size_t numberOfChannels)
        : m_bufferList(tryCreateAudioBufferList(numberOfChannels))
    {
    }

    ~AudioBufferListHolder()
    {
        if (m_bufferList)
            destroyAudioBufferList(m_bufferList);
    }

    explicit operator bool() const { return !!m_bufferList; }
    AudioBufferList* operator->() const { return m_bufferList; }
    operator AudioBufferList*() const { return m_bufferList; }
    AudioBufferList& operator*() const { ASSERT(m_bufferList); return *m_bufferList; }
    bool isValid() const { return validateAudioBufferList(m_bufferList); }
private:
    AudioBufferList* m_bufferList;
};

class AudioFileReaderWebMData {
    WTF_MAKE_FAST_ALLOCATED;

public:
#if ENABLE(MEDIA_SOURCE)
    SourceBufferParserWebM::InitializationSegment m_initSegment;
#endif
    MediaTime m_duration;
    Vector<Ref<MediaSampleAVFObjC>> m_samples;
};

AudioFileReader::AudioFileReader(const void* data, size_t dataSize)
    : m_data(data)
    , m_dataSize(dataSize)
#if !RELEASE_LOG_DISABLED
    , m_logger(Logger::create(this))
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
{
#if ENABLE(MEDIA_SOURCE)
    if (isMaybeWebM(static_cast<const uint8_t*>(data), dataSize)) {
        m_webmData = demuxWebMData(static_cast<const uint8_t*>(data), dataSize);
        if (m_webmData)
            return;
    }
#endif
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

#if ENABLE(MEDIA_SOURCE)
bool AudioFileReader::isMaybeWebM(const uint8_t* data, size_t dataSize) const
{
    // From https://mimesniff.spec.whatwg.org/#signature-for-webm
    return dataSize >= 4 && data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3;
}

std::unique_ptr<AudioFileReaderWebMData> AudioFileReader::demuxWebMData(const uint8_t* data, size_t dataSize) const
{
    auto parser = adoptRef(new SourceBufferParserWebM());
    bool error = false;
    std::optional<uint64_t> audioTrackId;
    MediaTime duration;
    SourceBufferParserWebM::InitializationSegment initSegment;
    Vector<Ref<MediaSampleAVFObjC>> samples;
    parser->setDidEncounterErrorDuringParsingCallback([&](uint64_t) {
        error = true;
    });
    parser->setDidParseInitializationDataCallback([&](SourceBufferParserWebM::InitializationSegment&& init) {
        for (auto& audioTrack : init.audioTracks) {
            if (audioTrack.track && audioTrack.track->trackUID()) {
                duration = init.duration;
                audioTrackId = audioTrack.track->trackUID();
                initSegment = WTFMove(init);
                return;
            }
        }
    });
    parser->setDidProvideMediaDataCallback([&](Ref<MediaSample>&& sample, uint64_t trackID, const String&) {
        if (!audioTrackId || trackID != *audioTrackId)
            return;
        samples.append(static_reference_cast<MediaSampleAVFObjC>(WTFMove(sample)));
    });
    parser->setCallOnClientThreadCallback([](auto&& function) {
        function();
    });
    SourceBufferParser::Segment segment({ data, dataSize });
    parser->appendData(WTFMove(segment));
    if (!audioTrackId)
        return nullptr;
    parser->flushPendingAudioBuffers();
    return makeUnique<AudioFileReaderWebMData>(AudioFileReaderWebMData { WTFMove(initSegment), WTFMove(duration), WTFMove(samples) });
}

struct PassthroughUserData {
    UInt32 m_channels;
    UInt32 m_dataSize;
    const void* m_data;
    UInt32 m_index;
    Vector<AudioStreamPacketDescription>& m_packets;
};

// Error value we pass through the decoder to signal that nothing
// has gone wrong during decoding and we're done processing the packet.
const uint32_t kNoMoreDataErr = 'MOAR';

static OSStatus passthroughInputDataCallback(AudioConverterRef, UInt32* numDataPackets, AudioBufferList* data, AudioStreamPacketDescription** packetDesc, void* inUserData)
{
    ASSERT(numDataPackets && data && inUserData);
    if (!numDataPackets || !data || !inUserData)
        return kAudioConverterErr_UnspecifiedError;

    auto* userData = static_cast<PassthroughUserData*>(inUserData);
    if (userData->m_index == userData->m_packets.size()) {
        *numDataPackets = 0;
        return kNoMoreDataErr;
    }

    if (packetDesc) {
        if (userData->m_index >= userData->m_packets.size()) {
            *numDataPackets = 0;
            return kAudioConverterErr_RequiresPacketDescriptionsError;
        }
        *packetDesc = &userData->m_packets[userData->m_index];
    }

    data->mBuffers[0].mNumberChannels = userData->m_channels;
    data->mBuffers[0].mDataByteSize = userData->m_dataSize;
    data->mBuffers[0].mData = const_cast<void*>(userData->m_data);

    *numDataPackets = 1;
    userData->m_index++;

    return noErr;
}

Vector<AudioStreamPacketDescription> AudioFileReader::getPacketDescriptions(CMSampleBufferRef sampleBuffer) const
{
    size_t packetDescriptionsSize;
    if (PAL::CMSampleBufferGetAudioStreamPacketDescriptions(sampleBuffer, 0, nullptr, &packetDescriptionsSize) != noErr) {
        RELEASE_LOG_FAULT(WebAudio, "Unable to get packet description list size");
        return { };
    }
    size_t numDescriptions = packetDescriptionsSize / sizeof(AudioStreamPacketDescription);
    if (!numDescriptions) {
        RELEASE_LOG_FAULT(WebAudio, "No packet description found.");
        return { };
    }
    Vector<AudioStreamPacketDescription> descriptions(numDescriptions);
    if (PAL::CMSampleBufferGetAudioStreamPacketDescriptions(sampleBuffer, packetDescriptionsSize, descriptions.data(), nullptr) != noErr) {
        RELEASE_LOG_FAULT(WebAudio, "Unable to get packet description list");
        return { };
    }
    auto numPackets = PAL::CMSampleBufferGetNumSamples(sampleBuffer);
    if (numDescriptions != size_t(numPackets)) {
        RELEASE_LOG_FAULT(WebAudio, "Unhandled CMSampleBuffer structure");
        return { };
    }
    return descriptions;
}

std::optional<size_t> AudioFileReader::decodeWebMData(AudioBufferList& bufferList, size_t numberOfFrames, const AudioStreamBasicDescription& inFormat, const AudioStreamBasicDescription& outFormat) const
{
    AudioConverterRef converter;
    if (PAL::AudioConverterNew(&inFormat, &outFormat, &converter) != noErr) {
        RELEASE_LOG_FAULT(WebAudio, "Unable to create decoder");
        return { };
    }
    auto cleanup = makeScopeExit([&] {
        PAL::AudioConverterDispose(converter);
    });
    ASSERT(m_webmData && !m_webmData->m_samples.isEmpty() && m_webmData->m_samples[0]->sampleBuffer(), "Structure integrity was checked in numberOfFrames");
    auto formatDescription = PAL::CMSampleBufferGetFormatDescription(m_webmData->m_samples[0]->sampleBuffer());
    if (!formatDescription) {
        RELEASE_LOG_FAULT(WebAudio, "Unable to retrieve format description from first sample");
        return { };
    }
    size_t magicCookieSize = 0;
    const void* magicCookie = PAL::CMAudioFormatDescriptionGetMagicCookie(formatDescription, &magicCookieSize);
    if (magicCookie && magicCookieSize)
        PAL::AudioConverterSetProperty(converter, kAudioConverterDecompressionMagicCookie, magicCookieSize, magicCookie);

    AudioBufferListHolder decodedBufferList(inFormat.mChannelsPerFrame);
    if (!decodedBufferList) {
        RELEASE_LOG_FAULT(WebAudio, "Unable to create decoder");
        return { };
    }

    size_t decodedFrames = 0;
    OSStatus status;
    for (auto& sample : m_webmData->m_samples) {
        CMSampleBufferRef sampleBuffer = sample->sampleBuffer();
        auto buffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer);
        ASSERT(PAL::CMBlockBufferIsRangeContiguous(buffer, 0, 0));
        if (!PAL::CMBlockBufferIsRangeContiguous(buffer, 0, 0)) {
            RELEASE_LOG_FAULT(WebAudio, "Unable to read sample content (not contiguous)");
            return { };
        }

        size_t srcSize = PAL::CMBlockBufferGetDataLength(buffer);
        char* srcData = nullptr;
        if (PAL::CMBlockBufferGetDataPointer(buffer, 0, nullptr, nullptr, &srcData) != noErr) {
            RELEASE_LOG_FAULT(WebAudio, "Unable to retrieve data");
            return { };
        }

        auto descriptions = getPacketDescriptions(sampleBuffer);
        if (descriptions.isEmpty())
            return { };

        PassthroughUserData userData = { inFormat.mChannelsPerFrame, UInt32(srcSize), srcData, 0, descriptions };

        do {
            if (numberOfFrames < decodedFrames) {
                RELEASE_LOG_FAULT(WebAudio, "Decoded more frames than first calculated");
                return { };
            }
            // in: the max number of packets we can handle from the decoder.
            // out: the number of packets the decoder is actually returning.
            UInt32 numFrames = std::min<uint32_t>(std::numeric_limits<int32_t>::max() / sizeof(float), numberOfFrames - decodedFrames);

            for (UInt32 i = 0; i < inFormat.mChannelsPerFrame; i++) {
                decodedBufferList->mBuffers[i].mNumberChannels = 1;
                decodedBufferList->mBuffers[i].mDataByteSize = numFrames * sizeof(float);
                decodedBufferList->mBuffers[i].mData = static_cast<float*>(bufferList.mBuffers[i].mData) + decodedFrames;
            }
            status = PAL::AudioConverterFillComplexBuffer(converter, passthroughInputDataCallback, &userData, &numFrames, decodedBufferList, nullptr);
            if (status && status != kNoMoreDataErr) {
                RELEASE_LOG_FAULT(WebAudio, "Error decoding data");
                return { };
            }
            decodedFrames += numFrames;
        } while (status != kNoMoreDataErr);
    }
    return decodedFrames;
}
#endif

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

ssize_t AudioFileReader::numberOfFrames() const
{
    SInt64 numberOfFramesIn64 = 0;

    if (!m_webmData) {
        if (!m_extAudioFileRef)
            return -1;

        UInt32 size = sizeof(numberOfFramesIn64);
        if (PAL::ExtAudioFileGetProperty(m_extAudioFileRef, kExtAudioFileProperty_FileLengthFrames, &size, &numberOfFramesIn64) != noErr || numberOfFramesIn64 <= 0) {
            RELEASE_LOG_FAULT(WebAudio, "Unable to retrieve number of frames in content (unsupported?");
            return -1;
        }

        return numberOfFramesIn64;
    }
#if ENABLE(MEDIA_SOURCE)
    if (m_webmData->m_samples.isEmpty()) {
        RELEASE_LOG_FAULT(WebAudio, "No sample demuxed from webm container");
        return -1;
    }

    // Calculate the total number of decoded samples that were demuxed.
    // This code only handle the CMSampleBuffer as generated by the SourceBufferParserWebM
    // where a AudioStreamPacketDescriptions array is always provided even with
    // Constant bitrate and constant frames-per-packet audio.
    for (auto& sample : m_webmData->m_samples) {
        auto sampleBuffer = sample->sampleBuffer();
        if (!sampleBuffer) {
            RELEASE_LOG_FAULT(WebAudio, "Impossible memory corruption encountered");
            return -1;
        }
        const auto formatDescription = PAL::CMSampleBufferGetFormatDescription(sampleBuffer);
        if (!formatDescription) {
            RELEASE_LOG_FAULT(WebAudio, "Unable to retrieve format descriptiong from sample");
            return -1;
        }
        const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription);
        if (!asbd) {
            RELEASE_LOG_FAULT(WebAudio, "Unable to retrieve asbd from format description");
            return -1;
        }

        auto descriptions = getPacketDescriptions(sampleBuffer);
        if (descriptions.isEmpty())
            return -1;

        for (const auto& description : descriptions) {
            uint32_t fpp = description.mVariableFramesInPacket ? description.mVariableFramesInPacket : asbd->mFramesPerPacket;
            numberOfFramesIn64 += fpp;
        }
    }
    return numberOfFramesIn64;
#else
    return 0;
#endif
}

std::optional<AudioStreamBasicDescription> AudioFileReader::fileDataFormat() const
{
    if (!m_webmData) {
        AudioStreamBasicDescription format;
        UInt32 size = sizeof(format);
        if (PAL::ExtAudioFileGetProperty(m_extAudioFileRef, kExtAudioFileProperty_FileDataFormat, &size, &format) != noErr)
            return { };
        return format;
    }

    if (m_webmData->m_samples.isEmpty())
        return { };

    CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(m_webmData->m_samples[0]->sampleBuffer());
    if (!formatDescription)
        return { };

    const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription);
    return *asbd;
}

AudioStreamBasicDescription AudioFileReader::clientDataFormat(const AudioStreamBasicDescription& inFormat, float sampleRate) const
{
    // Make client format same number of channels as file format, but tweak a few things.
    // Client format will be linear PCM (canonical), and potentially change sample-rate.
    AudioStreamBasicDescription outFormat = inFormat;

    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    outFormat.mFormatID = kAudioFormatLinearPCM;
    outFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    outFormat.mBytesPerPacket = outFormat.mBytesPerFrame = bytesPerFloat;
    outFormat.mFramesPerPacket = 1;
    outFormat.mBitsPerChannel = bitsPerByte * bytesPerFloat;

    if (sampleRate)
        outFormat.mSampleRate = sampleRate;

    return outFormat;
}

RefPtr<AudioBus> AudioFileReader::createBus(float sampleRate, bool mixToMono)
{
    SInt64 numberOfFramesIn64 = numberOfFrames();
    if (numberOfFramesIn64 <= 0)
        return nullptr;

    auto inFormat = fileDataFormat();
    if (!inFormat)
        return nullptr;

    AudioStreamBasicDescription outFormat = clientDataFormat(*inFormat, sampleRate);
    size_t numberOfChannels = inFormat->mChannelsPerFrame;
    double fileSampleRate = inFormat->mSampleRate;
    SInt64 numberOfFramesOut64 = numberOfFramesIn64 * (outFormat.mSampleRate / fileSampleRate);
    size_t numberOfFrames = static_cast<size_t>(numberOfFramesOut64);
    size_t busChannelCount = mixToMono ? 1 : numberOfChannels;

    // Create AudioBus where we'll put the PCM audio data
    auto audioBus = AudioBus::create(busChannelCount, numberOfFrames);
    audioBus->setSampleRate(narrowPrecisionToFloat(outFormat.mSampleRate)); // save for later

    AudioBufferListHolder bufferList(numberOfChannels);
    if (!bufferList) {
        RELEASE_LOG_FAULT(WebAudio, "tryCreateAudioBufferList(%ld) returned null", numberOfChannels);
        return nullptr;
    }
    const size_t bufferSize = numberOfFrames * sizeof(float);

    // Only allocated in the mixToMono case; deallocated on destruction.
    AudioFloatArray leftChannel;
    AudioFloatArray rightChannel;

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

    if (!bufferList.isValid()) {
        RELEASE_LOG_FAULT(WebAudio, "Generated buffer in AudioFileReader::createBus() did not pass validation");
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (m_webmData) {
#if ENABLE(MEDIA_SOURCE)
        auto decodedFrames = decodeWebMData(*bufferList, numberOfFrames, *inFormat, outFormat);
        if (!decodedFrames)
            return nullptr;
        // The actual decoded number of frames may not match the number of frames calculated
        // while demuxing as frames can be trimmed. It will always be lower.
        audioBus->setLength(*decodedFrames);
        numberOfFrames = *decodedFrames;
#endif
    } else {
        if (PAL::ExtAudioFileSetProperty(m_extAudioFileRef, kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription), &outFormat) != noErr)
            return nullptr;

        // Read from the file (or in-memory version)
        UInt32 framesToRead = numberOfFrames;
        if (PAL::ExtAudioFileRead(m_extAudioFileRef, &framesToRead, bufferList) != noErr)
            return nullptr;
    }

    if (mixToMono && numberOfChannels == 2) {
        // Mix stereo down to mono
        float* destL = audioBus->channel(0)->mutableData();
        for (size_t i = 0; i < numberOfFrames; ++i)
            destL[i] = 0.5f * (leftChannel[i] + rightChannel[i]);
    }

    return audioBus;
}

RefPtr<AudioBus> createBusFromInMemoryAudioFile(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    AudioFileReader reader(data, dataSize);
    return reader.createBus(sampleRate, mixToMono);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& AudioFileReader::logChannel() const
{
    return LogMedia;
}
#endif

} // WebCore

#endif // ENABLE(WEB_AUDIO)
