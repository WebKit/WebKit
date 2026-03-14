/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

#import "config.h"

#if ENABLE(WEB_AUDIO)
#import "AudioFileReaderCocoa.h"

#import "AudioBus.h"
#import "AudioFileReader.h"
#import "AudioSampleDataSource.h"
#import "AudioTrackPrivateWebM.h"
#import "CMUtilities.h"
#import "FloatConversion.h"
#import "InbandTextTrackPrivate.h"
#import "Logging.h"
#import "MIMESniffer.h"
#import "MediaSampleAVFObjC.h"
#import "SharedBuffer.h"
#import "VideoTrackPrivate.h"
#import "WebMAudioUtilitiesCocoa.h"
#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVAssetReader.h>
#import <AVFoundation/AVAssetReaderOutput.h>
#import <AVFoundation/AVAssetTrack.h>
#import <AudioToolbox/AudioConverter.h>
#import <CoreFoundation/CoreFoundation.h>
#import <SourceBufferParserWebM.h>
#import <limits>
#import <pal/cf/CoreAudioExtras.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/Function.h>
#import <wtf/NativePromise.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/Scope.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/Vector.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/darwin/DispatchExtras.h>

#import <pal/cf/AudioToolboxSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

// Delegate class for AVAssetResourceLoader to provide data from memory
@interface WebCoreAudioFileReaderLoaderDelegate : NSObject<AVAssetResourceLoaderDelegate> {
    std::span<const uint8_t> _data;
    String _mimeType;
}
- (instancetype)initWithData:(std::span<const uint8_t>)data mimeType:(const String&)mimeType;
- (void)close;
@end

@implementation WebCoreAudioFileReaderLoaderDelegate

- (instancetype)initWithData:(std::span<const uint8_t>)data mimeType:(const String&)mimeType
{
    if (!(self = [super init]))
        return nil;
    _data = data;
    _mimeType = mimeType;
    return self;
}

- (void)close
{
    _data = { };
}

- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest *)loadingRequest
{
    UNUSED_PARAM(resourceLoader);

    if (_data.empty())
        return NO;

    if (RetainPtr<AVAssetResourceLoadingContentInformationRequest> contentRequest = loadingRequest.contentInformationRequest) {
        contentRequest.get().contentType = _mimeType.createNSString().get();
        contentRequest.get().contentLength = _data.size();
        contentRequest.get().byteRangeAccessSupported = YES;
    }

    if (RetainPtr<AVAssetResourceLoadingDataRequest> dataRequest = loadingRequest.dataRequest) {
        NSInteger requestedOffset = dataRequest.get().requestedOffset;
        NSInteger requestedLength = dataRequest.get().requestedLength;

        if (requestedOffset < 0 || static_cast<size_t>(requestedOffset) >= _data.size()) {
            [loadingRequest finishLoadingWithError:[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorResourceUnavailable userInfo:nil]];
            return YES;
        }

        NSInteger availableLength = std::min<NSInteger>(requestedLength, _data.size() - requestedOffset);
        RetainPtr responseData = adoptNS([[NSData alloc] initWithBytes:_data.subspan(requestedOffset).data() length:availableLength]);
        [dataRequest respondWithData:responseData.get()];
    }

    [loadingRequest finishLoading];
    return YES;
}

@end

