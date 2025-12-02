/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#import "MediaDescription.h"
#import "MediaSample.h"
#import "MediaSampleAVFObjC.h"
#import "MediaSessionManagerCocoa.h"
#import "NotImplemented.h"
#import "SharedBuffer.h"
#import "SourceBufferPrivate.h"
#import "TextTrackPrivateMediaSourceAVFObjC.h"
#import "TimeRanges.h"
#import "VideoTrackPrivateMediaSourceAVFObjC.h"
#import <AVFoundation/AVAssetTrack.h>
#import <JavaScriptCore/TypedArrayInlines.h>
#import <objc/runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/text/MakeString.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

#pragma mark -
#pragma mark WebAVStreamDataParserListener

@interface AVContentKeySpecifier (WebCorePrivate)
@property (readonly) NSData *initializationData;
@end

@interface WebAVStreamDataParserListener : NSObject<AVStreamDataParserOutputHandling> {
    ThreadSafeWeakPtr<WebCore::SourceBufferParserAVFObjC> _parent;
    WeakObjCPtr<AVStreamDataParser> _parser;
}
@property (assign) ThreadSafeWeakPtr<WebCore::SourceBufferParserAVFObjC> parent;
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

@synthesize parent = _parent;

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
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser.get());
    _parent.get()->didParseStreamDataAsAsset(asset);
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didParseStreamDataAsAsset:(AVAsset *)asset withDiscontinuity:(BOOL)discontinuity
{
    UNUSED_PARAM(discontinuity);
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser.get());
    _parent.get()->didParseStreamDataAsAsset(asset);
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didFailToParseStreamDataWithError:(NSError *)error
{
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser.get());
    _parent.get()->didFailToParseStreamDataWithError(error);
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didProvideMediaData:(CMSampleBufferRef)sample forTrackID:(CMPersistentTrackID)trackID mediaType:(NSString *)nsMediaType flags:(AVStreamDataParserOutputMediaDataFlags)flags
{
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser.get());
    _parent.get()->didProvideMediaDataForTrackID(trackID, sample, nsMediaType, flags);
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)streamDataParserWillProvideContentKeyRequestInitializationData:(AVStreamDataParser *)streamDataParser forTrackID:(CMPersistentTrackID)trackID
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser.get());
    _parent.get()->willProvideContentKeyRequestInitializationDataForTrackID(trackID);
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didProvideContentKeyRequestInitializationData:(NSData *)initData forTrackID:(CMPersistentTrackID)trackID
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser.get());
    _parent.get()->didProvideContentKeyRequestInitializationDataForTrackID(initData, trackID);
}

@end

@interface WebAVStreamDataParserWithKeySpecifierListener : WebAVStreamDataParserListener
@end

@implementation WebAVStreamDataParserWithKeySpecifierListener
- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didProvideContentKeySpecifier:(AVContentKeySpecifier *)keySpecifier forTrackID:(CMPersistentTrackID)trackID
{
    ASSERT_UNUSED(streamDataParser, streamDataParser == _parser.get());
    if ([keySpecifier respondsToSelector:@selector(initializationData)])
        _parent.get()->didProvideContentKeyRequestSpecifierForTrackID(keySpecifier.initializationData, trackID);
}
@end

namespace WebCore {

#pragma mark -
#pragma mark MediaDescriptionAVFObjC

class MediaDescriptionAVFObjC final : public MediaDescription {
public:
    static Ref<MediaDescriptionAVFObjC> create(AVAssetTrack* track) { return adoptRef(*new MediaDescriptionAVFObjC(track)); }
    virtual ~MediaDescriptionAVFObjC() { }

    bool isVideo() const final { return m_isVideo; }
    bool isAudio() const final { return m_isAudio; }
    bool isText() const final { return m_isText; }

private:
    MediaDescriptionAVFObjC(AVAssetTrack* track)
        : MediaDescription(extractCodecName(track))
    {
        NSString* mediaType = [track mediaType];
        m_isVideo = [mediaType isEqualToString:AVMediaTypeVideo];
        m_isAudio = [mediaType isEqualToString:AVMediaTypeAudio];
        m_isText = [mediaType isEqualToString:AVMediaTypeText] || [mediaType isEqualToString:AVMediaTypeClosedCaption] || [mediaType isEqualToString:AVMediaTypeSubtitle];
    }

