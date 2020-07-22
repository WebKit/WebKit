/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

#import "AVAssetTrackUtilities.h"
#import "AudioTrackPrivateMediaSourceAVFObjC.h"
#import "CDMFairPlayStreaming.h"
#import "CDMInstanceFairPlayStreamingAVFObjC.h"
#import "CDMSessionAVContentKeySession.h"
#import "CDMSessionMediaSourceAVFObjC.h"
#import "FourCC.h"
#import "InbandTextTrackPrivateAVFObjC.h"
#import "Logging.h"
#import "MediaDescription.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"
#import "MediaSample.h"
#import "MediaSampleAVFObjC.h"
#import "MediaSourcePrivateAVFObjC.h"
#import "NotImplemented.h"
#import "SharedBuffer.h"
#import "SourceBufferParserAVFObjC.h"
#import "SourceBufferPrivateClient.h"
#import "TimeRanges.h"
#import "VideoTrackPrivateMediaSourceAVFObjC.h"
#import "WebCoreDecompressionSession.h"
#import <AVFoundation/AVAssetTrack.h>
#import <JavaScriptCore/TypedArrayInlines.h>
#import <QuartzCore/CALayer.h>
#import <objc/runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/HashCountedSet.h>
#import <wtf/MainThread.h>
#import <wtf/SoftLinking.h>
#import <wtf/WTFSemaphore.h>
#import <wtf/WeakPtr.h>
#import <wtf/text/AtomString.h>
#import <wtf/text/CString.h>

#pragma mark - Soft Linking

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (WebCoreAVSampleBufferDisplayLayerQueueManagementPrivate)
- (void)prerollDecodeWithCompletionHandler:(void (^)(BOOL success))block;
- (void)expectMinimumUpcomingSampleBufferPresentationTime: (CMTime)minimumUpcomingPresentationTime;
- (void)resetUpcomingSampleBufferPresentationTimeExpectations;
@end

#pragma mark -
#pragma mark AVStreamSession

@interface AVStreamSession : NSObject
- (void)addStreamDataParser:(AVStreamDataParser *)streamDataParser;
- (void)removeStreamDataParser:(AVStreamDataParser *)streamDataParser;
@end

@interface WebAVSampleBufferErrorListener : NSObject {
    WeakPtr<WebCore::SourceBufferPrivateAVFObjC> _parent;
    Vector<RetainPtr<AVSampleBufferDisplayLayer>> _layers;
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    Vector<RetainPtr<AVSampleBufferAudioRenderer>> _renderers;
    ALLOW_NEW_API_WITHOUT_GUARDS_END
}

- (id)initWithParent:(WeakPtr<WebCore::SourceBufferPrivateAVFObjC>&&)parent;
- (void)invalidate;
- (void)beginObservingLayer:(AVSampleBufferDisplayLayer *)layer;
- (void)stopObservingLayer:(AVSampleBufferDisplayLayer *)layer;
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
- (void)beginObservingRenderer:(AVSampleBufferAudioRenderer *)renderer;
- (void)stopObservingRenderer:(AVSampleBufferAudioRenderer *)renderer;
ALLOW_NEW_API_WITHOUT_GUARDS_END
@end

@implementation WebAVSampleBufferErrorListener

