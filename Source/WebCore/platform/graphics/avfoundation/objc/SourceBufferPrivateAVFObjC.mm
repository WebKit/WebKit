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
#import "MediaSessionManagerCocoa.h"
#import "MediaSourcePrivateAVFObjC.h"
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
#import <wtf/WorkQueue.h>
#import <wtf/text/AtomString.h>
#import <wtf/text/CString.h>
#import <wtf/text/StringToIntegerConversion.h>

#pragma mark - Soft Linking

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (WebCoreAVSampleBufferDisplayLayerQueueManagementPrivate)
- (void)prerollDecodeWithCompletionHandler:(void (^)(BOOL success))block;
- (void)expectMinimumUpcomingSampleBufferPresentationTime: (CMTime)minimumUpcomingPresentationTime;
- (void)resetUpcomingSampleBufferPresentationTimeExpectations;
@end

@interface AVSampleBufferDisplayLayer (WebCoreSampleBufferKeySession) <AVContentKeyRecipient>
@end

@interface AVSampleBufferAudioRenderer (WebCoreSampleBufferKeySession) <AVContentKeyRecipient>
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
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(audioRendererWasAutomaticallyFlushed:) name:AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification object:renderer];
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
- (void)stopObservingRenderer:(AVSampleBufferAudioRenderer*)renderer
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    ASSERT(_parent);
    ASSERT(_renderers.contains(renderer));

    [renderer removeObserver:self forKeyPath:@"error"];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification object:renderer];

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
                if (auto strongParent = RefPtr { parent.get() })
                    strongParent->layerDidReceiveError(layer.get(), error.get());
            });
        } else if ([keyPath isEqualToString:@"outputObscuredDueToInsufficientExternalProtection"]) {
            callOnMainThread([parent = _parent, obscured = [[change valueForKey:NSKeyValueChangeNewKey] boolValue]] {
                if (auto strongParent = RefPtr { parent.get() })
                    strongParent->outputObscuredDueToInsufficientExternalProtectionChanged(obscured);
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
            if (auto strongParent = RefPtr { parent.get() })
                strongParent->rendererDidReceiveError(renderer.get(), error.get());
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
        if (auto strongParent = RefPtr { parent.get() })
            strongParent->layerDidReceiveError(layer.get(), error.get());
    });
}

- (void)audioRendererWasAutomaticallyFlushed:(NSNotification*)note
{
    RetainPtr<AVSampleBufferAudioRenderer> renderer = (AVSampleBufferAudioRenderer *)[note object];
    if (!_renderers.contains(renderer.get()))
        return;

    callOnMainThread([parent = _parent, renderer = WTFMove(renderer), flushTime = [[[note userInfo] valueForKey:AVSampleBufferAudioRendererFlushTimeKey] CMTimeValue]] {
        if (auto strongParent = RefPtr { parent.get() })
            strongParent->rendererWasAutomaticallyFlushed(renderer.get(), flushTime);
    });
}
@end

namespace WebCore {

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

static bool sampleBufferRenderersSupportKeySession()
{
    static bool supports = false;
#if HAVE(AVCONTENTKEYSPECIFIER)
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        supports =
        [PAL::getAVSampleBufferAudioRendererClass() conformsToProtocol:@protocol(AVContentKeyRecipient)]
            && [PAL::getAVSampleBufferDisplayLayerClass() conformsToProtocol:@protocol(AVContentKeyRecipient)]
            && MediaSessionManagerCocoa::sampleBufferContentKeySessionSupportEnabled();
    });
#endif
    return supports;
}

static Vector<Ref<SharedBuffer>> copyKeyIDs(const Vector<Ref<SharedBuffer>> keyIDs)
{
    return keyIDs.map([] (auto& keyID) {
        return keyID.copyRef();
    });
}

static void bufferWasConsumedCallback(CMNotificationCenterRef, const void* listener, CFStringRef notificationName, const void*, CFTypeRef)
{
    if (!CFEqual(PAL::kCMSampleBufferConsumerNotification_BufferConsumed, notificationName))
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
    , m_errorListener(adoptNS([[WebAVSampleBufferErrorListener alloc] initWithParent:*this]))
    , m_appendQueue(WorkQueue::create("SourceBufferPrivateAVFObjC data parser queue"))
    , m_mediaSource(parent)
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    , m_keyStatusesChangedObserver(makeUniqueRef<Observer<void()>>([this] { keyStatusesChanged(); }))
#endif
    , m_mapID(nextMapID())
#if !RELEASE_LOG_DISABLED
    , m_logger(parent->logger())
    , m_logIdentifier(parent->nextSourceBufferLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(prerollDecodeWithCompletionHandler:)])
        PAL::CMNotificationCenterAddListener(PAL::CMNotificationCenterGetDefaultLocalCenter(), reinterpret_cast<void*>(m_mapID), bufferWasConsumedCallback, PAL::kCMSampleBufferConsumerNotification_BufferConsumed, nullptr, 0);

#if !RELEASE_LOG_DISABLED
    m_parser->setLogger(m_logger.get(), m_logIdentifier);
#endif

    sourceBufferMap().add(m_mapID, *this);
}