    String extractCodecName(AVAssetTrack* track)
    {
        NSArray* formatDescriptions = [track formatDescriptions];
        CMFormatDescriptionRef description = [formatDescriptions count] ? (__bridge CMFormatDescriptionRef)[formatDescriptions objectAtIndex:0] : 0;
        if (!description)
            return emptyString();
        FourCC originalCodec = PAL::softLink_CoreMedia_CMFormatDescriptionGetMediaSubType(description);
        CFStringRef originalFormatKey = PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_ProtectedContentOriginalFormat() ? PAL::kCMFormatDescriptionExtension_ProtectedContentOriginalFormat : CFSTR("CommonEncryptionOriginalFormat");
        if (auto originalFormat = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(description, originalFormatKey)))
            CFNumberGetValue(originalFormat, kCFNumberSInt32Type, &originalCodec.value);
        return String::fromLatin1(originalCodec.string().data());
    }
    bool m_isVideo { false };
    bool m_isAudio { false };
    bool m_isText { false };
};

#pragma mark -
#pragma mark SourceBufferParserAVFObjC

MediaPlayerEnums::SupportsType SourceBufferParserAVFObjC::isContentTypeSupported(const ContentType& type)
{
    // Check that AVStreamDataParser is in a functional state.
    if (!PAL::getAVStreamDataParserClassSingleton() || !adoptNS([PAL::allocAVStreamDataParserInstance() init]))
        return MediaPlayerEnums::SupportsType::IsNotSupported;

    String extendedType = type.raw();
    String outputCodecs = type.parameter(ContentType::codecsParameter());
    if (!outputCodecs.isEmpty() && [PAL::getAVStreamDataParserClassSingleton() respondsToSelector:@selector(outputMIMECodecParameterForInputMIMECodecParameter:)]) {
        outputCodecs = [PAL::getAVStreamDataParserClassSingleton() outputMIMECodecParameterForInputMIMECodecParameter:outputCodecs.createNSString().get()];
        extendedType = makeString(type.containerType(), "; codecs=\""_s, outputCodecs, "\""_s);
    }

    return AVStreamDataParserMIMETypeCache::singleton().canDecodeType(extendedType);
}

SourceBufferParserAVFObjC::SourceBufferParserAVFObjC(const MediaSourceConfiguration& configuration)
    : m_parser(adoptNS([PAL::allocAVStreamDataParserInstance() init]))
    , m_configuration(configuration)
{
    m_delegate = adoptNS([[WebAVStreamDataParserWithKeySpecifierListener alloc] initWithParser:m_parser.get() parent:this]);

#if USE(MEDIAPARSERD)
    if ([m_parser.get() respondsToSelector:@selector(setPreferSandboxedParsing:)])
        [m_parser.get() setPreferSandboxedParsing:!m_configuration.demuxInProcess];
#endif
}

SourceBufferParserAVFObjC::~SourceBufferParserAVFObjC()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_delegate.get().parent = nullptr;
    [m_delegate invalidate];
}

Expected<void, PlatformMediaError> SourceBufferParserAVFObjC::appendData(Ref<const SharedBuffer>&& segment, AppendFlags flags)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    RetainPtr nsData = segment->createNSData();
    if (m_parserStateWasReset || flags == AppendFlags::Discontinuity)
        [m_parser appendStreamData:nsData.get() withFlags:AVStreamDataParserStreamDataDiscontinuity];
    else
        [m_parser appendStreamData:nsData.get()];
    m_parserStateWasReset = false;

    if (m_lastErrorCode)
        return makeUnexpected(PlatformMediaError::ParsingError);
    return { };
}

void SourceBufferParserAVFObjC::flushPendingMediaData()
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    [m_parser providePendingMediaData];
}

void SourceBufferParserAVFObjC::resetParserState()
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_parserStateWasReset = true;
}

void SourceBufferParserAVFObjC::invalidate()
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    [m_delegate invalidate];
    m_delegate = nullptr;
    m_parser = nullptr;
}

#if !RELEASE_LOG_DISABLED
void SourceBufferParserAVFObjC::setLogger(const Logger& newLogger, uint64_t newLogIdentifier)
{
    m_logger = newLogger;
    m_logIdentifier = newLogIdentifier;
    ALWAYS_LOG(LOGIDENTIFIER);
}
#endif

