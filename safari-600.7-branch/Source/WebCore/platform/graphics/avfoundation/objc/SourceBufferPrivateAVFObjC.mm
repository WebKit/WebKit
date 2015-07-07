/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#import "BlockExceptions.h"
#import "CDMSessionMediaSourceAVFObjC.h"
#import "ExceptionCodePlaceholder.h"
#import "Logging.h"
#import "MediaDescription.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"
#import "MediaSample.h"
#import "MediaSourcePrivateAVFObjC.h"
#import "MediaTimeAVFoundation.h"
#import "NotImplemented.h"
#import "SoftLinking.h"
#import "SourceBufferPrivateClient.h"
#import "TimeRanges.h"
#import "AudioTrackPrivateMediaSourceAVFObjC.h"
#import "VideoTrackPrivateMediaSourceAVFObjC.h"
#import "InbandTextTrackPrivateAVFObjC.h"
#import <AVFoundation/AVAssetTrack.h>
#import <CoreMedia/CMSampleBuffer.h>
#import <QuartzCore/CALayer.h>
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
SOFT_LINK_CLASS(AVFoundation, AVStreamSession)

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
SOFT_LINK_CONSTANT(AVFoundation, AVSampleBufferDisplayLayerFailedToDecodeNotification, NSString*)
SOFT_LINK_CONSTANT(AVFoundation, AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey, NSString*)

SOFT_LINK(CoreMedia, CMFormatDescriptionGetMediaType, CMMediaType, (CMFormatDescriptionRef desc), (desc))
SOFT_LINK(CoreMedia, CMSampleBufferCreate, OSStatus, (CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady, CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon, CMFormatDescriptionRef formatDescription, CMItemCount numSamples, CMItemCount numSampleTimingEntries, const CMSampleTimingInfo *sampleTimingArray, CMItemCount numSampleSizeEntries, const size_t *sampleSizeArray, CMSampleBufferRef *sBufOut), (allocator, dataBuffer, dataReady, makeDataReadyCallback, makeDataReadyRefcon, formatDescription, numSamples, numSampleTimingEntries, sampleTimingArray, numSampleSizeEntries, sampleSizeArray, sBufOut))
SOFT_LINK(CoreMedia, CMSampleBufferCreateCopy, OSStatus, (CFAllocatorRef allocator, CMSampleBufferRef sbuf, CMSampleBufferRef *sbufCopyOut), (allocator, sbuf, sbufCopyOut))
SOFT_LINK(CoreMedia, CMSampleBufferCallForEachSample, OSStatus, (CMSampleBufferRef sbuf, OSStatus (*callback)( CMSampleBufferRef sampleBuffer, CMItemCount index, void *refcon), void *refcon), (sbuf, callback, refcon))
SOFT_LINK(CoreMedia, CMSampleBufferGetDecodeTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf))
SOFT_LINK(CoreMedia, CMSampleBufferGetDuration, CMTime, (CMSampleBufferRef sbuf), (sbuf))
SOFT_LINK(CoreMedia, CMSampleBufferGetFormatDescription, CMFormatDescriptionRef, (CMSampleBufferRef sbuf), (sbuf))
SOFT_LINK(CoreMedia, CMSampleBufferGetPresentationTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf))
SOFT_LINK(CoreMedia, CMSampleBufferGetSampleAttachmentsArray, CFArrayRef, (CMSampleBufferRef sbuf, Boolean createIfNecessary), (sbuf, createIfNecessary))
SOFT_LINK(CoreMedia, CMSampleBufferGetTotalSampleSize, size_t, (CMSampleBufferRef sbuf), (sbuf))
SOFT_LINK(CoreMedia, CMFormatDescriptionGetMediaSubType, FourCharCode, (CMFormatDescriptionRef desc), (desc))
SOFT_LINK(CoreMedia, CMSetAttachment, void, (CMAttachmentBearerRef target, CFStringRef key, CFTypeRef value, CMAttachmentMode attachmentMode), (target, key, value, attachmentMode))
SOFT_LINK(CoreMedia, CMVideoFormatDescriptionGetPresentationDimensions, CGSize, (CMVideoFormatDescriptionRef videoDesc, Boolean usePixelAspectRatio, Boolean useCleanAperture), (videoDesc, usePixelAspectRatio, useCleanAperture))