- (id)initWithParent:(WeakPtr<WebCore::SourceBufferPrivateAVFObjC>&&)parent
{
    if (!(self = [super init]))
        return nil;

    _parent = WTFMove(parent);
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

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
- (void)beginObservingRenderer:(AVSampleBufferAudioRenderer*)renderer
{
ALLOW_NEW_API_WITHOUT_GUARDS_END
    ASSERT(_parent);
    ASSERT(!_renderers.contains(renderer));

    _renderers.append(renderer);
    [renderer addObserver:self forKeyPath:@"error" options:NSKeyValueObservingOptionNew context:nullptr];
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
- (void)stopObservingRenderer:(AVSampleBufferAudioRenderer*)renderer
ALLOW_NEW_API_WITHOUT_GUARDS_END
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

    if ([object isKindOfClass:PAL::getAVSampleBufferDisplayLayerClass()]) {
        RetainPtr<AVSampleBufferDisplayLayer> layer = (AVSampleBufferDisplayLayer *)object;
        ASSERT(_layers.contains(layer.get()));

        if ([keyPath isEqualToString:@"error"]) {
            RetainPtr<NSError> error = [change valueForKey:NSKeyValueChangeNewKey];
            if ([error isKindOfClass:[NSNull class]])
                return;

            callOnMainThread([parent = _parent, layer = WTFMove(layer), error = WTFMove(error)] {
                if (parent)
                    parent->layerDidReceiveError(layer.get(), error.get());
            });
        } else if ([keyPath isEqualToString:@"outputObscuredDueToInsufficientExternalProtection"]) {
            callOnMainThread([parent = _parent, obscured = [[change valueForKey:NSKeyValueChangeNewKey] boolValue]] {
                if (parent)
                    parent->outputObscuredDueToInsufficientExternalProtectionChanged(obscured);
            });
        } else
            ASSERT_NOT_REACHED();

    } else if ([object isKindOfClass:PAL::getAVSampleBufferAudioRendererClass()]) {
        ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
        RetainPtr<AVSampleBufferAudioRenderer> renderer = (AVSampleBufferAudioRenderer *)object;
        ALLOW_NEW_API_WITHOUT_GUARDS_END
        RetainPtr<NSError> error = [change valueForKey:NSKeyValueChangeNewKey];
        if ([error isKindOfClass:[NSNull class]])
            return;

        ASSERT(_renderers.contains(renderer.get()));
        ASSERT([keyPath isEqualToString:@"error"]);

        callOnMainThread([parent = _parent, renderer = WTFMove(renderer), error = WTFMove(error)] {
            if (parent)
                parent->rendererDidReceiveError(renderer.get(), error.get());
        });
    } else
        ASSERT_NOT_REACHED();
}

- (void)layerFailedToDecode:(NSNotification*)note
{
    RetainPtr<AVSampleBufferDisplayLayer> layer = (AVSampleBufferDisplayLayer *)[note object];
    if (!_layers.contains(layer.get()))
        return;

    callOnMainThread([parent = _parent, layer = WTFMove(layer), error = retainPtr([[note userInfo] valueForKey:AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey])] {
        if (parent)
            parent->layerDidReceiveError(layer.get(), error.get());
    });
}
@end

namespace WebCore {
using namespace PAL;

#pragma mark -
#pragma mark SourceBufferPrivateAVFObjC

static HashMap<uint64_t, WeakPtr<SourceBufferPrivateAVFObjC>>& sourceBufferMap()
{
    static NeverDestroyed<HashMap<uint64_t, WeakPtr<SourceBufferPrivateAVFObjC>>> map;
    return map;
}

static uint64_t nextMapID()
{
    static uint64_t mapID = 0;
    return ++mapID;
}

static void bufferWasConsumedCallback(CMNotificationCenterRef, const void* listener, CFStringRef notificationName, const void*, CFTypeRef)
{
    if (!CFEqual(kCMSampleBufferConsumerNotification_BufferConsumed, notificationName))
        return;

    if (!isMainThread()) {
        callOnMainThread([notificationName, listener] {
            bufferWasConsumedCallback(nullptr, listener, notificationName, nullptr, nullptr);
        });
        return;
    }

    uint64_t mapID = reinterpret_cast<uint64_t>(listener);
    if (!mapID) {
        RELEASE_LOG(MediaSource, "bufferWasConsumedCallback - ERROR: didn't find ID %llu in map", mapID);
        return;
    }

    if (auto sourceBuffer = sourceBufferMap().get(mapID).get())
        sourceBuffer->bufferWasConsumed();
}

Ref<SourceBufferPrivateAVFObjC> SourceBufferPrivateAVFObjC::create(MediaSourcePrivateAVFObjC* parent, Ref<SourceBufferParser>&& parser)
{
    return adoptRef(*new SourceBufferPrivateAVFObjC(parent, WTFMove(parser)));
}

SourceBufferPrivateAVFObjC::SourceBufferPrivateAVFObjC(MediaSourcePrivateAVFObjC* parent, Ref<SourceBufferParser>&& parser)
    : m_parser(WTFMove(parser))
    , m_errorListener(adoptNS([[WebAVSampleBufferErrorListener alloc] initWithParent:makeWeakPtr(*this)]))
    , m_isAppendingGroup(adoptOSObject(dispatch_group_create()))
    , m_mediaSource(parent)
    , m_mapID(nextMapID())
#if !RELEASE_LOG_DISABLED
    , m_logger(parent->logger())
    , m_logIdentifier(parent->nextSourceBufferLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(prerollDecodeWithCompletionHandler:)])
        CMNotificationCenterAddListener(CMNotificationCenterGetDefaultLocalCenter(), reinterpret_cast<void*>(m_mapID), bufferWasConsumedCallback, kCMSampleBufferConsumerNotification_BufferConsumed, nullptr, 0);

    sourceBufferMap().add(m_mapID, makeWeakPtr(*this));

    m_parser->setDidParseInitializationDataCallback([weakThis = makeWeakPtr(this)] (InitializationSegment&& segment) mutable {
        ASSERT(isMainThread());
        if (weakThis)
            weakThis->didParseInitializationData(WTFMove(segment));
    });

    m_parser->setDidEncounterErrorDuringParsingCallback([weakThis = makeWeakPtr(this)] (int32_t errorCode) mutable {
        ASSERT(isMainThread());
        if (weakThis)
            weakThis->didEncounterErrorDuringParsing(errorCode);
    });

    m_parser->setDidProvideMediaDataCallback([weakThis = makeWeakPtr(this)] (Ref<MediaSample>&& sample, uint64_t trackID, const String& mediaType) mutable {
        ASSERT(isMainThread());
        if (weakThis)
            weakThis->didProvideMediaDataForTrackID(WTFMove(sample), trackID, mediaType);
    });
}

SourceBufferPrivateAVFObjC::~SourceBufferPrivateAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(!m_client);
    sourceBufferMap().remove(m_mapID);
    destroyParser();
    destroyRenderers();
    clearTracks();

    if (![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(prerollDecodeWithCompletionHandler:)])
        CMNotificationCenterRemoveListener(CMNotificationCenterGetDefaultLocalCenter(), this, bufferWasConsumedCallback, kCMSampleBufferConsumerNotification_BufferConsumed, nullptr);

    if (m_hasSessionSemaphore)
        m_hasSessionSemaphore->signal();
}