namespace WebCore {

[[nodiscard]] static AudioBufferList* tryCreateAudioBufferList(size_t numberOfBuffers)
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

static bool NODELETE validateAudioBufferList(AudioBufferList* bufferList)
{
    if (!bufferList)
        return false;

    std::optional<unsigned> expectedDataSize;
    for (auto& buffer : span(*bufferList)) {
        if (!buffer.mData)
            return false;

        unsigned dataSize = buffer.mDataByteSize;
        if (!expectedDataSize)
            expectedDataSize = dataSize;
        else if (*expectedDataSize != dataSize)
            return false;
    }
    return true;
}

static OSStatus readProc(void* clientData, SInt64 position, UInt32 requestCount, void* rawBuffer, UInt32* actualCount)
{
    auto* dataSpan = static_cast<std::span<const uint8_t>*>(clientData);
    auto buffer = unsafeMakeSpan(static_cast<uint8_t*>(rawBuffer), requestCount);

    size_t bytesToRead = 0;

    if (static_cast<UInt64>(position) < dataSpan->size()) {
        size_t bytesAvailable = dataSpan->size() - static_cast<size_t>(position);
        bytesToRead = requestCount <= bytesAvailable ? requestCount : bytesAvailable;
        memcpySpan(buffer, dataSpan->subspan(position).first(bytesToRead));
    }

    if (actualCount)
        *actualCount = bytesToRead;

    return noErr;
}

static SInt64 getSizeProc(void* clientData)
{
    return static_cast<std::span<const uint8_t>*>(clientData)->size();
}

static String mimeTypeFor(std::span<const uint8_t> data)
{
    AudioFileID audioFileID { nullptr };
    if (PAL::AudioFileOpenWithCallbacks(&data, readProc, 0, getSizeProc, 0, 0, &audioFileID) != noErr)
        return emptyString();

    auto cleanup = makeScopeExit([&] {
        PAL::AudioFileClose(audioFileID);
    });

    AudioFileTypeID typeID;
    UInt32 typeIDSize = sizeof(typeID);
    if (PAL::AudioFileGetProperty(audioFileID, kAudioFilePropertyFileFormat, &typeIDSize, &typeID) != noErr)
        return emptyString();

    switch (typeID) {
    // AudioFileStream
    case kAudioFileAMRType:
        return "audio/amr"_s;
    case kAudioFileMP1Type:
    case kAudioFileMP2Type:
    case kAudioFileMP3Type:
        return "audio/mpeg"_s;
    case kAudioFileLATMInLOASType:
        return "audio/aac"_s;
    case kAudioFileFLACType:
        return "audio/flac"_s;
    case kAudioFileAC3Type:
        return "audio/ac3"_s;
    case kAudioFileAAC_ADTSType:
        return "audio/aac"_s;
    // Audio Files
    case kAudioFileAIFFType:
    case kAudioFileAIFCType:
        return "audio/aiff"_s;
    case kAudioFileWAVEType:
    case kAudioFileRF64Type:
    case kAudioFileBW64Type:
    case kAudioFileWave64Type:
        return "audio/wav"_s;
    case kAudioFileCAFType:
        return "audio/x-caf"_s;
    case kAudioFileNextType:
    default:
        return emptyString();
    }
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
    AudioBufferList* NODELETE operator->() const { return m_bufferList; }
    operator AudioBufferList*() const { return m_bufferList; }
    AudioBufferList& NODELETE operator*() const { ASSERT(m_bufferList); return *m_bufferList; }
    bool NODELETE isValid() const { return validateAudioBufferList(m_bufferList); }
private:
    AudioBufferList* m_bufferList;
};

struct AudioFileReaderData {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(AudioFileReaderData);

public:
    const std::optional<MediaTime> trimStart;
    const std::optional<MediaTime> trimEnd;
    const Vector<Ref<MediaSampleAVFObjC>> samples;
    const size_t numberOfFrames { 0 };
};

AudioFileReader::AudioFileReader(std::span<const uint8_t> data)
    : m_data(data)
#if !RELEASE_LOG_DISABLED
    , m_logger(Logger::create(this))
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
{
#if ENABLE(MEDIA_SOURCE)
    if (isMaybeWebM(data)) {
        m_readerData = demuxWebMData(data);
        if (m_readerData)
            return;
    }
#endif
    m_readerData = demuxAVFData(data);
}

AudioFileReader::~AudioFileReader() = default;

static std::optional<size_t> framesInSamples(Vector<Ref<MediaSampleAVFObjC>>& samples)
{
    if (samples.isEmpty())
        return { };

    size_t numberOfFrames = 0;
    for (auto& sample : samples) {
        RetainPtr sampleBuffer = sample->sampleBuffer();
        if (!sampleBuffer)
            return { };
        RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(sampleBuffer.get());
        if (!formatDescription)
            continue;
        const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription.get());
        if (!asbd)
            return { };

        auto descriptions = getPacketDescriptions(sampleBuffer.get());
        if (descriptions.isEmpty()) {
            numberOfFrames += PAL::CMSampleBufferGetNumSamples(sampleBuffer.get()) * asbd->mFramesPerPacket;
            continue;
        }

        for (const auto& description : descriptions) {
            uint32_t fpp = description.mVariableFramesInPacket ? description.mVariableFramesInPacket : asbd->mFramesPerPacket;
            numberOfFrames += fpp;
        }
    }
    return numberOfFrames;
}