#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVMediaTypeAudio getAVMediaTypeAudio()
#define AVMediaTypeText getAVMediaTypeText()
#define AVSampleBufferDisplayLayerFailedToDecodeNotification getAVSampleBufferDisplayLayerFailedToDecodeNotification()
#define AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey getAVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey()
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
#pragma mark AVStreamSession

@interface AVStreamSession : NSObject
- (void)addStreamDataParser:(AVStreamDataParser *)streamDataParser;
- (void)removeStreamDataParser:(AVStreamDataParser *)streamDataParser;
@end

#pragma mark -
#pragma mark AVStreamDataParser

@interface AVStreamDataParser : NSObject
- (void)setDelegate:(id)delegate;
- (void)appendStreamData:(NSData *)data;
- (void)setShouldProvideMediaData:(BOOL)shouldProvideMediaData forTrackID:(CMPersistentTrackID)trackID;
- (BOOL)shouldProvideMediaDataForTrackID:(CMPersistentTrackID)trackID;
- (void)providePendingMediaData;
- (void)processContentKeyResponseData:(NSData *)contentKeyResponseData forTrackID:(CMPersistentTrackID)trackID;
- (void)processContentKeyResponseError:(NSError *)error forTrackID:(CMPersistentTrackID)trackID;
- (void)renewExpiringContentKeyResponseDataForTrackID:(CMPersistentTrackID)trackID;
- (NSData *)streamingContentKeyRequestDataForApp:(NSData *)appIdentifier contentIdentifier:(NSData *)contentIdentifier trackID:(CMPersistentTrackID)trackID options:(NSDictionary *)options error:(NSError **)outError;
@end

#pragma mark -
#pragma mark AVSampleBufferDisplayLayer

@interface AVSampleBufferDisplayLayer : CALayer
- (NSInteger)status;
- (NSError*)error;
- (void)enqueueSampleBuffer:(CMSampleBufferRef)sampleBuffer;
- (void)flush;
- (void)flushAndRemoveImage;
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
    WeakPtr<WebCore::SourceBufferPrivateAVFObjC> _parent;
    AVStreamDataParser* _parser;
}
- (id)initWithParser:(AVStreamDataParser*)parser parent:(WeakPtr<WebCore::SourceBufferPrivateAVFObjC>)parent;
@end

@implementation WebAVStreamDataParserListener
- (id)initWithParser:(AVStreamDataParser*)parser parent:(WeakPtr<WebCore::SourceBufferPrivateAVFObjC>)parent
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

