/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "SourceBufferParserAVFObjC.h"

#if ENABLE(MEDIA_SOURCE)

#import "AVAssetMIMETypeCache.h"
#import "AVAssetTrackUtilities.h"
#import "AVStreamDataParserMIMETypeCache.h"
#import "AudioTrackPrivateMediaSourceAVFObjC.h"
#import "FourCC.h"
#import "InbandTextTrackPrivateAVFObjC.h"
#import "Logging.h"
#import "MediaDescription.h"
#import "MediaSample.h"
#import "MediaSampleAVFObjC.h"
#import "NotImplemented.h"
#import "SharedBuffer.h"
#import "TimeRanges.h"
#import "VideoTrackPrivateMediaSourceAVFObjC.h"
#import <AVFoundation/AVAssetTrack.h>
#import <JavaScriptCore/TypedArrayInlines.h>
#import <objc/runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/BlockObjCExceptions.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

#pragma mark -
#pragma mark WebAVStreamDataParserListener

@interface WebAVStreamDataParserListener : NSObject<AVStreamDataParserOutputHandling> {
    WebCore::SourceBufferParserAVFObjC* _parent;
    AVStreamDataParser* _parser;
}
@property (assign) WebCore::SourceBufferParserAVFObjC* parent;
- (id)initWithParser:(AVStreamDataParser*)parser parent:(WebCore::SourceBufferParserAVFObjC*)parent;
@end

@implementation WebAVStreamDataParserListener
- (id)initWithParser:(AVStreamDataParser*)parser parent:(WebCore::SourceBufferParserAVFObjC*)parent
{
    self = [super init];
    if (!self)
        return nil;

    ASSERT(parent);
    _parent = parent;
    _parser = parser;
    [_parser setDelegate:self];
    return self;
}

@synthesize parent=_parent;

- (void)dealloc
{
    [_parser setDelegate:nil];
    [super dealloc];
}

- (void)invalidate
{
    [_parser setDelegate:nil];
    _parser = nullptr;
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didParseStreamDataAsAsset:(AVAsset *)asset
{
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser);
    _parent->didParseStreamDataAsAsset(asset);
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didParseStreamDataAsAsset:(AVAsset *)asset withDiscontinuity:(BOOL)discontinuity
{
    UNUSED_PARAM(discontinuity);
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser);
    _parent->didParseStreamDataAsAsset(asset);
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didFailToParseStreamDataWithError:(NSError *)error
{
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser);
    _parent->didFailToParseStreamDataWithError(error);
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didProvideMediaData:(CMSampleBufferRef)sample forTrackID:(CMPersistentTrackID)trackID mediaType:(NSString *)nsMediaType flags:(AVStreamDataParserOutputMediaDataFlags)flags
{
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser);
    _parent->didProvideMediaDataForTrackID(trackID, sample, nsMediaType, flags);
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)streamDataParserWillProvideContentKeyRequestInitializationData:(AVStreamDataParser *)streamDataParser forTrackID:(CMPersistentTrackID)trackID
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser);
    _parent->willProvideContentKeyRequestInitializationDataForTrackID(trackID);
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didProvideContentKeyRequestInitializationData:(NSData *)initData forTrackID:(CMPersistentTrackID)trackID
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser);
    _parent->didProvideContentKeyRequestInitializationDataForTrackID(initData, trackID);
}
@end

namespace WebCore {

#pragma mark -
#pragma mark MediaDescriptionAVFObjC

class MediaDescriptionAVFObjC final : public MediaDescription {
public:
    static Ref<MediaDescriptionAVFObjC> create(AVAssetTrack* track) { return adoptRef(*new MediaDescriptionAVFObjC(track)); }
    virtual ~MediaDescriptionAVFObjC() { }