void SourceBufferPrivateAVFObjC::didParseInitializationData(InitializationSegment&& segment)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!m_mediaSource)
        return;

    if (m_mediaSource->player()->shouldCheckHardwareSupport()) {
        for (auto& info : segment.videoTracks) {
            auto codec = FourCC::fromString(info.description->codec());
            if (!codec)
                continue;
            if (!codecsMeetHardwareDecodeRequirements({{ *codec }}, m_mediaSource->player()->mediaContentTypesRequiringHardwareSupport())) {
                m_parsingSucceeded = false;
                return;
            }
        }
    }

    clearTracks();

    for (auto videoTrackInfo : segment.videoTracks) {
        videoTrackInfo.track->setSelectedChangedCallback([weakThis = makeWeakPtr(this)] (VideoTrackPrivate& track, bool selected) {
            if (weakThis)
                weakThis->trackDidChangeSelected(track, selected);
        });
        m_videoTracks.append(videoTrackInfo.track);
    }

    for (auto audioTrackInfo : segment.audioTracks) {
        audioTrackInfo.track->setEnabledChangedCallback([weakThis = makeWeakPtr(this)] (AudioTrackPrivate& track, bool enabled) {
            if (weakThis)
                weakThis->trackDidChangeEnabled(track, enabled);
        });
        m_audioTracks.append(audioTrackInfo.track);
    }

    if (m_mediaSource)
        m_mediaSource->player()->characteristicsChanged();

    if (m_client)
        m_client->sourceBufferPrivateDidReceiveInitializationSegment(segment);
}

void SourceBufferPrivateAVFObjC::didEncounterErrorDuringParsing(int32_t code)
{
#if LOG_DISABLED
    UNUSED_PARAM(code);
#endif
    ERROR_LOG(LOGIDENTIFIER, code);

    m_parsingSucceeded = false;
}

void SourceBufferPrivateAVFObjC::didProvideMediaDataForTrackID(Ref<MediaSample>&& mediaSample, uint64_t trackID, const String& mediaType)
{
    UNUSED_PARAM(mediaType);

    if (trackID != m_enabledVideoTrackID && !m_audioRenderers.contains(trackID)) {
        // FIXME(125161): We don't handle text tracks, and passing this sample up to SourceBuffer
        // will just confuse its state. Drop this sample until we can handle text tracks properly.
        return;
    }

    DEBUG_LOG(LOGIDENTIFIER, mediaSample->toJSONString());
    if (m_client)
        m_client->sourceBufferPrivateDidReceiveSample(mediaSample);
}

void SourceBufferPrivateAVFObjC::willProvideContentKeyRequestInitializationDataForTrackID(uint64_t trackID)
{
    if (!m_mediaSource)
        return;

#if HAVE(AVSTREAMSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    ALWAYS_LOG(LOGIDENTIFIER, "track = ", trackID);

    m_protectedTrackID = trackID;

    auto parser = this->parser();
    if (!parser)
        return;

    if (CDMSessionMediaSourceAVFObjC* session = m_mediaSource->player()->cdmSession())
        session->addParser(parser);
    else if (!CDMSessionAVContentKeySession::isAvailable()) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [m_mediaSource->player()->streamSession() addStreamDataParser:parser];
        END_BLOCK_OBJC_EXCEPTIONS
    }
#else
    UNUSED_PARAM(trackID);
#endif
}