std::unique_ptr<AudioFileReaderData> AudioFileReader::demuxAVFData(std::span<const uint8_t> data) const
{
    // Determine MIME type from data signature
    String mimeType = MIMESniffer::getMIMETypeFromContent(data);
    if (mimeType.isEmpty())
        mimeType = mimeTypeFor(data);
    if (mimeType.isEmpty())
        return nullptr;

    // Create a custom URL scheme for in-memory data
    RetainPtr url = adoptNS([[NSURL alloc] initWithString:@"custom-audiofilereader://audio"]);

    // Create the resource loader delegate
    RetainPtr loaderDelegate = adoptNS([[WebCoreAudioFileReaderLoaderDelegate alloc] initWithData:data mimeType:mimeType]);

    auto scopeExit = makeScopeExit([loaderDelegate] {
        [loaderDelegate close];
    });

    // Create AVURLAsset with custom resource loader
    RetainPtr options = @{
        AVURLAssetReferenceRestrictionsKey: @(AVAssetReferenceRestrictionForbidAll),
        AVURLAssetUsesNoPersistentCacheKey: @YES,
        @"AVAssetRequiresInProcessOperationKey": @YES,
        AVURLAssetPreferPreciseDurationAndTimingKey: @YES,
        AVURLAssetOutOfBandMIMETypeKey: mimeType.createNSString().get()
    };
    RetainPtr asset = adoptNS([PAL::allocAVURLAssetInstance() initWithURL:url.get() options:options]);
    [[asset resourceLoader] setDelegate:loaderDelegate.get() queue:globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];

    // Get audio tracks synchronously (in-process loading enabled via AVAssetRequiresInProcessOperationKey)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr audioTracks = [asset tracksWithMediaType:AVMediaTypeAudio];
ALLOW_DEPRECATED_DECLARATIONS_END
    if (!audioTracks || ![audioTracks count]) {
        RELEASE_LOG_FAULT(WebAudio, "AudioFileReader: No audio tracks found");
        return nullptr;
    }

    RetainPtr audioTrack = [audioTracks objectAtIndex:0];

    // Create AVAssetReader
    NSError *error = nil;
    RetainPtr assetReader = adoptNS([PAL::allocAVAssetReaderInstance() initWithAsset:asset.get() error:&error]);
    if (!assetReader || error) {
        RELEASE_LOG_FAULT(WebAudio, "AudioFileReader: Failed to create AVAssetReader: %s", [[error localizedDescription] UTF8String]);
        return nullptr;
    }

    // Create track output - request compressed samples (no decompression settings)
    RetainPtr trackOutput = adoptNS([PAL::allocAVAssetReaderTrackOutputInstance() initWithTrack:audioTrack.get() outputSettings:nil]);
    if (!trackOutput) {
        RELEASE_LOG_FAULT(WebAudio, "AudioFileReader: Failed to create AVAssetReaderTrackOutput");
        return nullptr;
    }

    [trackOutput setAlwaysCopiesSampleData:NO];

    if (![assetReader canAddOutput:trackOutput.get()]) {
        RELEASE_LOG_FAULT(WebAudio, "AudioFileReader: Cannot add track output to asset reader");
        return nullptr;
    }

    [assetReader addOutput:trackOutput.get()];

    if (![assetReader startReading]) {
        RELEASE_LOG_FAULT(WebAudio, "AudioFileReader: Failed to start reading: %s", [[[assetReader error] localizedDescription] UTF8String]);
        return nullptr;
    }

    Vector<Ref<MediaSampleAVFObjC>> samples;
    size_t totalFrames = 0;
    MediaTime trimDurationAtStart = MediaTime::zeroTime();
    MediaTime trimDurationAtEnd = MediaTime::zeroTime();

    auto getTimeAttachment = [](CMSampleBufferRef sbuf, CFStringRef key) -> std::optional<MediaTime> {
        if (RetainPtr dict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMGetAttachment(sbuf, key, NULL)))
            return PAL::toMediaTime(PAL::CMTimeMakeFromDictionary(dict));

        return { };
    };

    // Read all sample buffers
    while (RetainPtr sampleBuffer = adoptCF([trackOutput copyNextSampleBuffer])) {
        if (auto trimStart = getTimeAttachment(sampleBuffer.get(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart))
            trimDurationAtStart += *trimStart;

        if (auto trimEnd = getTimeAttachment(sampleBuffer.get(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtEnd))
            trimDurationAtEnd += *trimEnd;

        if (!PAL::CMSampleBufferGetDataBuffer(sampleBuffer.get()))
            continue;

        CMItemCount numSamples = PAL::CMSampleBufferGetNumSamples(sampleBuffer.get());
        totalFrames += numSamples;
        samples.append(MediaSampleAVFObjC::create(sampleBuffer, 0));
    }

    if ([assetReader status] == AVAssetReaderStatusFailed) {
        RELEASE_LOG_FAULT(WebAudio, "AudioFileReader: Asset reader failed: %s", [[[assetReader error] localizedDescription] UTF8String]);
        return nullptr;
    }

    if (samples.isEmpty()) {
        RELEASE_LOG_FAULT(WebAudio, "AudioFileReader: No samples read from asset");
        return nullptr;
    }

    INFO_LOG(LOGIDENTIFIER, "Demuxed ", samples.size(), " sample buffers with ", totalFrames, " total frames");

    auto frames = framesInSamples(samples);
    if (!frames)
        return nullptr;

    std::optional<MediaTime> trimStart;
    if (trimDurationAtStart)
        trimStart = trimDurationAtStart;
    std::optional<MediaTime> trimEnd;
    if (trimDurationAtEnd)
        trimEnd = trimDurationAtEnd;

    return makeUnique<AudioFileReaderData>(AudioFileReaderData {
        .trimStart = WTF::move(trimStart),
        .trimEnd = WTF::move(trimEnd),
        .samples = WTF::move(samples),
        .numberOfFrames = *frames
    });
}

#if ENABLE(MEDIA_SOURCE)
bool AudioFileReader::isMaybeWebM(std::span<const uint8_t> data) const
{
    // From https://mimesniff.spec.whatwg.org/#signature-for-webm
    return data.size() >= 4 && data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3;
}

std::unique_ptr<AudioFileReaderData> AudioFileReader::demuxWebMData(std::span<const uint8_t> data) const
{
    auto parser = SourceBufferParserWebM::create();
    if (!parser)
        return nullptr;
    Ref buffer = SharedBuffer::create(data);

    std::optional<uint64_t> audioTrackId;
    RefPtr<AudioTrackPrivateWebM> track;
    Vector<Ref<MediaSampleAVFObjC>> samples;
    parser->setLogger(m_logger, m_logIdentifier);
    parser->setDidParseInitializationDataCallback([&](SourceBufferParserWebM::InitializationSegment&& init) {
        for (auto& audioTrack : init.audioTracks) {
            if (audioTrack.track) {
                audioTrackId = RefPtr { audioTrack.track }->id();
                // FIXME: Use downcast instead.
                track = unsafeRefPtrDowncast<AudioTrackPrivateWebM>(audioTrack.track);
                return;
            }
        }
    });
    parser->setDidProvideMediaDataCallback([&](Ref<MediaSampleAVFObjC>&& sample, uint64_t trackID, const String&) {
        if (!audioTrackId || trackID != *audioTrackId)
            return;
        samples.append(WTF::move(sample));
    });
    parser->setCallOnClientThreadCallback([](auto&& function) {
        function();
    });
    parser->setDidParseTrimmingDataCallback([&](uint64_t trackID, const MediaTime& discardPadding) {
        if (!audioTrackId || !track || trackID != *audioTrackId)
            return;
        track->setDiscardPadding(discardPadding);
    });
    auto result = parser->appendData(WTF::move(buffer));
    if (!track || !result)
        return nullptr;
    parser->flushPendingAudioSamples();

    auto frames = framesInSamples(samples);
    if (!frames)
        return nullptr;

    return makeUnique<AudioFileReaderData>(AudioFileReaderData {
        .trimStart = track->codecDelay(),
        .trimEnd = track->discardPadding(),
        .samples = WTF::move(samples),
        .numberOfFrames = *frames
    });
}
#endif

struct PassthroughUserData {
    const UInt32 m_channels;
    std::span<const uint8_t> m_data;
    const bool m_eos;
    const Vector<AudioStreamPacketDescription>& m_packets;
    UInt32 m_index;
    AudioStreamPacketDescription m_packet;
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
        return userData->m_eos ? noErr : kNoMoreDataErr;
    }

    if (userData->m_index >= userData->m_packets.size()) {
        *numDataPackets = 0;
        return kAudioConverterErr_RequiresPacketDescriptionsError;
    }

    if (packetDesc) {
        userData->m_packet = userData->m_packets[userData->m_index];
        userData->m_packet.mStartOffset = 0;
        *packetDesc = &userData->m_packet;
    }

    auto& firstBuffer = span(*data)[0];
    firstBuffer.mNumberChannels = userData->m_channels;
    firstBuffer.mDataByteSize = userData->m_packets[userData->m_index].mDataByteSize;

    firstBuffer.mData = const_cast<uint8_t*>(userData->m_data.subspan(userData->m_packets[userData->m_index].mStartOffset).data());

    // Sanity check
    if (std::to_address(span<uint8_t>(firstBuffer).end()) > std::to_address(userData->m_data.end())) {
        RELEASE_LOG_FAULT(WebAudio, "Nonsensical data structure, aborting");
        return kAudioConverterErr_UnspecifiedError;
    }

    *numDataPackets = 1;
    userData->m_index++;

    return noErr;
}