- (void)invalidate
{
    [_parser setDelegate:nil];
    _parser = nullptr;
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didParseStreamDataAsAsset:(AVAsset *)asset
{
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    RetainPtr<WebAVStreamDataParserListener> strongSelf = self;

    RetainPtr<AVAsset*> strongAsset = asset;
    callOnMainThread([strongSelf, strongAsset] {
        if (strongSelf->_parent)
            strongSelf->_parent->didParseStreamDataAsAsset(strongAsset.get());
    });
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didParseStreamDataAsAsset:(AVAsset *)asset withDiscontinuity:(BOOL)discontinuity
{
    UNUSED_PARAM(discontinuity);
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    RetainPtr<WebAVStreamDataParserListener> strongSelf = self;

    RetainPtr<AVAsset*> strongAsset = asset;
    callOnMainThread([strongSelf, strongAsset] {
        if (strongSelf->_parent)
            strongSelf->_parent->didParseStreamDataAsAsset(strongAsset.get());
    });
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didFailToParseStreamDataWithError:(NSError *)error
{
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    RetainPtr<WebAVStreamDataParserListener> strongSelf = self;

    RetainPtr<NSError> strongError = error;
    callOnMainThread([strongSelf, strongError] {
        if (strongSelf->_parent)
            strongSelf->_parent->didFailToParseStreamDataWithError(strongError.get());
    });
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didProvideMediaData:(CMSampleBufferRef)sample forTrackID:(CMPersistentTrackID)trackID mediaType:(NSString *)nsMediaType flags:(NSUInteger)flags
{
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    RetainPtr<WebAVStreamDataParserListener> strongSelf = self;

    RetainPtr<CMSampleBufferRef> strongSample = sample;
    String mediaType = nsMediaType;
    callOnMainThread([strongSelf, strongSample, trackID, mediaType, flags] {
        if (strongSelf->_parent)
            strongSelf->_parent->didProvideMediaDataForTrackID(trackID, strongSample.get(), mediaType, flags);
    });
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didReachEndOfTrackWithTrackID:(CMPersistentTrackID)trackID mediaType:(NSString *)nsMediaType
{
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    RetainPtr<WebAVStreamDataParserListener> strongSelf = self;

    String mediaType = nsMediaType;
    callOnMainThread([strongSelf, trackID, mediaType] {
        if (strongSelf->_parent)
            strongSelf->_parent->didReachEndOfTrackWithTrackID(trackID, mediaType);
    });
}

- (void)streamDataParserWillProvideContentKeyRequestInitializationData:(AVStreamDataParser *)streamDataParser forTrackID:(CMPersistentTrackID)trackID
{
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);

    if (isMainThread()) {
        _parent->willProvideContentKeyRequestInitializationDataForTrackID(trackID);
        return;
    }

    // We must call synchronously to the main thread, as the AVStreamSession must be associated
    // with the streamDataParser before the delegate method returns.
    RetainPtr<WebAVStreamDataParserListener> strongSelf = self;
    dispatch_sync(dispatch_get_main_queue(), [strongSelf, trackID]() {
        if (strongSelf->_parent)
            strongSelf->_parent->willProvideContentKeyRequestInitializationDataForTrackID(trackID);
    });
}

- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didProvideContentKeyRequestInitializationData:(NSData *)initData forTrackID:(CMPersistentTrackID)trackID
{
#if ASSERT_DISABLED
    UNUSED_PARAM(streamDataParser);
#endif
    ASSERT(streamDataParser == _parser);
    RetainPtr<WebAVStreamDataParserListener> strongSelf = self;

    RetainPtr<NSData> strongData = initData;
    callOnMainThread([strongSelf, strongData, trackID] {
        if (strongSelf->_parent)
            strongSelf->_parent->didProvideContentKeyRequestInitializationDataForTrackID(strongData.get(), trackID);
    });
}
@end

@interface WebAVSampleBufferErrorListener : NSObject {
    WebCore::SourceBufferPrivateAVFObjC* _parent;
    Vector<RetainPtr<AVSampleBufferDisplayLayer>> _layers;
    Vector<RetainPtr<AVSampleBufferAudioRenderer>> _renderers;
}

- (id)initWithParent:(WebCore::SourceBufferPrivateAVFObjC*)parent;
- (void)invalidate;
- (void)beginObservingLayer:(AVSampleBufferDisplayLayer *)layer;
- (void)stopObservingLayer:(AVSampleBufferDisplayLayer *)layer;
- (void)beginObservingRenderer:(AVSampleBufferAudioRenderer *)renderer;
- (void)stopObservingRenderer:(AVSampleBufferAudioRenderer *)renderer;
@end

@implementation WebAVSampleBufferErrorListener

- (id)initWithParent:(WebCore::SourceBufferPrivateAVFObjC*)parent
{
    if (!(self = [super init]))
        return nil;

    _parent = parent;
    return self;
}

- (void)dealloc
{
    [self invalidate];
    [super dealloc];
}

- (void)invalidate
{
    if (!_parent && !_layers.size() && !_renderers.size())
        return;

    for (auto& layer : _layers) {
        [layer removeObserver:self forKeyPath:@"error"];
        [layer removeObserver:self forKeyPath:@"outputObscuredDueToInsufficientExternalProtection"];
    }
    _layers.clear();

    for (auto& renderer : _renderers)
        [renderer removeObserver:self forKeyPath:@"error"];
    _renderers.clear();

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    _parent = nullptr;
}

- (void)beginObservingLayer:(AVSampleBufferDisplayLayer*)layer
{
    ASSERT(_parent);
    ASSERT(!_layers.contains(layer));

    _layers.append(layer);
    [layer addObserver:self forKeyPath:@"error" options:NSKeyValueObservingOptionNew context:nullptr];
    [layer addObserver:self forKeyPath:@"outputObscuredDueToInsufficientExternalProtection" options:NSKeyValueObservingOptionNew context:nullptr];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(layerFailedToDecode:) name:AVSampleBufferDisplayLayerFailedToDecodeNotification object:layer];
}

- (void)stopObservingLayer:(AVSampleBufferDisplayLayer*)layer
{
    ASSERT(_parent);
    ASSERT(_layers.contains(layer));

    [layer removeObserver:self forKeyPath:@"error"];
    [layer removeObserver:self forKeyPath:@"outputObscuredDueToInsufficientExternalProtection"];
    _layers.remove(_layers.find(layer));

    [[NSNotificationCenter defaultCenter] removeObserver:self name:AVSampleBufferDisplayLayerFailedToDecodeNotification object:layer];
}

- (void)beginObservingRenderer:(AVSampleBufferAudioRenderer*)renderer
{
    ASSERT(_parent);
    ASSERT(!_renderers.contains(renderer));

    _renderers.append(renderer);
    [renderer addObserver:self forKeyPath:@"error" options:NSKeyValueObservingOptionNew context:nullptr];
}

- (void)stopObservingRenderer:(AVSampleBufferAudioRenderer*)renderer
{
    ASSERT(_parent);
    ASSERT(_renderers.contains(renderer));

    [renderer removeObserver:self forKeyPath:@"error"];
    _renderers.remove(_renderers.find(renderer));
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(keyPath);
    ASSERT(_parent);

    RetainPtr<WebAVSampleBufferErrorListener> strongSelf = self;
    if ([object isKindOfClass:getAVSampleBufferDisplayLayerClass()]) {
        RetainPtr<AVSampleBufferDisplayLayer> layer = (AVSampleBufferDisplayLayer *)object;
        ASSERT(_layers.contains(layer.get()));

        if ([keyPath isEqualTo:@"error"]) {
            RetainPtr<NSError> error = [change valueForKey:NSKeyValueChangeNewKey];
            callOnMainThread([strongSelf, layer, error] {
                strongSelf->_parent->layerDidReceiveError(layer.get(), error.get());
            });
        } else if ([keyPath isEqualTo:@"outputObscuredDueToInsufficientExternalProtection"]) {
            if ([[change valueForKey:NSKeyValueChangeNewKey] boolValue]) {
                RetainPtr<NSError> error = [NSError errorWithDomain:@"com.apple.WebKit" code:'HDCP' userInfo:nil];
                callOnMainThread([strongSelf, layer, error] {
                    strongSelf->_parent->layerDidReceiveError(layer.get(), error.get());
                });
            }
        } else
            ASSERT_NOT_REACHED();

    } else if ([object isKindOfClass:getAVSampleBufferAudioRendererClass()]) {
        RetainPtr<AVSampleBufferAudioRenderer> renderer = (AVSampleBufferAudioRenderer *)object;
        RetainPtr<NSError> error = [change valueForKey:NSKeyValueChangeNewKey];

        ASSERT(_renderers.contains(renderer.get()));
        ASSERT([keyPath isEqualTo:@"error"]);

        callOnMainThread([strongSelf, renderer, error] {
            strongSelf->_parent->rendererDidReceiveError(renderer.get(), error.get());
        });
    } else
        ASSERT_NOT_REACHED();
}

- (void)layerFailedToDecode:(NSNotification*)note
{
    RetainPtr<AVSampleBufferDisplayLayer> layer = (AVSampleBufferDisplayLayer *)[note object];
    RetainPtr<NSError> error = [[note userInfo] valueForKey:AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey];

    RetainPtr<WebAVSampleBufferErrorListener> strongSelf = self;
    callOnMainThread([strongSelf, layer, error] {
        if (!strongSelf->_parent || !strongSelf->_layers.contains(layer.get()))
            return;
        strongSelf->_parent->layerDidReceiveError(layer.get(), error.get());
    });
}
@end

namespace WebCore {

#pragma mark -
#pragma mark MediaSampleAVFObjC

class MediaSampleAVFObjC final : public MediaSample {
public:
    static RefPtr<MediaSampleAVFObjC> create(CMSampleBufferRef sample, int trackID) { return adoptRef(new MediaSampleAVFObjC(sample, trackID)); }
    virtual ~MediaSampleAVFObjC() { }

private:
    MediaSampleAVFObjC(CMSampleBufferRef sample, int trackID)
        : m_sample(sample)
        , m_id(String::format("%d", trackID))
    {
    }

    virtual MediaTime presentationTime() const override { return toMediaTime(CMSampleBufferGetPresentationTimeStamp(m_sample.get())); }
    virtual MediaTime decodeTime() const override { return toMediaTime(CMSampleBufferGetDecodeTimeStamp(m_sample.get())); }
    virtual MediaTime duration() const override { return toMediaTime(CMSampleBufferGetDuration(m_sample.get())); }
    virtual AtomicString trackID() const override { return m_id; }
    virtual size_t sizeInBytes() const override { return CMSampleBufferGetTotalSampleSize(m_sample.get()); }
    virtual FloatSize presentationSize() const override;

    virtual SampleFlags flags() const override;
    virtual PlatformSample platformSample() override;
    virtual void dump(PrintStream&) const override;

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

FloatSize MediaSampleAVFObjC::presentationSize() const
{
    CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(m_sample.get());
    if (CMFormatDescriptionGetMediaType(formatDescription) != kCMMediaType_Video)
        return FloatSize();

    return FloatSize(CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true)); 
}

void MediaSampleAVFObjC::dump(PrintStream& out) const
{
    out.print("{PTS(", presentationTime(), "), DTS(", decodeTime(), "), duration(", duration(), "), flags(", (int)flags(), "), presentationSize(", presentationSize(), ")}");
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
    : m_weakFactory(this)
    , m_parser(adoptNS([[getAVStreamDataParserClass() alloc] init]))
    , m_delegate(adoptNS([[WebAVStreamDataParserListener alloc] initWithParser:m_parser.get() parent:createWeakPtr()]))
    , m_errorListener(adoptNS([[WebAVSampleBufferErrorListener alloc] initWithParent:this]))
    , m_mediaSource(parent)
    , m_client(0)
    , m_parsingSucceeded(true)
    , m_enabledVideoTrackID(-1)
    , m_protectedTrackID(-1)
{
}

SourceBufferPrivateAVFObjC::~SourceBufferPrivateAVFObjC()
{
    ASSERT(!m_client);
    destroyParser();
    destroyRenderers();
}

void SourceBufferPrivateAVFObjC::didParseStreamDataAsAsset(AVAsset* asset)
{
    LOG(MediaSource, "SourceBufferPrivateAVFObjC::didParseStreamDataAsAsset(%p)", this);

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

    if (m_mediaSource)
        m_mediaSource->player()->characteristicsChanged();

    if (m_client)
        m_client->sourceBufferPrivateDidReceiveInitializationSegment(this, segment);
}

void SourceBufferPrivateAVFObjC::didFailToParseStreamDataWithError(NSError* error)
{
#if LOG_DISABLED
    UNUSED_PARAM(error);
#endif
    LOG(MediaSource, "SourceBufferPrivateAVFObjC::didFailToParseStreamDataWithError(%p) - error:\"%s\"", this, String([error description]).utf8().data());

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
    if (trackID == m_enabledVideoTrackID) {
        CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer);
        FloatSize formatSize = FloatSize(CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true));
        if (formatSize != m_cachedSize) {
            LOG(MediaSource, "SourceBufferPrivateAVFObjC::processCodedFrame(%p) - size change detected: {width=%lf, height=%lf}", formatSize.width(), formatSize.height());
            m_cachedSize = formatSize;
            if (m_mediaSource)
                m_mediaSource->player()->sizeChanged();
        }
    }


    if (m_client) {
        RefPtr<MediaSample> mediaSample = MediaSampleAVFObjC::create(sampleBuffer, trackID);
        LOG(MediaSourceSamples, "SourceBufferPrivateAVFObjC::processCodedFrame(%p) - sample(%s)", this, toString(*mediaSample).utf8().data());
        m_client->sourceBufferPrivateDidReceiveSample(this, mediaSample.release());
    }

    return true;
}

void SourceBufferPrivateAVFObjC::didReachEndOfTrackWithTrackID(int trackID, const String& mediaType)
{
    UNUSED_PARAM(mediaType);
    UNUSED_PARAM(trackID);
    notImplemented();
}

void SourceBufferPrivateAVFObjC::willProvideContentKeyRequestInitializationDataForTrackID(int trackID)
{
    if (!m_mediaSource)
        return;

    ASSERT(m_parser);

#if ENABLE(ENCRYPTED_MEDIA_V2)
    LOG(MediaSource, "SourceBufferPrivateAVFObjC::willProvideContentKeyRequestInitializationDataForTrackID(%p) - track:%d", this, trackID);
    m_protectedTrackID = trackID;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_mediaSource->player()->streamSession() addStreamDataParser:m_parser.get()];
    END_BLOCK_OBJC_EXCEPTIONS;
#else
    UNUSED_PARAM(trackID);
#endif
}

void SourceBufferPrivateAVFObjC::didProvideContentKeyRequestInitializationDataForTrackID(NSData* initData, int trackID)
{
    if (!m_mediaSource)
        return;

    UNUSED_PARAM(trackID);
#if ENABLE(ENCRYPTED_MEDIA_V2)
    LOG(MediaSource, "SourceBufferPrivateAVFObjC::didProvideContentKeyRequestInitializationDataForTrackID(%p) - track:%d", this, trackID);
    m_protectedTrackID = trackID;
    RefPtr<Uint8Array> initDataArray = Uint8Array::create([initData length]);
    [initData getBytes:initDataArray->data() length:initDataArray->length()];
    m_mediaSource->sourceBufferKeyNeeded(this, initDataArray.get());
#else
    UNUSED_PARAM(initData);
#endif
}

void SourceBufferPrivateAVFObjC::setClient(SourceBufferPrivateClient* client)
{
    m_client = client;
}

static dispatch_queue_t globalDataParserQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create("SourceBufferPrivateAVFObjC data parser queue", DISPATCH_QUEUE_CONCURRENT);
    });
    return globalQueue;
}