void SourceBufferPrivateAVFObjC::didProvideContentKeyRequestInitializationDataForTrackID(Ref<Uint8Array>&& initData, uint64_t trackID, Box<BinarySemaphore> hasSessionSemaphore)
{
    if (!m_mediaSource)
        return;

#if HAVE(AVCONTENTKEYSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    ALWAYS_LOG(LOGIDENTIFIER, "track = ", trackID);

    m_protectedTrackID = trackID;
    m_initData = WTFMove(initData);
    m_mediaSource->sourceBufferKeyNeeded(this, m_initData.get());
    if (auto session = m_mediaSource->player()->cdmSession()) {
        if (auto parser = this->parser())
            session->addParser(parser);
        hasSessionSemaphore->signal();
        return;
    }
#endif

    if (m_hasSessionSemaphore)
        m_hasSessionSemaphore->signal();
    m_hasSessionSemaphore = hasSessionSemaphore;
    
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    auto initDataBuffer = SharedBuffer::create(m_initData->data(), m_initData->byteLength());
    auto keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsSinf(initDataBuffer);
    if (!keyIDs)
        return;

    if (m_cdmInstance) {
        if (auto instanceSession = m_cdmInstance->sessionForKeyIDs(keyIDs.value())) {
            if (auto parser = this->parser())
                [instanceSession->contentKeySession() addContentKeyRecipient:parser];
            if (m_hasSessionSemaphore) {
                m_hasSessionSemaphore->signal();
                m_hasSessionSemaphore = nullptr;
            }
            m_waitingForKey = false;
            return;
        }
    }

    m_keyIDs = WTFMove(keyIDs.value());
    m_mediaSource->player()->initializationDataEncountered("sinf", initDataBuffer->tryCreateArrayBuffer());

    m_waitingForKey = true;
    m_mediaSource->player()->waitingForKeyChanged();
#endif

    UNUSED_PARAM(initData);
    UNUSED_PARAM(trackID);
    UNUSED_PARAM(hasSessionSemaphore);
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

void SourceBufferPrivateAVFObjC::append(Vector<unsigned char>&& data)
{
    DEBUG_LOG(LOGIDENTIFIER, "data length = ", data.size());

    ASSERT(!m_hasSessionSemaphore);
    ASSERT(!m_abortSemaphore);

    m_abortSemaphore = Box<Semaphore>::create(0);
    m_parser->setWillProvideContentKeyRequestInitializationDataForTrackIDCallback([weakThis = makeWeakPtr(this), abortSemaphore = m_abortSemaphore] (uint64_t trackID) mutable {
        // We must call synchronously to the main thread, as the AVStreamSession must be associated
        // with the streamDataParser before the delegate method returns.
        Box<BinarySemaphore> respondedSemaphore = Box<BinarySemaphore>::create();
        callOnMainThread([weakThis = WTFMove(weakThis), trackID, respondedSemaphore]() {
            if (weakThis)
                weakThis->willProvideContentKeyRequestInitializationDataForTrackID(trackID);
            respondedSemaphore->signal();
        });

        while (true) {
            if (respondedSemaphore->waitFor(100_ms))
                return;

            if (abortSemaphore->waitFor(100_ms)) {
                abortSemaphore->signal();
                return;
            }
        }
    });

    m_parser->setDidProvideContentKeyRequestInitializationDataForTrackIDCallback([weakThis = makeWeakPtr(this), abortSemaphore = m_abortSemaphore] (Ref<Uint8Array>&& initData, uint64_t trackID) mutable {
        Box<BinarySemaphore> hasSessionSemaphore = Box<BinarySemaphore>::create();
        callOnMainThread([weakThis = WTFMove(weakThis), initData = WTFMove(initData), trackID, hasSessionSemaphore] () mutable {
            if (weakThis)
                weakThis->didProvideContentKeyRequestInitializationDataForTrackID(WTFMove(initData), trackID, hasSessionSemaphore);
        });

        while (true) {
            if (hasSessionSemaphore->waitFor(100_ms))
                return;

            if (abortSemaphore->waitFor(100_ms)) {
                abortSemaphore->signal();
                return;
            }
        }
    });

    m_parsingSucceeded = true;
    dispatch_group_enter(m_isAppendingGroup.get());

    dispatch_async(globalDataParserQueue(), [data = WTFMove(data), weakThis = m_appendWeakFactory.createWeakPtr(*this), parser = m_parser, isAppendingGroup = m_isAppendingGroup] () mutable {
        parser->appendData(WTFMove(data));

        callOnMainThread([weakThis] {
            if (weakThis)
                weakThis->appendCompleted();
        });
        dispatch_group_leave(isAppendingGroup.get());
    });
}

void SourceBufferPrivateAVFObjC::appendCompleted()
{
    if (m_abortSemaphore) {
        m_abortSemaphore->signal();
        m_abortSemaphore = nil;
    }

    if (m_hasSessionSemaphore) {
        m_hasSessionSemaphore->signal();
        m_hasSessionSemaphore = nil;
    }

    if (m_parsingSucceeded && m_mediaSource)
        m_mediaSource->player()->setLoadingProgresssed(true);

    if (m_client)
        m_client->sourceBufferPrivateAppendComplete(m_parsingSucceeded ? SourceBufferPrivateClient::AppendSucceeded : SourceBufferPrivateClient::ParsingFailed);
}

void SourceBufferPrivateAVFObjC::abort()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    // The parsing queue may be blocked waiting for the main thread to provide it a AVStreamSession. We
    // were asked to abort, and that cancels all outstanding append operations. Without cancelling this
    // semaphore, the m_isAppendingGroup wait operation will deadlock.
    if (m_hasSessionSemaphore) {
        m_hasSessionSemaphore->signal();
        m_hasSessionSemaphore = nullptr;
    }
    if (m_abortSemaphore) {
        m_abortSemaphore->signal();
        m_abortSemaphore = nullptr;
    }
    dispatch_group_wait(m_isAppendingGroup.get(), DISPATCH_TIME_FOREVER);
}

void SourceBufferPrivateAVFObjC::resetParserState()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_parser->resetParserState();
}

void SourceBufferPrivateAVFObjC::destroyParser()
{
    auto parser = this->parser();
    if (!parser)
        return;
#if HAVE(AVSTREAMSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (m_mediaSource && m_mediaSource->player()->hasStreamSession())
        [m_mediaSource->player()->streamSession() removeStreamDataParser:parser];
#endif
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (m_cdmInstance) {
        if (auto instanceSession = m_cdmInstance->sessionForKeyIDs(m_keyIDs))
            [instanceSession->contentKeySession() removeContentKeyRecipient:parser];
    }
#endif
}

void SourceBufferPrivateAVFObjC::destroyRenderers()
{
    if (m_displayLayer)
        setVideoLayer(nullptr);

    if (m_decompressionSession)
        setDecompressionSession(nullptr);

    for (auto& renderer : m_audioRenderers.values()) {
        if (m_mediaSource)
            m_mediaSource->player()->removeAudioRenderer(renderer.get());
        [renderer flush];
        [renderer stopRequestingMediaData];
        [m_errorListener stopObservingRenderer:renderer.get()];
    }

    [m_errorListener invalidate];
    m_errorListener = nullptr;

    m_audioRenderers.clear();
}