static UInt32 SMPTEEquivalentLayout(UInt32 layout)
{
    // https://webaudio.github.io/web-audio-api/#ChannelOrdering
    // WebAudio API requires SMPTE channel ordering. However webkit has been returning
    // incorrectly ordered channels for as long as WebAudio has been supported and sites
    // have adopted workaround it.
    // To avoid breaking existing behaviour, we leave all other layouts unchanged.
    // There are no system constants matching SMPTE, so we use the equivalent for a given channels count.
    switch (layout) {
    case kAudioChannelLayoutTag_Mono:
    case kAudioChannelLayoutTag_Stereo:
        return layout;
    case kAudioChannelLayoutTag_Ogg_3_0:
        return kAudioChannelLayoutTag_WAVE_3_0; // L R C
    case kAudioChannelLayoutTag_Ogg_4_0:
        return kAudioChannelLayoutTag_WAVE_4_0_B; // L R Rls Rrs
    case kAudioChannelLayoutTag_Ogg_5_0:
        return kAudioChannelLayoutTag_WAVE_5_0_B; // L R C LFE Rls Rrs
    case kAudioChannelLayoutTag_Ogg_5_1:
        return kAudioChannelLayoutTag_WAVE_5_1_B;
    case kAudioChannelLayoutTag_Ogg_6_1:
        return kAudioChannelLayoutTag_MPEG_6_1_A; // L R C LFE Ls Rs Cs
    case kAudioChannelLayoutTag_Ogg_7_1:
        return kAudioChannelLayoutTag_MPEG_7_1_C; // L R C LFE Ls Rs Rls Rrs
    default:
        return layout;
    }
}

