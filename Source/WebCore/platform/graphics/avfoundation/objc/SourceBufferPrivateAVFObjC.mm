/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "SourceBufferPrivateAVFObjC.h"

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#import "ExceptionCodePlaceholder.h"
#import "Logging.h"
#import "MediaDescription.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"
#import "MediaSample.h"
#import "MediaSourcePrivateAVFObjC.h"
#import "MediaTimeMac.h"
#import "NotImplemented.h"
#import "SoftLinking.h"
#import "SourceBufferPrivateClient.h"
#import "TimeRanges.h"
#import "AudioTrackPrivateMediaSourceAVFObjC.h"
#import "VideoTrackPrivateMediaSourceAVFObjC.h"
#import "InbandTextTrackPrivateAVFObjC.h"
#import <AVFoundation/AVAssetTrack.h>
#import <CoreMedia/CMSampleBuffer.h>
#import <objc/runtime.h>
#import <wtf/text/AtomicString.h>
#import <wtf/text/CString.h>
#import <wtf/HashCountedSet.h>
#import <wtf/WeakPtr.h>
#import <map>

#pragma mark -
#pragma mark Soft Linking

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_FRAMEWORK_OPTIONAL(CoreMedia)

SOFT_LINK_CLASS(AVFoundation, AVAssetTrack)
SOFT_LINK_CLASS(AVFoundation, AVStreamDataParser)
SOFT_LINK_CLASS(AVFoundation, AVSampleBufferAudioRenderer)
SOFT_LINK_CLASS(AVFoundation, AVSampleBufferDisplayLayer)

SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVMediaTypeText, NSString *)

SOFT_LINK_CONSTANT(CoreMedia, kCMTimeZero, CMTime);
SOFT_LINK_CONSTANT(CoreMedia, kCMTimeInvalid, CMTime);
SOFT_LINK_CONSTANT(CoreMedia, kCMSampleAttachmentKey_DoNotDisplay, CFStringRef)
SOFT_LINK_CONSTANT(CoreMedia, kCMSampleAttachmentKey_NotSync, CFStringRef)
SOFT_LINK_CONSTANT(CoreMedia, kCMSampleBufferAttachmentKey_DrainAfterDecoding, CFStringRef)
SOFT_LINK_CONSTANT(CoreMedia, kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding, CFStringRef)
SOFT_LINK_CONSTANT(CoreMedia, kCMSampleBufferAttachmentKey_EmptyMedia, CFStringRef)
SOFT_LINK_CONSTANT(CoreMedia, kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately, CFStringRef)

SOFT_LINK_CONSTANT(AVFoundation, AVMediaCharacteristicVisual, NSString*)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaCharacteristicAudible, NSString*)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaCharacteristicLegible, NSString*)

SOFT_LINK(CoreMedia, CMFormatDescriptionGetMediaType, CMMediaType, (CMFormatDescriptionRef desc), (desc))
SOFT_LINK(CoreMedia, CMSampleBufferCreate, OSStatus, (CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady, CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon, CMFormatDescriptionRef formatDescription, CMItemCount numSamples, CMItemCount numSampleTimingEntries, const CMSampleTimingInfo *sampleTimingArray, CMItemCount numSampleSizeEntries, const size_t *sampleSizeArray, CMSampleBufferRef *sBufOut), (allocator, dataBuffer, dataReady, makeDataReadyCallback, makeDataReadyRefcon, formatDescription, numSamples, numSampleTimingEntries, sampleTimingArray, numSampleSizeEntries, sampleSizeArray, sBufOut))
SOFT_LINK(CoreMedia, CMSampleBufferCreateCopy, OSStatus, (CFAllocatorRef allocator, CMSampleBufferRef sbuf, CMSampleBufferRef *sbufCopyOut), (allocator, sbuf, sbufCopyOut))
SOFT_LINK(CoreMedia, CMSampleBufferCallForEachSample, OSStatus, (CMSampleBufferRef sbuf, OSStatus (*callback)( CMSampleBufferRef sampleBuffer, CMItemCount index, void *refcon), void *refcon), (sbuf, callback, refcon))
SOFT_LINK(CoreMedia, CMSampleBufferGetDecodeTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf))
SOFT_LINK(CoreMedia, CMSampleBufferGetDuration, CMTime, (CMSampleBufferRef sbuf), (sbuf))
SOFT_LINK(CoreMedia, CMSampleBufferGetPresentationTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf))
SOFT_LINK(CoreMedia, CMSampleBufferGetSampleAttachmentsArray, CFArrayRef, (CMSampleBufferRef sbuf, Boolean createIfNecessary), (sbuf, createIfNecessary))
SOFT_LINK(CoreMedia, CMFormatDescriptionGetMediaSubType, FourCharCode, (CMFormatDescriptionRef desc), (desc))
SOFT_LINK(CoreMedia, CMSetAttachment, void, (CMAttachmentBearerRef target, CFStringRef key, CFTypeRef value, CMAttachmentMode attachmentMode), (target, key, value, attachmentMode))