void SourceBufferPrivateAVFObjC::append(const unsigned char* data, unsigned length)
{
    LOG(MediaSource, "SourceBufferPrivateAVFObjC::append(%p) - data:%p, length:%d", this, data, length);

    RetainPtr<NSData> nsData = adoptNS([[NSData alloc] initWithBytes:data length:length]);
    WeakPtr<SourceBufferPrivateAVFObjC> weakThis = createWeakPtr();
    RetainPtr<AVStreamDataParser> parser = m_parser;
    RetainPtr<WebAVStreamDataParserListener> delegate = m_delegate;

    m_parsingSucceeded = true;

    dispatch_async(globalDataParserQueue(), [nsData, weakThis, parser, delegate] {

        [parser appendStreamData:nsData.get()];

        callOnMainThread([weakThis] {
            if (weakThis)
                weakThis->appendCompleted();
        });
    });
}

void SourceBufferPrivateAVFObjC::appendCompleted()
{
    if (m_parsingSucceeded && m_mediaSource)
        m_mediaSource->player()->setLoadingProgresssed(true);

    if (m_client)
        m_client->sourceBufferPrivateAppendComplete(this, m_parsingSucceeded ? SourceBufferPrivateClient::AppendSucceeded : SourceBufferPrivateClient::ParsingFailed);
}