SourceBufferPrivateAVFObjC::~SourceBufferPrivateAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    sourceBufferMap().remove(m_mapID);
    destroyStreamDataParser();
    destroyRenderers();
    clearTracks();

    if (![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(prerollDecodeWithCompletionHandler:)])
        PAL::CMNotificationCenterRemoveListener(PAL::CMNotificationCenterGetDefaultLocalCenter(), this, bufferWasConsumedCallback, PAL::kCMSampleBufferConsumerNotification_BufferConsumed, nullptr);

    abort();
    resetParserState();
}

void SourceBufferPrivateAVFObjC::didParseInitializationData(InitializationSegment&& segment)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!m_mediaSource) {
        return;
    }

    if (auto player = this->player(); player && player->shouldCheckHardwareSupport()) {
        for (auto& info : segment.videoTracks) {
            auto codec = FourCC::fromString(info.description->codec());
            if (!codec)
                continue;
            if (!codecsMeetHardwareDecodeRequirements({{ *codec }}, player->mediaContentTypesRequiringHardwareSupport())) {
                m_parsingSucceeded = false;
                return;
            }
        }
    }

    m_protectedTrackInitDataMap.swap(m_pendingProtectedTrackInitDataMap);
    m_pendingProtectedTrackInitDataMap.clear();

    for (auto videoTrackInfo : segment.videoTracks) {
        videoTrackInfo.track->setSelectedChangedCallback([weakThis = WeakPtr { *this }, this] (VideoTrackPrivate& track, bool selected) {
            if (!weakThis)
                return;

            auto videoTrackSelectedChanged = [weakThis, this, trackRef = Ref { track }, selected] {
                if (!weakThis)
                    return;
                trackDidChangeSelected(trackRef, selected);
            };

            if (!m_processingInitializationSegment) {
                videoTrackSelectedChanged();
                return;
            }

            m_pendingTrackChangeCallbacks.append(WTFMove(videoTrackSelectedChanged));
        });

        m_videoTracks.set(videoTrackInfo.track->id(), videoTrackInfo.track);
    }

    for (auto audioTrackInfo : segment.audioTracks) {
        audioTrackInfo.track->setEnabledChangedCallback([weakThis = WeakPtr { *this }, this] (AudioTrackPrivate& track, bool enabled) {
            if (!weakThis)
                return;

            auto audioTrackEnabledChanged= [weakThis, this, trackRef = Ref { track }, enabled] {
                if (!weakThis)
                    return;

                trackDidChangeEnabled(trackRef, enabled);
            };

            if (!m_processingInitializationSegment) {
                audioTrackEnabledChanged();
                return;
            }

            m_pendingTrackChangeCallbacks.append(WTFMove(audioTrackEnabledChanged));
        });

        m_audioTracks.set(audioTrackInfo.track->id(), audioTrackInfo.track);
    }

    m_processingInitializationSegment = true;
    didReceiveInitializationSegment(WTFMove(segment), [this, weakThis = WeakPtr { *this }, abortCalled = m_abortCalled] (SourceBufferPrivateClient::ReceiveResult result) {
        ASSERT(isMainThread());
        if (!weakThis)
            return;

        m_processingInitializationSegment = false;
        auto didAbort = abortCalled != weakThis->m_abortCalled;
        if (didAbort || result != SourceBufferPrivateClient::ReceiveResult::RecieveSucceeded) {
            ERROR_LOG(LOGIDENTIFIER, "failed to process initialization segment: didAbort = ", didAbort, " recieveResult = ", result);
            m_pendingTrackChangeCallbacks.clear();
            m_mediaSamples.clear();
            return;
        }

        auto callbacks = std::exchange(m_pendingTrackChangeCallbacks, { });
        for (auto& callback : callbacks)
            callback();

        if (auto player = this->player())
            player->characteristicsChanged();

        auto mediaSamples = std::exchange(m_mediaSamples, { });
        for (auto& trackIdMediaSamplePair : mediaSamples) {
            auto trackId = trackIdMediaSamplePair.first;
            auto& mediaSample = trackIdMediaSamplePair.second;
            if (trackId == m_enabledVideoTrackID || m_audioRenderers.contains(trackId)) {
                DEBUG_LOG(LOGIDENTIFIER, mediaSample.get());
                didReceiveSample(WTFMove(mediaSample));
            }
        }

        ALWAYS_LOG(LOGIDENTIFIER, "initialization segment was processed");

        if (m_hasPendingAppendCompletedCallback) {
            m_hasPendingAppendCompletedCallback = false;
            appendCompleted();
        }
    });
}

void SourceBufferPrivateAVFObjC::didEncounterErrorDuringParsing(int32_t code)
{
#if LOG_DISABLED
    UNUSED_PARAM(code);
#endif
    ERROR_LOG(LOGIDENTIFIER, code);

    m_parsingSucceeded = false;
}

void SourceBufferPrivateAVFObjC::didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&& mediaSample, uint64_t trackId, const String& mediaType)
{
    UNUSED_PARAM(mediaType);

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    auto findResult = m_protectedTrackInitDataMap.find(trackId);
    if (findResult != m_protectedTrackInitDataMap.end())
        mediaSample->setKeyIDs(copyKeyIDs(findResult->value.keyIDs));
#endif

    if (m_processingInitializationSegment) {
        DEBUG_LOG(LOGIDENTIFIER, mediaSample.get());
        m_mediaSamples.append(std::make_pair(trackId, WTFMove(mediaSample)));
        return;
    }

    if (trackId != m_enabledVideoTrackID && !m_audioRenderers.contains(trackId)) {
        // FIXME(125161): We don't handle text tracks, and passing this sample up to SourceBuffer
        // will just confuse its state. Drop this sample until we can handle text tracks properly.
        return;
    }

    DEBUG_LOG(LOGIDENTIFIER, mediaSample.get());
    didReceiveSample(WTFMove(mediaSample));
}