#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVMediaTypeAudio getAVMediaTypeAudio()
#define AVMediaTypeText getAVMediaTypeText()
#define kCMTimeZero getkCMTimeZero()
#define kCMTimeInvalid getkCMTimeInvalid()
#define kCMSampleAttachmentKey_NotSync getkCMSampleAttachmentKey_NotSync()
#define kCMSampleAttachmentKey_DoNotDisplay getkCMSampleAttachmentKey_DoNotDisplay()
#define kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding getkCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding()
#define kCMSampleBufferAttachmentKey_DrainAfterDecoding getkCMSampleBufferAttachmentKey_DrainAfterDecoding()
#define kCMSampleBufferAttachmentKey_EmptyMedia getkCMSampleBufferAttachmentKey_EmptyMedia()
#define kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately getkCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately()

#define AVMediaCharacteristicVisual getAVMediaCharacteristicVisual()
#define AVMediaCharacteristicAudible getAVMediaCharacteristicAudible()
#define AVMediaCharacteristicLegible getAVMediaCharacteristicLegible()

#pragma mark -
#pragma mark AVStreamDataParser

@interface AVStreamDataParser : NSObject
- (void)setDelegate:(id)delegate;
- (void)appendStreamData:(NSData *)data;
- (void)setShouldProvideMediaData:(BOOL)shouldProvideMediaData forTrackID:(CMPersistentTrackID)trackID;
- (BOOL)shouldProvideMediaDataForTrackID:(CMPersistentTrackID)trackID;
@end

#pragma mark -
#pragma mark AVSampleBufferDisplayLayer

@interface AVSampleBufferDisplayLayer : CALayer
- (NSInteger)status;
- (NSError*)error;
- (void)enqueueSampleBuffer:(CMSampleBufferRef)sampleBuffer;
- (void)flush;
- (BOOL)isReadyForMoreMediaData;
- (void)requestMediaDataWhenReadyOnQueue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block;
- (void)stopRequestingMediaData;
@end

#pragma mark -
#pragma mark AVSampleBufferAudioRenderer

@interface AVSampleBufferAudioRenderer : NSObject
- (NSInteger)status;
- (NSError*)error;
- (void)enqueueSampleBuffer:(CMSampleBufferRef)sampleBuffer;
- (void)flush;
- (BOOL)isReadyForMoreMediaData;
- (void)requestMediaDataWhenReadyOnQueue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block;
- (void)stopRequestingMediaData;
@end

#pragma mark -
#pragma mark WebAVStreamDataParserListener

@interface WebAVStreamDataParserListener : NSObject {
    WebCore::SourceBufferPrivateAVFObjC* _parent;
    AVStreamDataParser* _parser;
}
- (id)initWithParser:(AVStreamDataParser*)parser parent:(WebCore::SourceBufferPrivateAVFObjC*)parent;
@end