void SourceBufferPrivateAVFObjC::clearTracks()
{
    for (auto& track : m_videoTracks)
        track->setSelectedChangedCallback(nullptr);
    m_videoTracks.clear();

    for (auto& track : m_audioTracks)
        track->setEnabledChangedCallback(nullptr);
    m_audioTracks.clear();
}

void SourceBufferPrivateAVFObjC::removedFromMediaSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    destroyParser();
    destroyRenderers();

    if (m_mediaSource)
        m_mediaSource->removeSourceBuffer(this);
}

MediaPlayer::ReadyState SourceBufferPrivateAVFObjC::readyState() const
{
    return m_mediaSource ? m_mediaSource->player()->readyState() : MediaPlayer::ReadyState::HaveNothing;
}

void SourceBufferPrivateAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    ALWAYS_LOG(LOGIDENTIFIER, readyState);

    if (m_mediaSource)
        m_mediaSource->player()->setReadyState(readyState);
}

bool SourceBufferPrivateAVFObjC::hasVideo() const
{
    return m_client && m_client->sourceBufferPrivateHasVideo();
}

bool SourceBufferPrivateAVFObjC::hasSelectedVideo() const
{
    return m_enabledVideoTrackID != notFound;
}

bool SourceBufferPrivateAVFObjC::hasAudio() const
{
    return m_client && m_client->sourceBufferPrivateHasAudio();
}

void SourceBufferPrivateAVFObjC::trackDidChangeSelected(VideoTrackPrivate& track, bool selected)
{
    auto trackID = track.id().string().toUInt64();

    ALWAYS_LOG(LOGIDENTIFIER, "video trackID = ", trackID, ", selected = ", selected);

    if (!selected && m_enabledVideoTrackID == trackID) {
        m_enabledVideoTrackID = -1;
        m_parser->setShouldProvideMediaDataForTrackID(false, trackID);

        if (m_decompressionSession)
            m_decompressionSession->stopRequestingMediaData();
    } else if (selected) {
        m_enabledVideoTrackID = trackID;
        m_parser->setShouldProvideMediaDataForTrackID(true, trackID);

        if (m_decompressionSession) {
            m_decompressionSession->requestMediaDataWhenReady([this, trackID] {
                didBecomeReadyForMoreSamples(trackID);
            });
        }
    }

    m_mediaSource->hasSelectedVideoChanged(*this);
}

void SourceBufferPrivateAVFObjC::trackDidChangeEnabled(AudioTrackPrivate& track, bool enabled)
{
    auto trackID = track.id().string().toUInt64();

    ALWAYS_LOG(LOGIDENTIFIER, "audio trackID = ", trackID, ", selected = ", enabled);

    if (!enabled) {
        ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
        RetainPtr<AVSampleBufferAudioRenderer> renderer = m_audioRenderers.get(trackID);
        ALLOW_NEW_API_WITHOUT_GUARDS_END
        m_parser->setShouldProvideMediaDataForTrackID(false, trackID);
        if (m_mediaSource)
            m_mediaSource->player()->removeAudioRenderer(renderer.get());
    } else {
        m_parser->setShouldProvideMediaDataForTrackID(true, trackID);
        ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
        RetainPtr<AVSampleBufferAudioRenderer> renderer;
        ALLOW_NEW_API_WITHOUT_GUARDS_END
        if (!m_audioRenderers.contains(trackID)) {
            renderer = adoptNS([PAL::allocAVSampleBufferAudioRendererInstance() init]);

            if (!renderer) {
                ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferAudioRenderer init] returned nil! bailing!");
                if (m_mediaSource)
                    m_mediaSource->failedToCreateRenderer(MediaSourcePrivateAVFObjC::RendererType::Audio);
                m_mediaSource->player()->setNetworkState(MediaPlayer::NetworkState::DecodeError);
                return;
            }

            auto weakThis = makeWeakPtr(*this);
            [renderer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
                if (weakThis)
                    weakThis->didBecomeReadyForMoreSamples(trackID);
            }];
            m_audioRenderers.set(trackID, renderer);
            [m_errorListener beginObservingRenderer:renderer.get()];
        } else
            renderer = m_audioRenderers.get(trackID);

        if (m_mediaSource)
            m_mediaSource->player()->addAudioRenderer(renderer.get());
    }
}

AVStreamDataParser* SourceBufferPrivateAVFObjC::parser() const
{
    if (is<SourceBufferParserAVFObjC>(m_parser.get()))
        return downcast<SourceBufferParserAVFObjC>(m_parser.get()).parser();
    return nil;
}