void SourceBufferPrivateAVFObjC::abort()
{
    // The parser does not have a mechanism for resetting to a clean state, so destroy and re-create it.
    // FIXME(135164): Support resetting parser to the last appended initialization segment.
    destroyParser();

    m_parser = adoptNS([[getAVStreamDataParserClass() alloc] init]);
    m_delegate = adoptNS([[WebAVStreamDataParserListener alloc] initWithParser:m_parser.get() parent:createWeakPtr()]);
}

void SourceBufferPrivateAVFObjC::destroyParser()
{
    if (m_mediaSource && m_mediaSource->player()->hasStreamSession())
        [m_mediaSource->player()->streamSession() removeStreamDataParser:m_parser.get()];

    [m_delegate invalidate];
    m_delegate = nullptr;
    m_parser = nullptr;
}

void SourceBufferPrivateAVFObjC::destroyRenderers()
{
    if (m_displayLayer) {
        if (m_mediaSource)
            m_mediaSource->player()->removeDisplayLayer(m_displayLayer.get());
        [m_displayLayer flush];
        [m_displayLayer stopRequestingMediaData];
        [m_errorListener stopObservingLayer:m_displayLayer.get()];
        m_displayLayer = nullptr;
    }

    for (auto& renderer : m_audioRenderers.values()) {
        if (m_mediaSource)
            m_mediaSource->player()->removeAudioRenderer(renderer.get());
        [renderer flush];
        [renderer stopRequestingMediaData];
        [m_errorListener stopObservingRenderer:renderer.get()];
    }

    m_audioRenderers.clear();
}