void SourceBufferPrivateAVFObjC::willProvideContentKeyRequestInitializationDataForTrackID(uint64_t trackID)
{
    if (!m_mediaSource)
        return;

#if HAVE(AVSTREAMSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    ALWAYS_LOG(LOGIDENTIFIER, "track = ", trackID);

    m_protectedTrackID = trackID;

    auto parser = this->streamDataParser();
    if (!parser)
        return;

    auto player = this->player();
    if (!player)
        return;

    if (CDMSessionMediaSourceAVFObjC* session = player->cdmSession())
        session->addParser(parser);
    else if (!CDMSessionAVContentKeySession::isAvailable()) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [player->streamSession() addStreamDataParser:parser];
        END_BLOCK_OBJC_EXCEPTIONS
    }
#else
    UNUSED_PARAM(trackID);
#endif
}

void SourceBufferPrivateAVFObjC::didProvideContentKeyRequestInitializationDataForTrackID(Ref<SharedBuffer>&& initData, uint64_t trackID, Box<BinarySemaphore> hasSessionSemaphore)
{
    auto player = this->player();
    if (!player)
        return;

#if HAVE(AVCONTENTKEYSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    ALWAYS_LOG(LOGIDENTIFIER, "track = ", trackID);

    m_protectedTrackID = trackID;
    m_initData = WTFMove(initData);
    m_mediaSource->sourceBufferKeyNeeded(this, *m_initData);

    if (auto session = player->cdmSession()) {
        if (sampleBufferRenderersSupportKeySession()) {
            // no-op.
        } else if (auto parser = this->streamDataParser())
            session->addParser(parser);
        if (hasSessionSemaphore)
            hasSessionSemaphore->signal();
        return;
    }
#endif

    if (m_hasSessionSemaphore)
        m_hasSessionSemaphore->signal();
    m_hasSessionSemaphore = hasSessionSemaphore;
    
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    auto keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsSinf(*m_initData);
    if (!keyIDs)
        return;

    m_pendingProtectedTrackInitDataMap.set(trackID, TrackInitData { m_initData.copyRef(), *keyIDs });

    if (m_cdmInstance) {
        if (auto instanceSession = m_cdmInstance->sessionForKeyIDs(keyIDs.value())) {
            if (sampleBufferRenderersSupportKeySession()) {
                // no-op.
            } else if (auto parser = this->streamDataParser())
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
    player->initializationDataEncountered("sinf"_s, m_initData->tryCreateArrayBuffer());
    player->needsVideoLayerChanged();

    m_waitingForKey = true;
    player->waitingForKeyChanged();
#endif

    UNUSED_PARAM(initData);
    UNUSED_PARAM(trackID);
    UNUSED_PARAM(hasSessionSemaphore);
}

bool SourceBufferPrivateAVFObjC::needsVideoLayer() const
{
    if (m_protectedTrackID == notFound)
        return false;

    if (m_enabledVideoTrackID != m_protectedTrackID)
        return false;

    // When video content is protected and keys are assigned through
    // the renderers, decoding content through decompression sessions
    // will fail. In this scenario, ask the player to create a layer
    // instead.
    return sampleBufferRenderersSupportKeySession();
}

void SourceBufferPrivateAVFObjC::append(Ref<SharedBuffer>&& data)
{
    ALWAYS_LOG(LOGIDENTIFIER, "data length = ", data->size());

    ASSERT(!m_hasSessionSemaphore);
    ASSERT(!m_abortSemaphore);

    if (m_client)
        m_client->sourceBufferPrivateReportExtraMemoryCost(totalTrackBufferSizeInBytes());

    m_parser->setDidParseInitializationDataCallback([weakThis = WeakPtr { *this }, abortCalled = m_abortCalled] (InitializationSegment&& segment) {
        ASSERT(isMainThread());
        if (!weakThis || abortCalled != weakThis->m_abortCalled)
            return;
        weakThis->didParseInitializationData(WTFMove(segment));
    });

    m_parser->setDidEncounterErrorDuringParsingCallback([weakThis = WeakPtr { *this }, abortCalled = m_abortCalled] (int32_t errorCode) {
        ASSERT(isMainThread());
        if (!weakThis || abortCalled != weakThis->m_abortCalled)
            return;
        weakThis->didEncounterErrorDuringParsing(errorCode);
    });

    m_parser->setDidProvideMediaDataCallback([weakThis = WeakPtr { *this }, abortCalled = m_abortCalled] (Ref<MediaSampleAVFObjC>&& sample, uint64_t trackId, const String& mediaType) {
        ASSERT(isMainThread());
        if (!weakThis || abortCalled != weakThis->m_abortCalled)
            return;
        weakThis->didProvideMediaDataForTrackId(WTFMove(sample), trackId, mediaType);
    });

    m_abortSemaphore = Box<Semaphore>::create(0);
    m_parser->setWillProvideContentKeyRequestInitializationDataForTrackIDCallback([weakThis = WeakPtr { *this }, abortSemaphore = m_abortSemaphore, abortCalled = m_abortCalled] (uint64_t trackID) mutable {
        // We must call synchronously to the main thread, as the AVStreamSession must be associated
        // with the streamDataParser before the delegate method returns.
        Box<BinarySemaphore> respondedSemaphore = Box<BinarySemaphore>::create();
        callOnMainThread([weakThis = WTFMove(weakThis), abortCalled, trackID, respondedSemaphore]() {
            if (weakThis && abortCalled == weakThis->m_abortCalled)
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

    m_parser->setDidProvideContentKeyRequestInitializationDataForTrackIDCallback([weakThis = WeakPtr { *this }, abortSemaphore = m_abortSemaphore, abortCalled = m_abortCalled](Ref<SharedBuffer>&& initData, uint64_t trackID) mutable {
        // Called on the data parser queue.
        Box<BinarySemaphore> hasSessionSemaphore = Box<BinarySemaphore>::create();
        callOnMainThread([weakThis = WTFMove(weakThis), abortCalled, initData = WTFMove(initData), trackID, hasSessionSemaphore] () mutable {
            if (!weakThis || abortCalled != weakThis->m_abortCalled)
                return;
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

    m_parser->setDidProvideContentKeyRequestIdentifierForTrackIDCallback([weakThis = WeakPtr { *this }, abortCalled = m_abortCalled] (Ref<SharedBuffer>&& initData, uint64_t trackID) {
        ASSERT(isMainThread());
        if (!weakThis || abortCalled != weakThis->m_abortCalled)
            return;
        weakThis->didProvideContentKeyRequestInitializationDataForTrackID(WTFMove(initData), trackID, nullptr);
    });

    m_parsingSucceeded = true;

    m_appendQueue->dispatch([data = WTFMove(data), weakThis = m_appendWeakFactory.createWeakPtr(*this), parser = m_parser, abortCalled = m_abortCalled]() mutable {
        parser->appendData(WTFMove(data), [weakThis = WTFMove(weakThis), abortCalled]() mutable {
            callOnMainThread([weakThis = WTFMove(weakThis), abortCalled] {
                if (!weakThis || abortCalled != weakThis->m_abortCalled)
                    return;

                if (weakThis->m_processingInitializationSegment) {
                    weakThis->m_hasPendingAppendCompletedCallback = true;
                    return;
                }

                weakThis->appendCompleted();
            });
        });
    });
}

void SourceBufferPrivateAVFObjC::appendCompleted()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_abortSemaphore) {
        m_abortSemaphore->signal();
        m_abortSemaphore = nil;
    }

    if (m_hasSessionSemaphore) {
        m_hasSessionSemaphore->signal();
        m_hasSessionSemaphore = nil;
    }

    if (auto player = this->player(); player && m_parsingSucceeded)
        player->setLoadingProgresssed(true);

    SourceBufferPrivate::appendCompleted(m_parsingSucceeded, m_mediaSource ? m_mediaSource->isEnded() : true);
}

void SourceBufferPrivateAVFObjC::abort()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    // The parsing queue may be blocked waiting for the main thread to provide it an AVStreamSession. We
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

    m_abortCalled++;
}

void SourceBufferPrivateAVFObjC::resetParserState()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    // Wait until all tasks in the workqueue have run.
    m_appendQueue->dispatchSync([] { });
    m_mediaSamples.clear();
    m_hasPendingAppendCompletedCallback = false;
    m_processingInitializationSegment = false;
    m_parser->resetParserState();
}

void SourceBufferPrivateAVFObjC::destroyStreamDataParser()
{
    auto parser = this->streamDataParser();
    if (!parser)
        return;
#if HAVE(AVSTREAMSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (auto player = this->player(); player && player->hasStreamSession())
        [player->streamSession() removeStreamDataParser:parser];
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
        if (auto player = this->player())
            player->removeAudioRenderer(renderer.get());
        [renderer flush];
        [renderer stopRequestingMediaData];
        [m_errorListener stopObservingRenderer:renderer.get()];

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        if (m_cdmInstance && sampleBufferRenderersSupportKeySession())
            [m_cdmInstance->contentKeySession() removeContentKeyRecipient:renderer.get()];
#endif
    }

    [m_errorListener invalidate];
    m_errorListener = nullptr;

    m_audioRenderers.clear();
}

void SourceBufferPrivateAVFObjC::clearTracks()
{
    for (auto& track : m_videoTracks.values()) {
        track->setSelectedChangedCallback(nullptr);
        if (auto player = this->player())
            player->removeVideoTrack(*track);
    }
    m_videoTracks.clear();

    for (auto& track : m_audioTracks.values()) {
        track->setEnabledChangedCallback(nullptr);
        if (auto player = this->player())
            player->removeAudioTrack(*track);
    }
    m_audioTracks.clear();
}

void SourceBufferPrivateAVFObjC::removedFromMediaSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    clearTrackBuffers();
    destroyStreamDataParser();
    destroyRenderers();

    if (m_mediaSource)
        m_mediaSource->removeSourceBuffer(this);
}

MediaPlayer::ReadyState SourceBufferPrivateAVFObjC::readyState() const
{
    if (auto player = this->player())
        return player->readyState();
    return MediaPlayer::ReadyState::HaveNothing;
}

void SourceBufferPrivateAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    ALWAYS_LOG(LOGIDENTIFIER, readyState);

    if (auto player = this->player())
        player->setReadyState(readyState);
}

bool SourceBufferPrivateAVFObjC::hasSelectedVideo() const
{
    return m_enabledVideoTrackID != notFound;
}

void SourceBufferPrivateAVFObjC::trackDidChangeSelected(VideoTrackPrivate& track, bool selected)
{
    auto trackID = parseIntegerAllowingTrailingJunk<uint64_t>(track.id()).value_or(0);

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
            m_decompressionSession->requestMediaDataWhenReady([weakThis = WeakPtr { *this }, trackID] {
                if (weakThis)
                    weakThis->didBecomeReadyForMoreSamples(trackID);
            });
        }
    }

    if (auto* player = this->player())
        player->needsVideoLayerChanged();

    m_mediaSource->hasSelectedVideoChanged(*this);
}

void SourceBufferPrivateAVFObjC::trackDidChangeEnabled(AudioTrackPrivate& track, bool enabled)
{
    auto trackID = parseIntegerAllowingTrailingJunk<uint64_t>(track.id()).value_or(0);

    ALWAYS_LOG(LOGIDENTIFIER, "audio trackID = ", trackID, ", enabled = ", enabled);

    if (!enabled) {
        ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
        RetainPtr<AVSampleBufferAudioRenderer> renderer = m_audioRenderers.get(trackID);
        ALLOW_NEW_API_WITHOUT_GUARDS_END
        m_parser->setShouldProvideMediaDataForTrackID(false, trackID);
        if (auto player = this->player())
            player->removeAudioRenderer(renderer.get());
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
                if (auto player = this->player())
                    player->setNetworkState(MediaPlayer::NetworkState::DecodeError);
                return;
            }

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        if (m_cdmInstance && sampleBufferRenderersSupportKeySession())
            [m_cdmInstance->contentKeySession() addContentKeyRecipient:renderer.get()];
#endif

            WeakPtr weakThis { *this };
            [renderer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
                if (weakThis)
                    weakThis->didBecomeReadyForMoreSamples(trackID);
            }];
            m_audioRenderers.set(trackID, renderer);
            [m_errorListener beginObservingRenderer:renderer.get()];
        } else
            renderer = m_audioRenderers.get(trackID);

        if (auto player = this->player())
            player->addAudioRenderer(renderer.get());
    }
}