void SourceBufferPrivateAVFObjC::setCDMSession(CDMSessionMediaSourceAVFObjC* session)
{
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (session == m_session)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_session)
        m_session->removeSourceBuffer(this);

    m_session = makeWeakPtr(session);

    if (m_session) {
        m_session->addSourceBuffer(this);
        if (m_hasSessionSemaphore) {
            m_hasSessionSemaphore->signal();
            m_hasSessionSemaphore = nullptr;
        }

        if (m_hdcpError) {
            callOnMainThread([weakThis = makeWeakPtr(*this)] {
                if (!weakThis || !weakThis->m_session || !weakThis->m_hdcpError)
                    return;

                bool ignored = false;
                weakThis->m_session->layerDidReceiveError(nullptr, weakThis->m_hdcpError.get(), ignored);
            });
        }
    }
#else
    UNUSED_PARAM(session);
#endif
}

void SourceBufferPrivateAVFObjC::setCDMInstance(CDMInstance* instance)
{
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    auto* fpsInstance = is<CDMInstanceFairPlayStreamingAVFObjC>(instance) ? downcast<CDMInstanceFairPlayStreamingAVFObjC>(instance) : nullptr;
    if (fpsInstance == m_cdmInstance)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_cdmInstance = fpsInstance;
    attemptToDecrypt();
#else
    UNUSED_PARAM(instance);
#endif
}

void SourceBufferPrivateAVFObjC::attemptToDecrypt()
{
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (!m_cdmInstance || m_keyIDs.isEmpty() || !m_waitingForKey)
        return;

    auto instanceSession = m_cdmInstance->sessionForKeyIDs(m_keyIDs);
    if (!instanceSession)
        return;

    if (auto parser = this->parser())
        [instanceSession->contentKeySession() addContentKeyRecipient:parser];
    if (m_hasSessionSemaphore) {
        m_hasSessionSemaphore->signal();
        m_hasSessionSemaphore = nullptr;
    }
    m_waitingForKey = false;
#endif
}

void SourceBufferPrivateAVFObjC::flush()
{
    flushVideo();

    for (auto& renderer : m_audioRenderers.values())
        flush(renderer.get());
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
    ERROR_LOG(LOGIDENTIFIER, [[error description] UTF8String]);

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
        m_client->sourceBufferPrivateDidReceiveRenderingError(errorCode);
}