void SourceBufferPrivateAVFObjC::removedFromMediaSource()
{
    destroyParser();
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
            m_displayLayer = adoptNS([[getAVSampleBufferDisplayLayerClass() alloc] init]);
#ifndef NDEBUG
            [m_displayLayer setName:@"SourceBufferPrivateAVFObjC AVSampleBufferDisplayLayer"];
#endif
            [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
                didBecomeReadyForMoreSamples(trackID);
            }];
            [m_errorListener beginObservingLayer:m_displayLayer.get()];
        }
        if (m_mediaSource)
            m_mediaSource->player()->addDisplayLayer(m_displayLayer.get());
    }
}

void SourceBufferPrivateAVFObjC::trackDidChangeEnabled(AudioTrackPrivateMediaSourceAVFObjC* track)
{
    int trackID = track->trackID();

    if (!track->enabled()) {
        RetainPtr<AVSampleBufferAudioRenderer> renderer = m_audioRenderers.get(trackID);
        [m_parser setShouldProvideMediaData:NO forTrackID:trackID];
        if (m_mediaSource)
            m_mediaSource->player()->removeAudioRenderer(renderer.get());
    } else {
        [m_parser setShouldProvideMediaData:YES forTrackID:trackID];
        RetainPtr<AVSampleBufferAudioRenderer> renderer;
        if (!m_audioRenderers.contains(trackID)) {
            renderer = adoptNS([[getAVSampleBufferAudioRendererClass() alloc] init]);
            [renderer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
                didBecomeReadyForMoreSamples(trackID);
            }];
            m_audioRenderers.set(trackID, renderer);
            [m_errorListener beginObservingRenderer:renderer.get()];
        } else
            renderer = m_audioRenderers.get(trackID);

        if (m_mediaSource)
            m_mediaSource->player()->addAudioRenderer(renderer.get());
    }
}