@implementation WebAVStreamDataParserListener
- (id)initWithParser:(AVStreamDataParser*)parser parent:(WebCore::SourceBufferPrivateAVFObjC*)parent
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

- (void)dealloc
{
    [_parser setDelegate:nil];
    [super dealloc];
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didParseStreamDataAsAsset:(AVAsset *)asset
{
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    _parent->didParseStreamDataAsAsset(asset);
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didParseStreamDataAsAsset:(AVAsset *)asset withDiscontinuity:(BOOL)discontinuity
{
    UNUSED_PARAM(discontinuity);
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    _parent->didParseStreamDataAsAsset(asset);
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didFailToParseStreamDataWithError:(NSError *)error
{
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    _parent->didFailToParseStreamDataWithError(error);
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didProvideMediaData:(CMSampleBufferRef)mediaData forTrackID:(CMPersistentTrackID)trackID mediaType:(NSString *)mediaType flags:(NSUInteger)flags
{
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    _parent->didProvideMediaDataForTrackID(trackID, mediaData, mediaType, flags);
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didReachEndOfTrackWithTrackID:(CMPersistentTrackID)trackID mediaType:(NSString *)mediaType
{
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    _parent->didReachEndOfTrackWithTrackID(trackID, mediaType);
}
@end

namespace WebCore {

#pragma mark -
#pragma mark MediaSampleAVFObjC

class MediaSampleAVFObjC final : public MediaSample {
public:
    static RefPtr<MediaSampleAVFObjC> create(CMSampleBufferRef sample, int trackID) { return adoptRef(new MediaSampleAVFObjC(sample, trackID)); }
    virtual ~MediaSampleAVFObjC() { }

    virtual MediaTime presentationTime() const override { return toMediaTime(CMSampleBufferGetPresentationTimeStamp(m_sample.get())); }
    virtual MediaTime decodeTime() const override { return toMediaTime(CMSampleBufferGetDecodeTimeStamp(m_sample.get())); }
    virtual MediaTime duration() const override { return toMediaTime(CMSampleBufferGetDuration(m_sample.get())); }
    virtual AtomicString trackID() const override { return m_id; }

    virtual SampleFlags flags() const override;
    virtual PlatformSample platformSample() override;

protected:
    MediaSampleAVFObjC(CMSampleBufferRef sample, int trackID)
        : m_sample(sample)
        , m_id(String::format("%d", trackID))
    {
    }

    RetainPtr<CMSampleBufferRef> m_sample;
    AtomicString m_id;
};

PlatformSample MediaSampleAVFObjC::platformSample()
{
    PlatformSample sample = { PlatformSample::CMSampleBufferType, { .cmSampleBuffer = m_sample.get() } };
    return sample;
}

static bool CMSampleBufferIsRandomAccess(CMSampleBufferRef sample)
{
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return true;

    for (CFIndex i = 0, count = CFArrayGetCount(attachments); i < count; ++i) {
        CFDictionaryRef attachmentDict = (CFDictionaryRef)CFArrayGetValueAtIndex(attachments, i);
        if (CFDictionaryContainsKey(attachmentDict, kCMSampleAttachmentKey_NotSync))
            return false;
    }
    return true;
}

MediaSample::SampleFlags MediaSampleAVFObjC::flags() const
{
    int returnValue = MediaSample::None;

    if (CMSampleBufferIsRandomAccess(m_sample.get()))
        returnValue |= MediaSample::IsSync;

    return SampleFlags(returnValue);
}

#pragma mark -
#pragma mark MediaDescriptionAVFObjC

class MediaDescriptionAVFObjC final : public MediaDescription {
public:
    static RefPtr<MediaDescriptionAVFObjC> create(AVAssetTrack* track) { return adoptRef(new MediaDescriptionAVFObjC(track)); }
    virtual ~MediaDescriptionAVFObjC() { }

    virtual AtomicString codec() const override { return m_codec; }
    virtual bool isVideo() const override { return m_isVideo; }
    virtual bool isAudio() const override { return m_isAudio; }
    virtual bool isText() const override { return m_isText; }
    
protected:
    MediaDescriptionAVFObjC(AVAssetTrack* track)
        : m_isVideo([track hasMediaCharacteristic:AVMediaCharacteristicVisual])
        , m_isAudio([track hasMediaCharacteristic:AVMediaCharacteristicAudible])
        , m_isText([track hasMediaCharacteristic:AVMediaCharacteristicLegible])
    {
        NSArray* formatDescriptions = [track formatDescriptions];
        CMFormatDescriptionRef description = [formatDescriptions count] ? (CMFormatDescriptionRef)[formatDescriptions objectAtIndex:0] : 0;
        if (description) {
            FourCharCode codec = CMFormatDescriptionGetMediaSubType(description);
            m_codec = AtomicString(reinterpret_cast<LChar*>(&codec), 4);
        }
    }

    AtomicString m_codec;
    bool m_isVideo;
    bool m_isAudio;
    bool m_isText;
};

#pragma mark -
#pragma mark SourceBufferPrivateAVFObjC

RefPtr<SourceBufferPrivateAVFObjC> SourceBufferPrivateAVFObjC::create(MediaSourcePrivateAVFObjC* parent)
{
    return adoptRef(new SourceBufferPrivateAVFObjC(parent));
}

SourceBufferPrivateAVFObjC::SourceBufferPrivateAVFObjC(MediaSourcePrivateAVFObjC* parent)
    : m_parser(adoptNS([[getAVStreamDataParserClass() alloc] init]))
    , m_delegate(adoptNS([[WebAVStreamDataParserListener alloc] initWithParser:m_parser.get() parent:this]))
    , m_mediaSource(parent)
    , m_client(0)
    , m_parsingSucceeded(true)
    , m_enabledVideoTrackID(-1)
{
}

SourceBufferPrivateAVFObjC::~SourceBufferPrivateAVFObjC()
{
    destroyRenderers();
}

void SourceBufferPrivateAVFObjC::didParseStreamDataAsAsset(AVAsset* asset)
{
    LOG(Media, "SourceBufferPrivateAVFObjC::didParseStreamDataAsAsset(%p)", this);

    m_asset = asset;

    m_videoTracks.clear();
    m_audioTracks.clear();

    SourceBufferPrivateClient::InitializationSegment segment;
    segment.duration = toMediaTime([m_asset duration]);

    for (AVAssetTrack* track in [m_asset tracks]) {
        if ([track hasMediaCharacteristic:AVMediaCharacteristicVisual]) {
            SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info;
            RefPtr<VideoTrackPrivateMediaSourceAVFObjC> videoTrack = VideoTrackPrivateMediaSourceAVFObjC::create(track, this);
            info.track = videoTrack;
            m_videoTracks.append(videoTrack);
            info.description = MediaDescriptionAVFObjC::create(track);
            segment.videoTracks.append(info);
        } else if ([track hasMediaCharacteristic:AVMediaCharacteristicAudible]) {
            SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info;
            RefPtr<AudioTrackPrivateMediaSourceAVFObjC> audioTrack = AudioTrackPrivateMediaSourceAVFObjC::create(track, this);
            info.track = audioTrack;
            m_audioTracks.append(audioTrack);
            info.description = MediaDescriptionAVFObjC::create(track);
            segment.audioTracks.append(info);
        }

        // FIXME(125161): Add TextTrack support
    }

    if (!m_videoTracks.isEmpty())
        m_mediaSource->player()->sizeChanged();

    if (m_client)
        m_client->sourceBufferPrivateDidReceiveInitializationSegment(this, segment);
}

void SourceBufferPrivateAVFObjC::didFailToParseStreamDataWithError(NSError* error)
{
#if LOG_DISABLED
    UNUSED_PARAM(error);
#endif
    LOG(Media, "SourceBufferPrivateAVFObjC::didFailToParseStreamDataWithError(%p) - error:\"%s\"", this, String([error description]).utf8().data());

    m_parsingSucceeded = false;
}

struct ProcessCodedFrameInfo {
    SourceBufferPrivateAVFObjC* sourceBuffer;
    int trackID;
    const String& mediaType;
};

void SourceBufferPrivateAVFObjC::didProvideMediaDataForTrackID(int trackID, CMSampleBufferRef sampleBuffer, const String& mediaType, unsigned flags)
{
    UNUSED_PARAM(flags);

    processCodedFrame(trackID, sampleBuffer, mediaType);
}

bool SourceBufferPrivateAVFObjC::processCodedFrame(int trackID, CMSampleBufferRef sampleBuffer, const String&)
{
    if (m_client)
        m_client->sourceBufferPrivateDidReceiveSample(this, MediaSampleAVFObjC::create(sampleBuffer, trackID));

    return true;
}

void SourceBufferPrivateAVFObjC::didReachEndOfTrackWithTrackID(int trackID, const String& mediaType)
{
    UNUSED_PARAM(mediaType);
    UNUSED_PARAM(trackID);
    notImplemented();
}

void SourceBufferPrivateAVFObjC::setClient(SourceBufferPrivateClient* client)
{
    m_client = client;
}

SourceBufferPrivate::AppendResult SourceBufferPrivateAVFObjC::append(const unsigned char* data, unsigned length)
{
    m_parsingSucceeded = true;

    LOG(Media, "SourceBufferPrivateAVFObjC::append(%p) - data:%p, length:%d", this, data, length);
    [m_parser appendStreamData:[NSData dataWithBytes:data length:length]];

    if (m_parsingSucceeded && m_mediaSource)
        m_mediaSource->player()->setLoadingProgresssed(true);

    return m_parsingSucceeded ? AppendSucceeded : ParsingFailed;
}

void SourceBufferPrivateAVFObjC::abort()
{
    notImplemented();
}

void SourceBufferPrivateAVFObjC::destroyRenderers()
{
    if (m_displayLayer) {
        if (m_mediaSource)
            m_mediaSource->player()->removeDisplayLayer(m_displayLayer.get());
        [m_displayLayer flush];
        [m_displayLayer stopRequestingMediaData];
        m_displayLayer = nullptr;
    }

    for (auto it = m_audioRenderers.begin(), end = m_audioRenderers.end(); it != end; ++it) {
        AVSampleBufferAudioRenderer* renderer = it->second.get();
        if (m_mediaSource)
            m_mediaSource->player()->removeAudioRenderer(renderer);
        [renderer flush];
        [renderer stopRequestingMediaData];
    }

    m_audioRenderers.clear();
}

void SourceBufferPrivateAVFObjC::removedFromMediaSource()
{
    destroyRenderers();

    if (m_mediaSource)
        m_mediaSource->removeSourceBuffer(this);
}

MediaPlayer::ReadyState SourceBufferPrivateAVFObjC::readyState() const
{
    return m_mediaSource ? m_mediaSource->player()->readyState() : MediaPlayer::HaveNothing;
}

void SourceBufferPrivateAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_mediaSource)
        m_mediaSource->player()->setReadyState(readyState);
}

void SourceBufferPrivateAVFObjC::evictCodedFrames()
{
    notImplemented();
}

bool SourceBufferPrivateAVFObjC::isFull()
{
    notImplemented();
    return false;
}


bool SourceBufferPrivateAVFObjC::hasVideo() const
{
    if (!m_client)
        return false;

    return m_client->sourceBufferPrivateHasVideo(this);
}

bool SourceBufferPrivateAVFObjC::hasAudio() const
{
    if (!m_client)
        return false;

    return m_client->sourceBufferPrivateHasAudio(this);
}

void SourceBufferPrivateAVFObjC::trackDidChangeEnabled(VideoTrackPrivateMediaSourceAVFObjC* track)
{
    int trackID = track->trackID();
    if (!track->selected() && m_enabledVideoTrackID == trackID) {
        m_enabledVideoTrackID = -1;
        [m_parser setShouldProvideMediaData:NO forTrackID:trackID];
        if (m_mediaSource)
            m_mediaSource->player()->removeDisplayLayer(m_displayLayer.get());
    } else if (track->selected()) {
        m_enabledVideoTrackID = trackID;
        [m_parser setShouldProvideMediaData:YES forTrackID:trackID];
        if (!m_displayLayer) {
            m_displayLayer = [[getAVSampleBufferDisplayLayerClass() alloc] init];
            [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
                didBecomeReadyForMoreSamples(trackID);
            }];
        }
        if (m_mediaSource)
            m_mediaSource->player()->addDisplayLayer(m_displayLayer.get());
    }
}

void SourceBufferPrivateAVFObjC::trackDidChangeEnabled(AudioTrackPrivateMediaSourceAVFObjC* track)
{
    int trackID = track->trackID();

    if (!track->enabled()) {
        AVSampleBufferAudioRenderer* renderer = m_audioRenderers[trackID].get();
        [m_parser setShouldProvideMediaData:NO forTrackID:trackID];
        if (m_mediaSource)
            m_mediaSource->player()->removeAudioRenderer(renderer);
    } else {
        [m_parser setShouldProvideMediaData:YES forTrackID:trackID];
        AVSampleBufferAudioRenderer* renderer;
        if (!m_audioRenderers.count(trackID)) {
            renderer = [[getAVSampleBufferAudioRendererClass() alloc] init];
            [renderer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
                didBecomeReadyForMoreSamples(trackID);
            }];
            m_audioRenderers[trackID] = renderer;
        } else
            renderer = m_audioRenderers[trackID].get();

        if (m_mediaSource)
            m_mediaSource->player()->addAudioRenderer(renderer);
    }
}

static RetainPtr<CMSampleBufferRef> createNonDisplayingCopy(CMSampleBufferRef sampleBuffer)
{
    CMSampleBufferRef newSampleBuffer = 0;
    CMSampleBufferCreateCopy(kCFAllocatorDefault, sampleBuffer, &newSampleBuffer);
    if (!newSampleBuffer)
        return sampleBuffer;

    CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(newSampleBuffer, true);
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachmentsArray, i);
        CFDictionarySetValue(attachments, kCMSampleAttachmentKey_DoNotDisplay, kCFBooleanTrue);
    }

    return adoptCF(newSampleBuffer);
}