void SourceBufferPrivateAVFObjC::outputObscuredDueToInsufficientExternalProtectionChanged(bool obscured)
{
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (m_mediaSource && m_mediaSource->cdmInstance()) {
        m_mediaSource->outputObscuredDueToInsufficientExternalProtectionChanged(obscured);
        return;
    }
#else
    UNUSED_PARAM(obscured);
#endif

    ERROR_LOG(LOGIDENTIFIER, obscured);

    RetainPtr<NSError> error = [NSError errorWithDomain:@"com.apple.WebKit" code:'HDCP' userInfo:nil];
    layerDidReceiveError(m_displayLayer.get(), error.get());
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void SourceBufferPrivateAVFObjC::rendererDidReceiveError(AVSampleBufferAudioRenderer *renderer, NSError *error)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    ERROR_LOG(LOGIDENTIFIER, [[error description] UTF8String]);

    if ([error code] == 'HDCP')
        m_hdcpError = error;

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

void SourceBufferPrivateAVFObjC::flush(const AtomString& trackIDString)
{
    auto trackID = trackIDString.string().toUInt64();
    DEBUG_LOG(LOGIDENTIFIER, trackID);

    if (trackID == m_enabledVideoTrackID) {
        flushVideo();
    } else if (m_audioRenderers.contains(trackID))
        flush(m_audioRenderers.get(trackID).get());
}

void SourceBufferPrivateAVFObjC::flushVideo()
{
    DEBUG_LOG(LOGIDENTIFIER);
    [m_displayLayer flush];

    if (m_decompressionSession) {
        m_decompressionSession->flush();
        m_decompressionSession->notifyWhenHasAvailableVideoFrame([weakThis = makeWeakPtr(*this)] {
            if (weakThis && weakThis->m_mediaSource)
                weakThis->m_mediaSource->player()->setHasAvailableVideoFrame(true);
        });
    }

    m_cachedSize = WTF::nullopt;

    if (m_mediaSource) {
        m_mediaSource->player()->setHasAvailableVideoFrame(false);
        m_mediaSource->player()->flushPendingSizeChanges();
    }
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void SourceBufferPrivateAVFObjC::flush(AVSampleBufferAudioRenderer *renderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    [renderer flush];

    if (m_mediaSource)
        m_mediaSource->player()->setHasAvailableAudioSample(renderer, false);
}

void SourceBufferPrivateAVFObjC::enqueueSample(Ref<MediaSample>&& sample, const AtomString& trackIDString)
{
    auto trackID = trackIDString.string().toUInt64();
    if (trackID != m_enabledVideoTrackID && !m_audioRenderers.contains(trackID))
        return;

    PlatformSample platformSample = sample->platformSample();
    if (platformSample.type != PlatformSample::CMSampleBufferType)
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, "track ID = ", trackID, ", sample = ", sample.get());

    CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(platformSample.sample.cmSampleBuffer);
    ASSERT(formatDescription);
    if (!formatDescription) {
        ERROR_LOG(logSiteIdentifier, "Received sample with a null formatDescription. Bailing.");
        return;
    }
    auto mediaType = CMFormatDescriptionGetMediaType(formatDescription);

    if (trackID == m_enabledVideoTrackID) {
        // AVSampleBufferDisplayLayer will throw an un-documented exception if passed a sample
        // whose media type is not kCMMediaType_Video. This condition is exceptional; we should
        // never enqueue a non-video sample in a AVSampleBufferDisplayLayer.
        ASSERT(mediaType == kCMMediaType_Video);
        if (mediaType != kCMMediaType_Video) {
            ERROR_LOG(logSiteIdentifier, "Expected sample of type '", FourCC(kCMMediaType_Video), "', got '", FourCC(mediaType), "'. Bailing.");
            return;
        }

        FloatSize formatSize = FloatSize(CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true));
        if (!m_cachedSize || formatSize != m_cachedSize.value()) {
            DEBUG_LOG(logSiteIdentifier, "size changed to ", formatSize);
            bool sizeWasNull = !m_cachedSize;
            m_cachedSize = formatSize;
            if (m_mediaSource) {
                if (sizeWasNull)
                    m_mediaSource->player()->setNaturalSize(formatSize);
                else
                    m_mediaSource->player()->sizeWillChangeAtTime(sample->presentationTime(), formatSize);
            }
        }

        if (m_decompressionSession)
            m_decompressionSession->enqueueSample(platformSample.sample.cmSampleBuffer);

        if (!m_displayLayer)
            return;

        if (m_mediaSource && !m_mediaSource->player()->hasAvailableVideoFrame() && !sample->isNonDisplaying()) {
            DEBUG_LOG(logSiteIdentifier, "adding buffer attachment");

            bool havePrerollDecodeWithCompletionHandler = [PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(prerollDecodeWithCompletionHandler:)];

            if (!havePrerollDecodeWithCompletionHandler) {
                CMSampleBufferRef rawSampleCopy;
                CMSampleBufferCreateCopy(kCFAllocatorDefault, platformSample.sample.cmSampleBuffer, &rawSampleCopy);
                auto sampleCopy = adoptCF(rawSampleCopy);
                CMSetAttachment(sampleCopy.get(), kCMSampleBufferAttachmentKey_PostNotificationWhenConsumed, (__bridge CFDictionaryRef)@{ (__bridge NSString *)kCMSampleBufferAttachmentKey_PostNotificationWhenConsumed : @YES }, kCMAttachmentMode_ShouldNotPropagate);
                [m_displayLayer enqueueSampleBuffer:sampleCopy.get()];
#if PLATFORM(IOS_FAMILY)
                m_mediaSource->player()->setHasAvailableVideoFrame(true);
#endif
            } else {

                [m_displayLayer enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
                [m_displayLayer prerollDecodeWithCompletionHandler:[this, logSiteIdentifier, weakThis = makeWeakPtr(*this)] (BOOL success) mutable {
                    if (!weakThis)
                        return;

                    if (!success) {
                        ERROR_LOG(logSiteIdentifier, "prerollDecodeWithCompletionHandler failed");
                        return;
                    }

                    callOnMainThread([weakThis = WTFMove(weakThis)] () mutable {
                        if (!weakThis)
                            return;

                        weakThis->bufferWasConsumed();
                    });
                }];
            }
        } else
            [m_displayLayer enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];

    } else {
        // AVSampleBufferAudioRenderer will throw an un-documented exception if passed a sample
        // whose media type is not kCMMediaType_Audio. This condition is exceptional; we should
        // never enqueue a non-video sample in a AVSampleBufferAudioRenderer.
        ASSERT(mediaType == kCMMediaType_Audio);
        if (mediaType != kCMMediaType_Audio) {
            ERROR_LOG(logSiteIdentifier, "Expected sample of type '", FourCC(kCMMediaType_Audio), "', got '", FourCC(mediaType), "'. Bailing.");
            return;
        }

        auto renderer = m_audioRenderers.get(trackID);
        [renderer enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
        if (m_mediaSource && !sample->isNonDisplaying())
            m_mediaSource->player()->setHasAvailableAudioSample(renderer.get(), true);
    }
}

void SourceBufferPrivateAVFObjC::bufferWasConsumed()
{
    DEBUG_LOG(LOGIDENTIFIER);

    if (m_mediaSource)
        m_mediaSource->player()->setHasAvailableVideoFrame(true);
}

bool SourceBufferPrivateAVFObjC::isReadyForMoreSamples(const AtomString& trackIDString)
{
    auto trackID = trackIDString.string().toUInt64();
    if (trackID == m_enabledVideoTrackID) {
        if (m_decompressionSession)
            return m_decompressionSession->isReadyForMoreMediaData();

        return [m_displayLayer isReadyForMoreMediaData];
    }

    if (m_audioRenderers.contains(trackID))
        return [m_audioRenderers.get(trackID) isReadyForMoreMediaData];

    return false;
}

void SourceBufferPrivateAVFObjC::setActive(bool isActive)
{
    ALWAYS_LOG(LOGIDENTIFIER, isActive);
    if (m_mediaSource)
        m_mediaSource->sourceBufferPrivateDidChangeActiveState(this, isActive);
}

MediaTime SourceBufferPrivateAVFObjC::fastSeekTimeForMediaTime(const MediaTime& time, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
{
    if (!m_client)
        return time;
    return m_client->sourceBufferPrivateFastSeekTimeForMediaTime(time, negativeThreshold, positiveThreshold);
}

void SourceBufferPrivateAVFObjC::willSeek()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    flush();
}

FloatSize SourceBufferPrivateAVFObjC::naturalSize()
{
    return m_cachedSize.valueOr(FloatSize());
}

void SourceBufferPrivateAVFObjC::didBecomeReadyForMoreSamples(uint64_t trackID)
{
    INFO_LOG(LOGIDENTIFIER, trackID);

    if (trackID == m_enabledVideoTrackID) {
        if (m_decompressionSession)
            m_decompressionSession->stopRequestingMediaData();
        [m_displayLayer stopRequestingMediaData];
    } else if (m_audioRenderers.contains(trackID))
        [m_audioRenderers.get(trackID) stopRequestingMediaData];
    else
        return;

    if (m_client)
        m_client->sourceBufferPrivateDidBecomeReadyForMoreSamples(AtomString::number(trackID));
}

void SourceBufferPrivateAVFObjC::notifyClientWhenReadyForMoreSamples(const AtomString& trackIDString)
{
    auto trackID = trackIDString.string().toUInt64();
    if (trackID == m_enabledVideoTrackID) {
        if (m_decompressionSession) {
            m_decompressionSession->requestMediaDataWhenReady([this, trackID] {
                didBecomeReadyForMoreSamples(trackID);
            });
        }
        if (m_displayLayer) {
            auto weakThis = makeWeakPtr(*this);
            [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
                if (weakThis)
                    weakThis->didBecomeReadyForMoreSamples(trackID);
            }];
        }
    } else if (m_audioRenderers.contains(trackID)) {
        auto weakThis = makeWeakPtr(*this);
        [m_audioRenderers.get(trackID) requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
            if (weakThis)
                weakThis->didBecomeReadyForMoreSamples(trackID);
        }];
    }
}