void SourceBufferParserAVFObjC::didParseStreamDataAsAsset(AVAsset* asset)
{
    auto identifier = LOGIDENTIFIER;
    INFO_LOG_IF_POSSIBLE(identifier, asset);

    m_callOnClientThreadCallback([this, protectedThis = Ref { *this }, asset = retainPtr(asset), identifier] {
        if (!m_didParseInitializationDataCallback)
            return;

        InitializationSegment segment;

        if ([asset respondsToSelector:@selector(overallDurationHint)])
            segment.duration = PAL::toMediaTime([asset overallDurationHint]);

        if (segment.duration.isInvalid() || segment.duration == MediaTime::zeroTime())
            segment.duration = PAL::toMediaTime([asset duration]);

        for (AVAssetTrack* track in [asset tracks]) {
            NSString* mediaType = [track mediaType];
            if ([mediaType isEqualToString:AVMediaTypeVideo]) {
                SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info;
                info.track = VideoTrackPrivateMediaSourceAVFObjC::create(track);
                info.description = MediaDescriptionAVFObjC::create(track);
                segment.videoTracks.append(WTFMove(info));
            } else if ([mediaType isEqualToString:AVMediaTypeAudio]) {
                SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info;
                info.track = AudioTrackPrivateMediaSourceAVFObjC::create(track);
                info.description = MediaDescriptionAVFObjC::create(track);
                segment.audioTracks.append(WTFMove(info));
            } else if ([mediaType isEqualToString:AVMediaTypeText] && m_configuration.textTracksEnabled) {
                SourceBufferPrivateClient::InitializationSegment::TextTrackInformation info;
                Ref description = MediaDescriptionAVFObjC::create(track);
                info.description = description.copyRef();
                if (description->codec().toString() != "wvtt"_s) {
                    ALWAYS_LOG_IF_POSSIBLE(identifier, "Ignoring text track of type ", description->codec());
                    break;
                }
                info.track = TextTrackPrivateMediaSourceAVFObjC::create(track, InbandTextTrackPrivate::CueFormat::WebVTT);
                segment.textTracks.append(WTFMove(info));
            } else {
                ALWAYS_LOG_IF_POSSIBLE(identifier, "Ignoring track of type ", String(mediaType));
            }
        }

        m_didParseInitializationDataCallback(WTFMove(segment));
    });
}

void SourceBufferParserAVFObjC::didFailToParseStreamDataWithError(NSError* error)
{
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, error);
    m_lastErrorCode.emplace([error code]);
}

void SourceBufferParserAVFObjC::didProvideMediaDataForTrackID(TrackID trackID, CMSampleBufferRef sampleBuffer, const String& mediaType, unsigned flags)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, "trackID = ", trackID, ", mediaType = ", mediaType);
    UNUSED_PARAM(flags);
    m_callOnClientThreadCallback([this, protectedThis = Ref { *this }, sampleBuffer = retainPtr(sampleBuffer), trackID, mediaType = mediaType] {
        if (!m_didProvideMediaDataCallback)
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
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, "trackID = ", trackID);
    m_callOnClientThreadCallback([protectedThis = Ref { *this }, trackID] {
        protectedThis->m_willProvideContentKeyRequestInitializationDataForTrackIDCallback(trackID);
    });
}

void SourceBufferParserAVFObjC::didProvideContentKeyRequestInitializationDataForTrackID(NSData* nsInitData, uint64_t trackID)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, "trackID = ", trackID);
    m_callOnClientThreadCallback([protectedThis = Ref { *this }, initData = SharedBuffer::create(nsInitData), trackID]() mutable {
        protectedThis->m_didProvideContentKeyRequestInitializationDataForTrackIDCallback(WTFMove(initData), trackID);
    });
}

void SourceBufferParserAVFObjC::didProvideContentKeyRequestSpecifierForTrackID(NSData* nsInitData, uint64_t trackID)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, "trackID = ", trackID);
    m_callOnClientThreadCallback([protectedThis = Ref { *this }, initData = SharedBuffer::create(nsInitData), trackID]() mutable {
        protectedThis->m_didProvideContentKeyRequestIdentifierForTrackIDCallback(WTFMove(initData), trackID);
    });
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferParserAVFObjC::logChannel() const
{
    return LogMedia;
}
#endif // !RELEASE_LOG_DISABLED

}

#endif // ENABLE(MEDIA_SOURCE)