void SourceBufferPrivateAVFObjC::flushAndEnqueueNonDisplayingSamples(Vector<RefPtr<MediaSample>> mediaSamples, AtomicString trackIDString)
{
    int trackID = trackIDString.toInt();
    LOG(Media, "SourceBufferPrivateAVFObjC::flushAndEnqueueNonDisplayingSamples(%p) samples: %d samples, trackId: %d", this, mediaSamples.size(), trackID);

    if (trackID == m_enabledVideoTrackID)
        flushAndEnqueueNonDisplayingSamples(mediaSamples, m_displayLayer.get());
    else if (m_audioRenderers.count(trackID))
        flushAndEnqueueNonDisplayingSamples(mediaSamples, m_audioRenderers[trackID].get());
}

void SourceBufferPrivateAVFObjC::flushAndEnqueueNonDisplayingSamples(Vector<RefPtr<MediaSample>> mediaSamples, AVSampleBufferAudioRenderer* renderer)
{
    [renderer flush];

    for (auto it = mediaSamples.begin(), end = mediaSamples.end(); it != end; ++it) {
        RefPtr<MediaSample>& mediaSample = *it;

        PlatformSample platformSample = mediaSample->platformSample();
        ASSERT(platformSample.type == PlatformSample::CMSampleBufferType);

        RetainPtr<CMSampleBufferRef> sampleBuffer = createNonDisplayingCopy(platformSample.sample.cmSampleBuffer);

        [renderer enqueueSampleBuffer:sampleBuffer.get()];
    }
}