    bool isVideo() const override { return m_isVideo; }
    bool isAudio() const override { return m_isAudio; }
    bool isText() const override { return m_isText; }
    AtomString codec() const override
    {
        // AtomStrings can only be destroyed from the same thread they're
        // created in. Since this structure is created in a parser thread
        // only create the AtomString the first time this function is accessed
        // which is presumably on the main thread.
        ASSERT(isMainThread());
        if (!m_codec)
            m_codec = AtomString(reinterpret_cast<const LChar*>(&m_originalCodec), 4);
        return *m_codec;
    }

private:
    MediaDescriptionAVFObjC(AVAssetTrack* track)
        : m_isVideo([track hasMediaCharacteristic:AVMediaCharacteristicVisual])
        , m_isAudio([track hasMediaCharacteristic:AVMediaCharacteristicAudible])
        , m_isText([track hasMediaCharacteristic:AVMediaCharacteristicLegible])
    {
        NSArray* formatDescriptions = [track formatDescriptions];
        CMFormatDescriptionRef description = [formatDescriptions count] ? (__bridge CMFormatDescriptionRef)[formatDescriptions objectAtIndex:0] : 0;
        if (description) {
            m_originalCodec = PAL::softLink_CoreMedia_CMFormatDescriptionGetMediaSubType(description);
            CFStringRef originalFormatKey = PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_ProtectedContentOriginalFormat() ? PAL::get_CoreMedia_kCMFormatDescriptionExtension_ProtectedContentOriginalFormat() : CFSTR("CommonEncryptionOriginalFormat");
            CFTypeRef originalFormat = CMFormatDescriptionGetExtension(description, originalFormatKey);
            if (originalFormat && CFGetTypeID(originalFormat) == CFNumberGetTypeID())
                CFNumberGetValue((CFNumberRef)originalFormat, kCFNumberSInt32Type, &m_originalCodec);
        }
    }