AVStreamDataParser* SourceBufferPrivateAVFObjC::streamDataParser() const
{
    if (is<SourceBufferParserAVFObjC>(m_parser))
        return downcast<SourceBufferParserAVFObjC>(m_parser.get()).streamDataParser();
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

    m_session = session;

    if (m_session) {
        m_session->addSourceBuffer(this);
        if (m_hasSessionSemaphore) {
            m_hasSessionSemaphore->signal();
            m_hasSessionSemaphore = nullptr;
        }

        if (m_hdcpError) {
            callOnMainThread([weakThis = WeakPtr { *this }] {
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
    auto* fpsInstance = dynamicDowncast<CDMInstanceFairPlayStreamingAVFObjC>(instance);
    if (fpsInstance == m_cdmInstance)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    if (sampleBufferRenderersSupportKeySession() && m_cdmInstance) {
        if (m_displayLayer)
            [m_cdmInstance->contentKeySession() removeContentKeyRecipient:m_displayLayer.get()];

        for (auto& audioRenderer : m_audioRenderers.values())
            [m_cdmInstance->contentKeySession() removeContentKeyRecipient:audioRenderer.get()];
    }

    m_cdmInstance = fpsInstance;

    if (sampleBufferRenderersSupportKeySession() && m_cdmInstance) {
        if (m_displayLayer)
            [m_cdmInstance->contentKeySession() addContentKeyRecipient:m_displayLayer.get()];

        for (auto& audioRenderer : m_audioRenderers.values())
            [m_cdmInstance->contentKeySession() addContentKeyRecipient:audioRenderer.get()];
    }

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

    if (!sampleBufferRenderersSupportKeySession()) {
        if (auto parser = this->streamDataParser())
            [instanceSession->contentKeySession() addContentKeyRecipient:parser];
    }
    if (m_hasSessionSemaphore) {
        m_hasSessionSemaphore->signal();
        m_hasSessionSemaphore = nullptr;
    }
    m_waitingForKey = false;
#endif
}

void SourceBufferPrivateAVFObjC::flush()
{
    if (m_videoTracks.size())
        flushVideo();

    if (!m_audioTracks.size())
        return;

    for (auto& renderer : m_audioRenderers.values())
        flushAudio(renderer.get());
}

#if PLATFORM(IOS_FAMILY)
void SourceBufferPrivateAVFObjC::flushIfNeeded()
{
    if (!m_displayLayerWasInterrupted)
        return;

    m_displayLayerWasInterrupted = false;
    if (m_videoTracks.size())
        flushVideo();

    // We initiatively enqueue samples instead of waiting for the
    // media data requests from m_decompressionSession and m_displayLayer.
    // In addition, we need to enqueue a sync sample (IDR video frame) first.
    if (m_decompressionSession)
        m_decompressionSession->stopRequestingMediaData();
    [m_displayLayer stopRequestingMediaData];

    reenqueSamples(AtomString::number(m_enabledVideoTrackID));
}
#endif

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

#if PLATFORM(IOS_FAMILY)
    if ([layer status] == AVQueuedSampleBufferRenderingStatusFailed && [[error domain] isEqualToString:@"AVFoundationErrorDomain"] && [error code] == AVErrorOperationInterrupted) {
        m_displayLayerWasInterrupted = true;
        return;
    }
#endif

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

void SourceBufferPrivateAVFObjC::rendererWasAutomaticallyFlushed(AVSampleBufferAudioRenderer *renderer, const CMTime& time)
{
    auto mediaTime = PAL::toMediaTime(time);
    ERROR_LOG(LOGIDENTIFIER, mediaTime);
    AtomString trackId;
    for (auto& pair : m_audioRenderers) {
        if (pair.value.get() == renderer) {
            trackId = AtomString::number(pair.key);
            break;
        }
    }
    if (trackId.isEmpty()) {
        ERROR_LOG(LOGIDENTIFIER, "Couldn't find track attached to Audio Renderer.");
        return;
    }
    reenqueSamples(trackId);
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
    auto trackID = parseIntegerAllowingTrailingJunk<uint64_t>(trackIDString).value_or(0);
    DEBUG_LOG(LOGIDENTIFIER, trackID);

    if (trackID == m_enabledVideoTrackID) {
        flushVideo();
    } else if (m_audioRenderers.contains(trackID))
        flushAudio(m_audioRenderers.get(trackID).get());
}

void SourceBufferPrivateAVFObjC::flushVideo()
{
    DEBUG_LOG(LOGIDENTIFIER);
    [m_displayLayer flush];

    if (m_decompressionSession) {
        m_decompressionSession->flush();
        m_decompressionSession->notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }] {
            if (weakThis && weakThis->player())
                weakThis->player()->setHasAvailableVideoFrame(true);
        });
    }

    m_cachedSize = std::nullopt;

    if (auto player = this->player()) {
        player->setHasAvailableVideoFrame(false);
        player->flushPendingSizeChanges();
    }
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void SourceBufferPrivateAVFObjC::flushAudio(AVSampleBufferAudioRenderer *renderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    [renderer flush];

    if (auto player = this->player())
        player->setHasAvailableAudioSample(renderer, false);
}

bool SourceBufferPrivateAVFObjC::trackIsBlocked(uint64_t trackID) const
{
    for (auto& samplePair : m_blockedSamples) {
        if (samplePair.first == trackID)
            return true;
    }
    return false;
}

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
void SourceBufferPrivateAVFObjC::keyStatusesChanged()
{
    while (!m_blockedSamples.isEmpty()) {
        auto& firstPair = m_blockedSamples.first();

        // If we still can't enqueue the sample, bail.
        if (!canEnqueueSample(firstPair.first, firstPair.second))
            return;

        auto firstPairTaken = m_blockedSamples.takeFirst();
        enqueueSample(WTFMove(firstPairTaken.second), firstPairTaken.first);
    }
}
#endif

bool SourceBufferPrivateAVFObjC::canEnqueueSample(uint64_t trackID, const MediaSampleAVFObjC& sample)
{
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    // If sample buffers don't support AVContentKeySession: enqueue sample
    if (!sampleBufferRenderersSupportKeySession())
        return true;

    // if sample is unencrytped: enqueue sample
    if (sample.keyIDs().isEmpty())
        return true;

    // if sample is encrypted, but we are not attached to a CDM: do not enqueue sample.
    if (!m_cdmInstance)
        return false;

    // DecompressionSessions doesn't support encrypted media.
    if (!m_displayLayer)
        return false;

    // if sample is encrypted, and keyIDs match the current set of keyIDs: enqueue sample.
    auto findResult = m_currentTrackIDs.find(trackID);
    if (findResult != m_currentTrackIDs.end() && findResult->value == sample.keyIDs())
        return true;

    // if sample's set of keyIDs does not match the current set of keyIDs, consult with the CDM
    // to determine if the keyIDs are usable; if so, update the current set of keyIDs and enqueue sample.
    if (m_cdmInstance->isAnyKeyUsable(sample.keyIDs())) {
        m_currentTrackIDs.set(trackID, sample.keyIDs());
        return true;
    }
    return false;
#else
    return true;
#endif
}

void SourceBufferPrivateAVFObjC::enqueueSample(Ref<MediaSample>&& sample, const AtomString& trackIDString)
{
    auto trackID = parseIntegerAllowingTrailingJunk<uint64_t>(trackIDString).value_or(0);
    if (trackID != m_enabledVideoTrackID && !m_audioRenderers.contains(trackID))
        return;

    ASSERT(is<MediaSampleAVFObjC>(sample));
    if (!is<MediaSampleAVFObjC>(sample))
        return;

    Ref<MediaSampleAVFObjC> sampleAVFObjC = static_reference_cast<MediaSampleAVFObjC>(WTFMove(sample));
    if (!sampleAVFObjC->keyIDs().isEmpty() && !canEnqueueSample(trackID, sampleAVFObjC)) {
        m_blockedSamples.append({ trackID, WTFMove(sampleAVFObjC) });
        return;
    }

    enqueueSample(WTFMove(sampleAVFObjC), trackID);
}

void SourceBufferPrivateAVFObjC::enqueueSample(Ref<MediaSampleAVFObjC>&& sample, uint64_t trackID)
{
    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, "track ID = ", trackID, ", sample = ", sample.get());

    PlatformSample platformSample = sample->platformSample();

    CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(platformSample.sample.cmSampleBuffer);
    ASSERT(formatDescription);
    if (!formatDescription) {
        ERROR_LOG(logSiteIdentifier, "Received sample with a null formatDescription. Bailing.");
        return;
    }
    auto mediaType = PAL::CMFormatDescriptionGetMediaType(formatDescription);

    if (trackID == m_enabledVideoTrackID) {
        // AVSampleBufferDisplayLayer will throw an un-documented exception if passed a sample
        // whose media type is not kCMMediaType_Video. This condition is exceptional; we should
        // never enqueue a non-video sample in a AVSampleBufferDisplayLayer.
        ASSERT(mediaType == kCMMediaType_Video);
        if (mediaType != kCMMediaType_Video) {
            ERROR_LOG(logSiteIdentifier, "Expected sample of type '", FourCC(kCMMediaType_Video), "', got '", FourCC(mediaType), "'. Bailing.");
            return;
        }

        auto player = this->player();
        FloatSize formatSize = FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true));
        if (!m_cachedSize || formatSize != m_cachedSize.value()) {
            DEBUG_LOG(logSiteIdentifier, "size changed to ", formatSize);
            bool sizeWasNull = !m_cachedSize;
            m_cachedSize = formatSize;
            if (player) {
                if (sizeWasNull)
                    player->setNaturalSize(formatSize);
                else
                    player->sizeWillChangeAtTime(sample->presentationTime(), formatSize);
            }
        }

        if (m_decompressionSession)
            m_decompressionSession->enqueueSample(platformSample.sample.cmSampleBuffer);

        if (!m_displayLayer)
            return;

        if (player && !player->hasAvailableVideoFrame() && !sample->isNonDisplaying()) {
            DEBUG_LOG(logSiteIdentifier, "adding buffer attachment");

            bool havePrerollDecodeWithCompletionHandler = [PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(prerollDecodeWithCompletionHandler:)];

            if (!havePrerollDecodeWithCompletionHandler) {
                CMSampleBufferRef rawSampleCopy;
                PAL::CMSampleBufferCreateCopy(kCFAllocatorDefault, platformSample.sample.cmSampleBuffer, &rawSampleCopy);
                auto sampleCopy = adoptCF(rawSampleCopy);
                PAL::CMSetAttachment(sampleCopy.get(), PAL::kCMSampleBufferAttachmentKey_PostNotificationWhenConsumed, (__bridge CFDictionaryRef)@{ (__bridge NSString *)PAL::kCMSampleBufferAttachmentKey_PostNotificationWhenConsumed : @YES }, kCMAttachmentMode_ShouldNotPropagate);
                [m_displayLayer enqueueSampleBuffer:sampleCopy.get()];
#if PLATFORM(IOS_FAMILY)
                player->setHasAvailableVideoFrame(true);
#endif
            } else {

                [m_displayLayer enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
                [m_displayLayer prerollDecodeWithCompletionHandler:[this, logSiteIdentifier, weakThis = WeakPtr { *this }] (BOOL success) mutable {
                    callOnMainThread([this, logSiteIdentifier, weakThis = WTFMove(weakThis), success] () {
                        if (!weakThis)
                            return;
                        
                        if (!success) {
                            ERROR_LOG(logSiteIdentifier, "prerollDecodeWithCompletionHandler failed");
                            return;
                        }

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
        if (auto player = this->player(); player && !sample->isNonDisplaying())
            player->setHasAvailableAudioSample(renderer.get(), true);
    }
}

void SourceBufferPrivateAVFObjC::bufferWasConsumed()
{
    DEBUG_LOG(LOGIDENTIFIER);

    if (auto player = this->player())
        player->setHasAvailableVideoFrame(true);
}

bool SourceBufferPrivateAVFObjC::isReadyForMoreSamples(const AtomString& trackIDString)
{
    auto trackID = parseIntegerAllowingTrailingJunk<uint64_t>(trackIDString).value_or(0);
    if (trackID == m_enabledVideoTrackID) {
#if PLATFORM(IOS_FAMILY)
        if (m_displayLayerWasInterrupted)
            return false;
#endif

        if (m_decompressionSession)
            return m_decompressionSession->isReadyForMoreMediaData();

        return [m_displayLayer isReadyForMoreMediaData];
    }

    if (m_audioRenderers.contains(trackID))
        return [m_audioRenderers.get(trackID) isReadyForMoreMediaData];

    return false;
}

MediaTime SourceBufferPrivateAVFObjC::timeFudgeFactor() const
{
    if (m_mediaSource)
        return m_mediaSource->timeFudgeFactor();

    return SourceBufferPrivate::timeFudgeFactor();
}

void SourceBufferPrivateAVFObjC::setActive(bool isActive)
{
    ALWAYS_LOG(LOGIDENTIFIER, isActive);
    m_isActive = isActive;
    if (m_mediaSource)
        m_mediaSource->sourceBufferPrivateDidChangeActiveState(this, isActive);
}

bool SourceBufferPrivateAVFObjC::isActive() const
{
    return m_isActive;
}

void SourceBufferPrivateAVFObjC::willSeek()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    // Set seeking to true so that no samples will be re-enqueued between
    // now and when seekToTime() is called. Without this, the AVSBDL may
    // request new samples mid seek, causing incorrect samples to be displayed
    // during a seek operation.
    m_seeking = true;
    flush();
}

void SourceBufferPrivateAVFObjC::seekToTime(const MediaTime& time)
{
    m_seeking = false;
    SourceBufferPrivate::seekToTime(time);
}
FloatSize SourceBufferPrivateAVFObjC::naturalSize()
{
    return valueOrDefault(m_cachedSize);
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

    if (trackIsBlocked(trackID))
        return;

    provideMediaData(AtomString::number(trackID));
}

void SourceBufferPrivateAVFObjC::notifyClientWhenReadyForMoreSamples(const AtomString& trackIDString)
{
    auto trackID = parseIntegerAllowingTrailingJunk<uint64_t>(trackIDString).value_or(0);
    if (trackID == m_enabledVideoTrackID) {
        if (m_decompressionSession) {
            m_decompressionSession->requestMediaDataWhenReady([weakThis = WeakPtr { *this }, trackID] {
                if (weakThis)
                    weakThis->didBecomeReadyForMoreSamples(trackID);
            });
        }
        if (m_displayLayer) {
            WeakPtr weakThis { *this };
            [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
                if (weakThis)
                    weakThis->didBecomeReadyForMoreSamples(trackID);
            }];
        }
    } else if (m_audioRenderers.contains(trackID)) {
        WeakPtr weakThis { *this };
        [m_audioRenderers.get(trackID) requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
            if (weakThis)
                weakThis->didBecomeReadyForMoreSamples(trackID);
        }];
    }
}

bool SourceBufferPrivateAVFObjC::canSetMinimumUpcomingPresentationTime(const AtomString& trackIDString) const
{
    return parseIntegerAllowingTrailingJunk<uint64_t>(trackIDString).value_or(0) == m_enabledVideoTrackID
        && [PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(expectMinimumUpcomingSampleBufferPresentationTime:)];
}

void SourceBufferPrivateAVFObjC::setMinimumUpcomingPresentationTime(const AtomString& trackIDString, const MediaTime& presentationTime)
{
    ASSERT(canSetMinimumUpcomingPresentationTime(trackIDString));
    if (canSetMinimumUpcomingPresentationTime(trackIDString))
        [m_displayLayer expectMinimumUpcomingSampleBufferPresentationTime:PAL::toCMTime(presentationTime)];
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
    return MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(parameters) != MediaPlayer::SupportsType::IsNotSupported;
}

bool SourceBufferPrivateAVFObjC::isSeeking() const
{
    return m_seeking;
}

MediaTime SourceBufferPrivateAVFObjC::currentMediaTime() const
{
    if (!m_mediaSource)
        return { };

    return m_mediaSource->currentMediaTime();
}

MediaTime SourceBufferPrivateAVFObjC::duration() const
{
    if (!m_mediaSource)
        return { };

    return m_mediaSource->duration();
}

void SourceBufferPrivateAVFObjC::setVideoLayer(AVSampleBufferDisplayLayer* layer)
{
    if (layer == m_displayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "!!layer = ", !!layer);
    ASSERT(!layer || !m_decompressionSession || hasSelectedVideo());

    if (m_displayLayer) {
        [m_displayLayer flush];
        [m_displayLayer stopRequestingMediaData];
        [m_errorListener stopObservingLayer:m_displayLayer.get()];

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        if (m_cdmInstance && sampleBufferRenderersSupportKeySession())
            [m_cdmInstance->contentKeySession() removeContentKeyRecipient:m_displayLayer.get()];
#endif
    }

    m_displayLayer = layer;

    if (m_displayLayer) {
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        if (m_cdmInstance && sampleBufferRenderersSupportKeySession())
            [m_cdmInstance->contentKeySession() addContentKeyRecipient:m_displayLayer.get()];
#endif

        WeakPtr weakThis { *this };
        [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
            if (weakThis)
                weakThis->didBecomeReadyForMoreSamples(m_enabledVideoTrackID);
        }];
        [m_errorListener beginObservingLayer:m_displayLayer.get()];
        reenqueSamples(AtomString::number(m_enabledVideoTrackID));
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

    m_decompressionSession->requestMediaDataWhenReady([weakThis = WeakPtr { *this }] {
        if (weakThis)
            weakThis->didBecomeReadyForMoreSamples(weakThis->m_enabledVideoTrackID);
    });
    m_decompressionSession->notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }] {
        if (weakThis && weakThis->player())
            weakThis->player()->setHasAvailableVideoFrame(true);
    });
    reenqueSamples(AtomString::number(m_enabledVideoTrackID));
}

MediaPlayerPrivateMediaSourceAVFObjC* SourceBufferPrivateAVFObjC::player() const
{
    return m_mediaSource ? m_mediaSource->player() : nullptr;
}


#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateAVFObjC::logChannel() const
{
    return LogMediaSource;
}
#endif

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