void SourceBufferPrivateAVFObjC::flushAndEnqueueNonDisplayingSamples(Vector<RefPtr<MediaSample>> mediaSamples, AVSampleBufferDisplayLayer* layer)
{
    [layer flush];

    for (auto it = mediaSamples.begin(), end = mediaSamples.end(); it != end; ++it) {
        RefPtr<MediaSample>& mediaSample = *it;

        PlatformSample platformSample = mediaSample->platformSample();
        ASSERT(platformSample.type == PlatformSample::CMSampleBufferType);

        RetainPtr<CMSampleBufferRef> sampleBuffer = createNonDisplayingCopy(platformSample.sample.cmSampleBuffer);

        [layer enqueueSampleBuffer:sampleBuffer.get()];
    }

    if (m_mediaSource)
        m_mediaSource->player()->setHasAvailableVideoFrame(false);
}

void SourceBufferPrivateAVFObjC::enqueueSample(PassRefPtr<MediaSample> prpMediaSample, AtomicString trackIDString)
{
    int trackID = trackIDString.toInt();
    if (trackID != m_enabledVideoTrackID && !m_audioRenderers.count(trackID))
        return;

    RefPtr<MediaSample> mediaSample = prpMediaSample;

    PlatformSample platformSample = mediaSample->platformSample();
    if (platformSample.type != PlatformSample::CMSampleBufferType)
        return;

    if (trackID == m_enabledVideoTrackID) {
        [m_displayLayer enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
        if (m_mediaSource)
            m_mediaSource->player()->setHasAvailableVideoFrame(true);
    } else
        [m_audioRenderers[trackID] enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
}

bool SourceBufferPrivateAVFObjC::isReadyForMoreSamples(AtomicString trackIDString)
{
    int trackID = trackIDString.toInt();
    if (trackID == m_enabledVideoTrackID)
        return [m_displayLayer isReadyForMoreMediaData];
    else if (m_audioRenderers.count(trackID))
        return [m_audioRenderers[trackID] isReadyForMoreMediaData];
    else
        ASSERT_NOT_REACHED();

    return false;
}

void SourceBufferPrivateAVFObjC::setActive(bool isActive)
{
    if (m_mediaSource)
        m_mediaSource->sourceBufferPrivateDidChangeActiveState(this, isActive);
}

MediaTime SourceBufferPrivateAVFObjC::fastSeekTimeForMediaTime(MediaTime time, MediaTime negativeThreshold, MediaTime positiveThreshold)
{
    if (m_client)
        return m_client->sourceBufferPrivateFastSeekTimeForMediaTime(this, time, negativeThreshold, positiveThreshold);
    return time;
}

void SourceBufferPrivateAVFObjC::seekToTime(MediaTime time)
{
    if (m_client)
        m_client->sourceBufferPrivateSeekToTime(this, time);
}

IntSize SourceBufferPrivateAVFObjC::naturalSize()
{
    for (auto videoTrack : m_videoTracks) {
        if (videoTrack->selected())
            return videoTrack->naturalSize();
    }

    return IntSize();
}

void SourceBufferPrivateAVFObjC::didBecomeReadyForMoreSamples(int trackID)
{
    if (trackID == m_enabledVideoTrackID)
        [m_displayLayer stopRequestingMediaData];
    else if (m_audioRenderers.count(trackID))
        [m_audioRenderers[trackID] stopRequestingMediaData];
    else {
        ASSERT_NOT_REACHED();
        return;
    }

    if (m_client)
        m_client->sourceBufferPrivateDidBecomeReadyForMoreSamples(this, AtomicString::number(trackID));
}

void SourceBufferPrivateAVFObjC::notifyClientWhenReadyForMoreSamples(AtomicString trackIDString)
{
    int trackID = trackIDString.toInt();
    if (trackID == m_enabledVideoTrackID) {
        [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
            didBecomeReadyForMoreSamples(trackID);
        }];
    } else if (m_audioRenderers.count(trackID)) {
        [m_audioRenderers[trackID] requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
            didBecomeReadyForMoreSamples(trackID);
        }];
    } else
        ASSERT_NOT_REACHED();
}

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