void SourceBufferPrivateAVFObjC::flush()
{
    if (m_displayLayer)
        [m_displayLayer flushAndRemoveImage];

    for (auto& renderer : m_audioRenderers.values())
        [renderer flush];
}

void SourceBufferPrivateAVFObjC::registerForErrorNotifications(SourceBufferPrivateAVFObjCErrorClient* client)
{
    ASSERT(!m_errorClients.contains(client));
    m_errorClients.append(client);
}

void SourceBufferPrivateAVFObjC::unregisterForErrorNotifications(SourceBufferPrivateAVFObjCErrorClient* client)
{
    ASSERT(m_errorClients.contains(client));
    m_errorClients.remove(m_errorClients.find(client));
}

void SourceBufferPrivateAVFObjC::layerDidReceiveError(AVSampleBufferDisplayLayer *layer, NSError *error)
{
    LOG(MediaSource, "SourceBufferPrivateAVFObjC::layerDidReceiveError(%p): layer(%p), error(%@)", this, layer, [error description]);

    // FIXME(142246): Remove the following once <rdar://problem/20027434> is resolved.
    bool anyIgnored = false;
    for (auto& client : m_errorClients) {
        bool shouldIgnore = false;
        client->layerDidReceiveError(layer, error, shouldIgnore);
        anyIgnored |= shouldIgnore;
    }
    if (anyIgnored)
        return;

    int errorCode = [[[error userInfo] valueForKey:@"OSStatus"] intValue];

    if (m_client)
        m_client->sourceBufferPrivateDidReceiveRenderingError(this, errorCode);
}