    FourCharCode m_originalCodec;
    mutable Optional<AtomString> m_codec;
    bool m_isVideo;
    bool m_isAudio;
    bool m_isText;
};

#pragma mark -
#pragma mark SourceBufferParserAVFObjC

MediaPlayerEnums::SupportsType SourceBufferParserAVFObjC::isContentTypeSupported(const ContentType& type)
{
    String extendedType = type.raw();
    String outputCodecs = type.parameter(ContentType::codecsParameter());
    if (!outputCodecs.isEmpty() && [PAL::getAVStreamDataParserClass() respondsToSelector:@selector(outputMIMECodecParameterForInputMIMECodecParameter:)]) {
        outputCodecs = [PAL::getAVStreamDataParserClass() outputMIMECodecParameterForInputMIMECodecParameter:outputCodecs];
        extendedType = makeString(type.containerType(), "; codecs=\"", outputCodecs, "\"");
    }

    auto& streamDataParserCache = AVStreamDataParserMIMETypeCache::singleton();
    if (streamDataParserCache.isAvailable())
        return streamDataParserCache.canDecodeType(extendedType);

    auto& assetCache = AVAssetMIMETypeCache::singleton();
    if (assetCache.isAvailable())
        return assetCache.canDecodeType(extendedType);

    return MediaPlayerEnums::SupportsType::IsNotSupported;
}

SourceBufferParserAVFObjC::SourceBufferParserAVFObjC()
    : m_parser(adoptNS([PAL::allocAVStreamDataParserInstance() init]))
    , m_delegate(adoptNS([[WebAVStreamDataParserListener alloc] initWithParser:m_parser.get() parent:this]))
{
}

SourceBufferParserAVFObjC::~SourceBufferParserAVFObjC()
{
    m_delegate.get().parent = nullptr;
    [m_delegate invalidate];
}

void SourceBufferParserAVFObjC::appendData(Vector<unsigned char>&& data, AppendFlags flags)
{
    auto sharedData = SharedBuffer::create(WTFMove(data));
    auto nsData = sharedData->createNSData();
    if (m_parserStateWasReset || flags == AppendFlags::Discontinuity)
        [m_parser appendStreamData:nsData.get() withFlags:AVStreamDataParserStreamDataDiscontinuity];
    else
        [m_parser appendStreamData:nsData.get()];
    m_parserStateWasReset = false;
}

void SourceBufferParserAVFObjC::flushPendingMediaData()
{
    [m_parser providePendingMediaData];
}

void SourceBufferParserAVFObjC::setShouldProvideMediaDataForTrackID(bool should, uint64_t trackID)
{
    [m_parser setShouldProvideMediaData:should forTrackID:trackID];
}

bool SourceBufferParserAVFObjC::shouldProvideMediadataForTrackID(uint64_t trackID)
{
    return [m_parser shouldProvideMediaDataForTrackID:trackID];
}

void SourceBufferParserAVFObjC::resetParserState()
{
    m_parserStateWasReset = true;
    m_discardSamplesUntilNextInitializationSegment = true;
}

void SourceBufferParserAVFObjC::invalidate()
{
    [m_delegate invalidate];
    m_delegate = nullptr;
    m_parser = nullptr;
}

void SourceBufferParserAVFObjC::didParseStreamDataAsAsset(AVAsset* asset)
{
    callOnMainThread([this, strongThis = makeRef(*this), asset = retainPtr(asset)] {
        m_discardSamplesUntilNextInitializationSegment = false;

        if (!m_didParseInitializationDataCallback)
            return;

        InitializationSegment segment;

        if ([asset respondsToSelector:@selector(overallDurationHint)])
            segment.duration = PAL::toMediaTime([asset overallDurationHint]);

        if (segment.duration.isInvalid() || segment.duration == MediaTime::zeroTime())
            segment.duration = PAL::toMediaTime([asset duration]);

        for (AVAssetTrack* track in [asset tracks]) {
            if ([track hasMediaCharacteristic:AVMediaCharacteristicLegible]) {
                // FIXME(125161): Handle in-band text tracks.
                continue;
            }

            if ([track hasMediaCharacteristic:AVMediaCharacteristicVisual]) {
                SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info;
                auto videoTrack = VideoTrackPrivateMediaSourceAVFObjC::create(track);
                info.track = videoTrack.copyRef();
                info.description = MediaDescriptionAVFObjC::create(track);
                segment.videoTracks.append(info);
            } else if ([track hasMediaCharacteristic:AVMediaCharacteristicAudible]) {
                SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info;
                auto audioTrack = AudioTrackPrivateMediaSourceAVFObjC::create(track);
                info.track = audioTrack.copyRef();
                info.description = MediaDescriptionAVFObjC::create(track);
                segment.audioTracks.append(info);
            }

            // FIXME(125161)    : Add TextTrack support
        }

        m_didParseInitializationDataCallback(WTFMove(segment));
    });
}

void SourceBufferParserAVFObjC::didFailToParseStreamDataWithError(NSError* error)
{
    callOnMainThread([this, strongThis = makeRef(*this), error = retainPtr(error)] {
        if (m_didEncounterErrorDuringParsingCallback)
            m_didEncounterErrorDuringParsingCallback(error.get().code);
    });
}

void SourceBufferParserAVFObjC::didProvideMediaDataForTrackID(uint64_t trackID, CMSampleBufferRef sampleBuffer, const String& mediaType, unsigned flags)
{
    UNUSED_PARAM(flags);
    callOnMainThread([this, strongThis = makeRef(*this), sampleBuffer = retainPtr(sampleBuffer), trackID, mediaType = mediaType] {
        if (!m_didProvideMediaDataCallback)
            return;

        if (m_discardSamplesUntilNextInitializationSegment)
            return;

        auto mediaSample = MediaSampleAVFObjC::create(sampleBuffer.get(), trackID);

        if (mediaSample->isHomogeneous()) {
            m_didProvideMediaDataCallback(WTFMove(mediaSample), trackID, mediaType);
            return;
        }

        for (auto& sample : mediaSample->divideIntoHomogeneousSamples())
            m_didProvideMediaDataCallback(WTFMove(sample), trackID, mediaType);
    });
}

void SourceBufferParserAVFObjC::willProvideContentKeyRequestInitializationDataForTrackID(uint64_t trackID)
{
    m_willProvideContentKeyRequestInitializationDataForTrackIDCallback(trackID);
}

void SourceBufferParserAVFObjC::didProvideContentKeyRequestInitializationDataForTrackID(NSData* nsInitData, uint64_t trackID)
{
    auto initData = Uint8Array::create(nsInitData.length);
    [nsInitData getBytes:initData->data() length:initData->length()];

    m_didProvideContentKeyRequestInitializationDataForTrackIDCallback(WTFMove(initData), trackID);
}

}

#endif // ENABLE(MEDIA_SOURCE)