std::optional<size_t> AudioFileReader::decodeData(AudioBufferList& bufferList, size_t numberOfFrames, const AudioStreamBasicDescription& inFormat, const AudioStreamBasicDescription& outFormat) const
{
    AudioConverterRef converter;
    if (PAL::AudioConverterNew(&inFormat, &outFormat, &converter) != noErr) {
        RELEASE_LOG_FAULT(WebAudio, "Unable to create decoder");
        return { };
    }
    auto cleanup = makeScopeExit([&] {
        PAL::AudioConverterDispose(converter);
    });
    ASSERT(m_readerData && !m_readerData->samples.isEmpty() && m_readerData->samples[0]->sampleBuffer(), "Structure integrity was checked in numberOfFrames");
    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(RetainPtr { m_readerData->samples[0]->sampleBuffer() }.get());
    if (!formatDescription) {
        RELEASE_LOG_FAULT(WebAudio, "Unable to retrieve format description from first sample");
        return { };
    }
    size_t magicCookieSize = 0;
    const void* magicCookie = PAL::CMAudioFormatDescriptionGetMagicCookie(formatDescription.get(), &magicCookieSize);
    if (magicCookie && magicCookieSize)
        PAL::AudioConverterSetProperty(converter, kAudioConverterDecompressionMagicCookie, magicCookieSize, magicCookie);

    auto setChannelLayoutIfNeeded = [&] {
        auto* formatListItem = PAL::CMAudioFormatDescriptionGetRichestDecodableFormat(formatDescription.get());
        if (!formatListItem)
            return;
        auto outputLayoutTag = SMPTEEquivalentLayout(formatListItem->mChannelLayoutTag);
        if (outputLayoutTag == formatListItem->mChannelLayoutTag)
            return;
        auto inputLayout = channelLayoutFromChannelLayoutTag(formatListItem->mChannelLayoutTag);
        auto outputLayout = channelLayoutFromChannelLayoutTag(outputLayoutTag);
        if (!inputLayout.first || !outputLayout.first)
            return;
        PAL::AudioConverterSetProperty(converter, kAudioConverterInputChannelLayout, inputLayout.second, inputLayout.first.get());
        PAL::AudioConverterSetProperty(converter, kAudioConverterOutputChannelLayout, outputLayout.second, outputLayout.first.get());
    };
    setChannelLayoutIfNeeded();

    AudioBufferListHolder decodedBufferList(inFormat.mChannelsPerFrame);
    if (!decodedBufferList) {
        RELEASE_LOG_FAULT(WebAudio, "Unable to create decoder");
        return { };
    }

    // Configure AudioConverter to trim initial frames.
    auto framesTrimmedAtStart = m_readerData->trimStart.value_or(MediaTime::zeroTime()).toTimeScale(inFormat.mSampleRate).timeValue();
    auto framesTrimmedAtEnd = m_readerData->trimEnd.value_or(MediaTime::zeroTime()).toTimeScale(inFormat.mSampleRate).timeValue();

    if (framesTrimmedAtStart < 0 || framesTrimmedAtStart > std::numeric_limits<int32_t>::max() || framesTrimmedAtEnd < 0 || framesTrimmedAtEnd > std::numeric_limits<int32_t>::max())
        return { };
    auto totalFramesTrimmed = WTF::checkedSum<uint32_t>(framesTrimmedAtStart, framesTrimmedAtEnd);
    if (totalFramesTrimmed.hasOverflowed())
        return { };
    if (m_readerData->numberOfFrames < totalFramesTrimmed.value())
        return { };
    size_t initialNumberOfFramesAfterTrim = m_readerData->numberOfFrames - totalFramesTrimmed.value();
    auto convertedNumberOfFramesAfterTrim = std::round<size_t>(initialNumberOfFramesAfterTrim * (outFormat.mSampleRate / inFormat.mSampleRate));

    AudioConverterPrimeInfo primeInfo = { static_cast<UInt32>(framesTrimmedAtStart), static_cast<UInt32>(framesTrimmedAtEnd) };
    PAL::AudioConverterSetProperty(converter, kAudioConverterPrimeInfo, sizeof(primeInfo), &primeInfo);

    size_t totalDecodedFrames = 0;
    OSStatus status;
    for (size_t i = 0; i < m_readerData->samples.size(); i++) {
        auto& sample = m_readerData->samples[i];
        RetainPtr sampleBuffer = sample->sampleBuffer();
        RetainPtr rawBuffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer.get());
        RetainPtr<CMBlockBufferRef> buffer = rawBuffer.get();
        if (!PAL::CMBlockBufferIsRangeContiguous(rawBuffer.get(), 0, 0)) {
            CMBlockBufferRef contiguousBuffer = nullptr;
            if (PAL::CMBlockBufferCreateContiguous(nullptr, rawBuffer.get(), nullptr, nullptr, 0, 0, 0, &contiguousBuffer) != kCMBlockBufferNoErr) {
                RELEASE_LOG_FAULT(WebAudio, "failed to create contiguous block buffer");
                return { };
            }
            buffer = adoptCF(contiguousBuffer);
        }

        auto srcData = PAL::CMBlockBufferGetDataSpan(buffer.get());
        if (!srcData.data()) {
            RELEASE_LOG_FAULT(WebAudio, "Unable to retrieve data");
            return { };
        }

        auto descriptions = getPacketDescriptions(sampleBuffer.get());
        if (descriptions.isEmpty()) {
            descriptions.append(AudioStreamPacketDescription {
                .mStartOffset = 0,
                .mVariableFramesInPacket = 0,
                .mDataByteSize = static_cast<UInt32>(srcData.size())
            });
        }

        PassthroughUserData userData = { inFormat.mChannelsPerFrame, srcData, i == m_readerData->samples.size() - 1, descriptions, 0, { } };

        do {
            if (numberOfFrames < totalDecodedFrames) {
                RELEASE_LOG_FAULT(WebAudio, "Decoded more frames than first calculated, no available space left");
                return { };
            }
            UInt32 numFrames = std::min<uint32_t>(std::numeric_limits<int32_t>::max() / sizeof(float), numberOfFrames - totalDecodedFrames);

            auto decodedBuffers = PAL::span(*decodedBufferList);
            auto bufferListBuffers = PAL::span(bufferList);
            for (UInt32 j = 0; j < inFormat.mChannelsPerFrame; ++j) {
                decodedBuffers[j].mNumberChannels = 1;
                decodedBuffers[j].mDataByteSize = numFrames * sizeof(float);
                decodedBuffers[j].mData = mutableSpan<float>(bufferListBuffers[j]).subspan(totalDecodedFrames).data();
            }

            status = PAL::AudioConverterFillComplexBuffer(converter, passthroughInputDataCallback, &userData, &numFrames, decodedBufferList, nullptr);
            if (status && status != kNoMoreDataErr) {
                RELEASE_LOG_FAULT(WebAudio, "Error decoding data");
                return { };
            }
            totalDecodedFrames += numFrames;
        } while (status != kNoMoreDataErr && status != noErr);
    }
    return std::min<size_t>(totalDecodedFrames, convertedNumberOfFramesAfterTrim);
}