bool SourceBufferPrivateAVFObjC::canSetMinimumUpcomingPresentationTime(const AtomString& trackIDString) const
{
    auto trackID = trackIDString.string().toUInt64();
    if (trackID == m_enabledVideoTrackID)
        return [getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(expectMinimumUpcomingSampleBufferPresentationTime:)];
    return false;
}

void SourceBufferPrivateAVFObjC::setMinimumUpcomingPresentationTime(const AtomString& trackIDString, const MediaTime& presentationTime)
{
    ASSERT(canSetMinimumUpcomingPresentationTime(trackIDString));
    if (canSetMinimumUpcomingPresentationTime(trackIDString))
        [m_displayLayer expectMinimumUpcomingSampleBufferPresentationTime:toCMTime(presentationTime)];
}

void SourceBufferPrivateAVFObjC::clearMinimumUpcomingPresentationTime(const AtomString& trackIDString)
{
    ASSERT(canSetMinimumUpcomingPresentationTime(trackIDString));
    if (canSetMinimumUpcomingPresentationTime(trackIDString))
        [m_displayLayer resetUpcomingSampleBufferPresentationTimeExpectations];
}

bool SourceBufferPrivateAVFObjC::canSwitchToType(const ContentType& contentType)
{
    ALWAYS_LOG(LOGIDENTIFIER, contentType);

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;
    return MediaPlayerPrivateMediaSourceAVFObjC::supportsType(parameters) != MediaPlayer::SupportsType::IsNotSupported;
}

void SourceBufferPrivateAVFObjC::setVideoLayer(AVSampleBufferDisplayLayer* layer)
{
    if (layer == m_displayLayer)
        return;

    ASSERT(!layer || !m_decompressionSession || hasSelectedVideo());

    if (m_displayLayer) {
        [m_displayLayer flush];
        [m_displayLayer stopRequestingMediaData];
        [m_errorListener stopObservingLayer:m_displayLayer.get()];
    }

    m_displayLayer = layer;

    if (m_displayLayer) {
        auto weakThis = makeWeakPtr(*this);
        [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
            if (weakThis)
                weakThis->didBecomeReadyForMoreSamples(m_enabledVideoTrackID);
        }];
        [m_errorListener beginObservingLayer:m_displayLayer.get()];
        if (m_client)
            m_client->sourceBufferPrivateReenqueSamples(AtomString::number(m_enabledVideoTrackID));
    }
}

void SourceBufferPrivateAVFObjC::setDecompressionSession(WebCoreDecompressionSession* decompressionSession)
{
    if (m_decompressionSession == decompressionSession)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_decompressionSession) {
        m_decompressionSession->stopRequestingMediaData();
        m_decompressionSession->invalidate();
    }

    m_decompressionSession = decompressionSession;

    if (!m_decompressionSession)
        return;

    m_decompressionSession->requestMediaDataWhenReady([weakThis = makeWeakPtr(*this)] {
        if (weakThis)
            weakThis->didBecomeReadyForMoreSamples(weakThis->m_enabledVideoTrackID);
    });
    m_decompressionSession->notifyWhenHasAvailableVideoFrame([weakThis = makeWeakPtr(*this)] {
        if (weakThis && weakThis->m_mediaSource)
            weakThis->m_mediaSource->player()->setHasAvailableVideoFrame(true);
    });
    if (m_client)
        m_client->sourceBufferPrivateReenqueSamples(AtomString::number(m_enabledVideoTrackID));
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateAVFObjC::logChannel() const
{
    return LogMediaSource;
}
#endif

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