void SourceBufferPrivateAVFObjC::rendererDidReceiveError(AVSampleBufferAudioRenderer *renderer, NSError *error)
{
    LOG(MediaSource, "SourceBufferPrivateAVFObjC::rendererDidReceiveError(%p): renderer(%p), error(%@)", this, renderer, [error description]);

    // FIXME(142246): Remove the following once <rdar://problem/20027434> is resolved.
    bool anyIgnored = false;
    for (auto& client : m_errorClients) {
        bool shouldIgnore = false;
        client->rendererDidReceiveError(renderer, error, shouldIgnore);
        anyIgnored |= shouldIgnore;
    }
    if (anyIgnored)
        return;
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
    LOG(MediaSource, "SourceBufferPrivateAVFObjC::flushAndEnqueueNonDisplayingSamples(%p) samples: %d samples, trackId: %d", this, mediaSamples.size(), trackID);

    if (trackID == m_enabledVideoTrackID)
        flushAndEnqueueNonDisplayingSamples(mediaSamples, m_displayLayer.get());
    else if (m_audioRenderers.contains(trackID))
        flushAndEnqueueNonDisplayingSamples(mediaSamples, m_audioRenderers.get(trackID).get());
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

        LOG(MediaSourceSamples, "SourceBufferPrivateAVFObjC::flushAndEnqueueNonDisplayingSamples(%p) - sample(%s)", this, toString(*mediaSample).utf8().data());

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
    if (trackID != m_enabledVideoTrackID && !m_audioRenderers.contains(trackID))
        return;

    RefPtr<MediaSample> mediaSample = prpMediaSample;

    PlatformSample platformSample = mediaSample->platformSample();
    if (platformSample.type != PlatformSample::CMSampleBufferType)
        return;

    LOG(MediaSourceSamples, "SourceBufferPrivateAVFObjC::enqueueSample(%p) - sample(%s)", this, toString(*mediaSample).utf8().data());

    if (trackID == m_enabledVideoTrackID) {
        [m_displayLayer enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
        if (m_mediaSource)
            m_mediaSource->player()->setHasAvailableVideoFrame(true);
    } else
        [m_audioRenderers.get(trackID) enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
}

bool SourceBufferPrivateAVFObjC::isReadyForMoreSamples(AtomicString trackIDString)
{
    int trackID = trackIDString.toInt();
    if (trackID == m_enabledVideoTrackID)
        return [m_displayLayer isReadyForMoreMediaData];
    else if (m_audioRenderers.contains(trackID))
        return [m_audioRenderers.get(trackID) isReadyForMoreMediaData];
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

FloatSize SourceBufferPrivateAVFObjC::naturalSize()
{
    return m_cachedSize;
}

void SourceBufferPrivateAVFObjC::didBecomeReadyForMoreSamples(int trackID)
{
    if (trackID == m_enabledVideoTrackID)
        [m_displayLayer stopRequestingMediaData];
    else if (m_audioRenderers.contains(trackID))
        [m_audioRenderers.get(trackID) stopRequestingMediaData];
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
    } else if (m_audioRenderers.contains(trackID)) {
        [m_audioRenderers.get(trackID) requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
            didBecomeReadyForMoreSamples(trackID);
        }];
    } else
        ASSERT_NOT_REACHED();
}

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