// Helper struct for AVF passthrough callback
struct AVFPassthroughUserData {
    const UInt32 m_channels;
    std::span<const uint8_t> m_data;
    const bool m_eos;
    const Vector<AudioStreamPacketDescription>& m_packets;
    UInt32 m_index;
    AudioStreamPacketDescription m_packet;
};

std::optional<AudioStreamBasicDescription> AudioFileReader::fileDataFormat() const
{
    if (!m_readerData || m_readerData->samples.isEmpty())
        return { };

    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(RetainPtr { m_readerData->samples[0]->sampleBuffer() }.get());
    if (!formatDescription)
        return { };

    const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription.get());
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
    outFormat.mFormatFlags = static_cast<AudioFormatFlags>(kAudioFormatFlagsNativeFloatPacked) | static_cast<AudioFormatFlags>(kAudioFormatFlagIsNonInterleaved);
    outFormat.mBytesPerPacket = outFormat.mBytesPerFrame = bytesPerFloat;
    outFormat.mFramesPerPacket = 1;
    outFormat.mBitsPerChannel = bitsPerByte * bytesPerFloat;

    if (sampleRate)
        outFormat.mSampleRate = sampleRate;

    return outFormat;
}

RefPtr<AudioBus> AudioFileReader::createBus(float sampleRate, bool mixToMono)
{
    auto inFormat = fileDataFormat();
    if (!inFormat)
        return nullptr;

    // Block loading of the Audible Audio codec.
    if (inFormat->mFormatID == kAudioFormatAudible
        || inFormat->mFormatID == kCMAudioCodecType_AAC_AudibleProtected)
        return nullptr;

    AudioStreamBasicDescription outFormat = clientDataFormat(*inFormat, sampleRate);
    size_t numberOfChannels = inFormat->mChannelsPerFrame;
    double fileSampleRate = inFormat->mSampleRate;
    SInt64 numberOfFramesOut64 = m_readerData->numberOfFrames * (outFormat.mSampleRate / fileSampleRate);
    size_t numberOfFrames = static_cast<size_t>(numberOfFramesOut64);
    size_t busChannelCount = mixToMono ? 1 : numberOfChannels;

    // Create AudioBus where we'll put the PCM audio data
    auto audioBus = AudioBus::create(busChannelCount, numberOfFrames);
    audioBus->setSampleRate(narrowPrecisionToFloat(outFormat.mSampleRate));

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
    auto buffers = PAL::span(*bufferList);
    if (mixToMono && numberOfChannels == 2) {
        leftChannel.resize(numberOfFrames);
        rightChannel.resize(numberOfFrames);

        buffers[0].mNumberChannels = 1;
        buffers[0].mDataByteSize = bufferSize;
        buffers[0].mData = leftChannel.data();

        buffers[1].mNumberChannels = 1;
        buffers[1].mDataByteSize = bufferSize;
        buffers[1].mData = rightChannel.data();
    } else {
        RELEASE_ASSERT(!mixToMono || numberOfChannels == 1);

        for (size_t i = 0; i < numberOfChannels; ++i) {
            audioBus->channel(i)->zero();
            buffers[i].mNumberChannels = 1;
            buffers[i].mDataByteSize = bufferSize;
            buffers[i].mData = audioBus->channel(i)->mutableData();
            ASSERT(buffers[i].mData);
        }
    }

    if (!bufferList.isValid()) {
        RELEASE_LOG_FAULT(WebAudio, "Generated buffer in AudioFileReader::createBus() did not pass validation");
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (!m_readerData)
        return nullptr;

    auto decodedFrames = decodeData(*bufferList, numberOfFrames, *inFormat, outFormat);
    if (!decodedFrames)
        return nullptr;
    numberOfFrames = *decodedFrames;

    // The actual decoded number of frames may not match the number of frames calculated
    // while demuxing as frames can be trimmed. It will always be lower.
    audioBus->setLength(numberOfFrames);

    if (mixToMono && numberOfChannels == 2) {
        // Mix stereo down to mono
        auto destL = audioBus->channel(0)->mutableSpan();
        for (size_t i = 0; i < numberOfFrames; ++i)
            destL[i] = 0.5f * (leftChannel[i] + rightChannel[i]);
    }

    return audioBus;
}

RefPtr<AudioBus> createBusFromInMemoryAudioFile(std::span<const uint8_t> data, bool mixToMono, float sampleRate)
{
    AudioFileReader reader(data);
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
