/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
#import <wtf/NativePromise.h>
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
- (void)expectMinimumUpcomingSampleBufferPresentationTime:(CMTime)minimumUpcomingPresentationTime;
- (void)resetUpcomingSampleBufferPresentationTimeExpectations;
@end

@interface AVSampleBufferDisplayLayer (WebCoreSampleBufferKeySession) <AVContentKeyRecipient>
@end

@interface AVSampleBufferAudioRenderer (WebCoreSampleBufferKeySession) <AVContentKeyRecipient>
@end

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
// FIXME (117934497): Remove staging code once -[AVSampleBufferDisplayLayer isReadyForDisplay] is available in SDKs used by WebKit builders
@interface AVSampleBufferDisplayLayer (Staging_113656776)
@property (nonatomic, readonly, getter=isReadyForDisplay) BOOL readyForDisplay;
@end
#endif

static NSString * const errorKeyPath = @"error";
static NSString * const outputObscuredDueToInsufficientExternalProtectionKeyPath = @"outputObscuredDueToInsufficientExternalProtection";

static void* errorContext = &errorContext;
static void* outputObscuredDueToInsufficientExternalProtectionContext = &outputObscuredDueToInsufficientExternalProtectionContext;

@interface WebAVSampleBufferListener : NSObject {
    ThreadSafeWeakPtr<WebCore::SourceBufferPrivateAVFObjC> _parent;
    Vector<RetainPtr<AVSampleBufferDisplayLayer>> _layers;
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    Vector<RetainPtr<AVSampleBufferAudioRenderer>> _renderers;
ALLOW_NEW_API_WITHOUT_GUARDS_END
}

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithParent:(WebCore::SourceBufferPrivateAVFObjC&)parent NS_DESIGNATED_INITIALIZER;
- (void)invalidate;
- (void)beginObservingLayer:(AVSampleBufferDisplayLayer *)layer;
- (void)stopObservingLayer:(AVSampleBufferDisplayLayer *)layer;
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
- (void)beginObservingRenderer:(AVSampleBufferAudioRenderer *)renderer;
- (void)stopObservingRenderer:(AVSampleBufferAudioRenderer *)renderer;
ALLOW_NEW_API_WITHOUT_GUARDS_END
@end

@implementation WebAVSampleBufferListener

- (instancetype)initWithParent:(WebCore::SourceBufferPrivateAVFObjC&)parent
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
    ASSERT(isMainThread());

    _parent = nullptr;

    for (auto& layer : _layers) {
        [layer removeObserver:self forKeyPath:errorKeyPath];
        [layer removeObserver:self forKeyPath:outputObscuredDueToInsufficientExternalProtectionKeyPath];
    }
    _layers.clear();

    for (auto& renderer : _renderers)
        [renderer removeObserver:self forKeyPath:errorKeyPath];
    _renderers.clear();

    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)beginObservingLayer:(AVSampleBufferDisplayLayer *)layer
{
    ASSERT(isMainThread());
    ASSERT(!_layers.contains(layer));

    _layers.append(layer);
    [layer addObserver:self forKeyPath:errorKeyPath options:NSKeyValueObservingOptionNew context:errorContext];
    [layer addObserver:self forKeyPath:outputObscuredDueToInsufficientExternalProtectionKeyPath options:NSKeyValueObservingOptionNew context:outputObscuredDueToInsufficientExternalProtectionContext];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(layerFailedToDecode:) name:AVSampleBufferDisplayLayerFailedToDecodeNotification object:layer];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(layerRequiresFlushToResumeDecodingChanged:) name:AVSampleBufferDisplayLayerRequiresFlushToResumeDecodingDidChangeNotification object:layer];
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
    // FIXME (117934497): Remove staging code once -[AVSampleBufferDisplayLayer isReadyForDisplay] is available in SDKs used by WebKit builders
    if (PAL::canLoad_AVFoundation_AVSampleBufferDisplayLayerReadyForDisplayDidChangeNotification())
        [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(layerReadyForDisplayChanged:) name:AVSampleBufferDisplayLayerReadyForDisplayDidChangeNotification object:layer];
#endif
}

- (void)stopObservingLayer:(AVSampleBufferDisplayLayer *)layer
{
    ASSERT(isMainThread());
    ASSERT(_layers.contains(layer));

    [layer removeObserver:self forKeyPath:errorKeyPath];
    [layer removeObserver:self forKeyPath:outputObscuredDueToInsufficientExternalProtectionKeyPath];
    _layers.remove(_layers.find(layer));

    [NSNotificationCenter.defaultCenter removeObserver:self name:AVSampleBufferDisplayLayerFailedToDecodeNotification object:layer];
    [NSNotificationCenter.defaultCenter removeObserver:self name:AVSampleBufferDisplayLayerRequiresFlushToResumeDecodingDidChangeNotification object:layer];
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
    // FIXME (117934497): Remove staging code once -[AVSampleBufferDisplayLayer isReadyForDisplay] is available in SDKs used by WebKit builders
    if (PAL::canLoad_AVFoundation_AVSampleBufferDisplayLayerReadyForDisplayDidChangeNotification())
        [NSNotificationCenter.defaultCenter removeObserver:self name:AVSampleBufferDisplayLayerReadyForDisplayDidChangeNotification object:layer];
#endif
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
- (void)beginObservingRenderer:(AVSampleBufferAudioRenderer *)renderer
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    ASSERT(isMainThread());
    ASSERT(!_renderers.contains(renderer));

    _renderers.append(renderer);
    [renderer addObserver:self forKeyPath:errorKeyPath options:NSKeyValueObservingOptionNew context:errorContext];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(audioRendererWasAutomaticallyFlushed:) name:AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification object:renderer];
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
- (void)stopObservingRenderer:(AVSampleBufferAudioRenderer *)renderer
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    ASSERT(isMainThread());
    ASSERT(_renderers.contains(renderer));

    [renderer removeObserver:self forKeyPath:errorKeyPath];
    [NSNotificationCenter.defaultCenter removeObserver:self name:AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification object:renderer];

    _renderers.remove(_renderers.find(renderer));
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void*)context
{
    if (context == errorContext) {
        RetainPtr error = dynamic_objc_cast<NSError>([change valueForKey:NSKeyValueChangeNewKey]);
        if (!error)
            return;

        if ([object isKindOfClass:PAL::getAVSampleBufferDisplayLayerClass()]) {
            RetainPtr layer = (AVSampleBufferDisplayLayer *)object;

            ensureOnMainThread([self, protectedSelf = RetainPtr { self }, layer = WTFMove(layer), error = WTFMove(error)] {
                ASSERT(_layers.contains(layer.get()));
                if (auto parent = _parent.get())
                    parent->layerDidReceiveError(layer.get(), error.get());
            });
            return;
        }

        if ([object isKindOfClass:PAL::getAVSampleBufferAudioRendererClass()]) {
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
            RetainPtr renderer = (AVSampleBufferAudioRenderer *)object;
ALLOW_NEW_API_WITHOUT_GUARDS_END

            ensureOnMainThread([self, protectedSelf = RetainPtr { self }, renderer = WTFMove(renderer), error = WTFMove(error)] {
                ASSERT(_renderers.contains(renderer.get()));
                if (auto parent = _parent.get())
                    parent->rendererDidReceiveError(renderer.get(), error.get());
            });
            return;
        }

        ASSERT_NOT_REACHED_WITH_MESSAGE("Unexpected kind of object while observing errorContext");
        return;
    }

    if (context == outputObscuredDueToInsufficientExternalProtectionContext) {
        RetainPtr layer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(object, PAL::getAVSampleBufferDisplayLayerClass());
        BOOL isObscured = [[change valueForKey:NSKeyValueChangeNewKey] boolValue];

        ensureOnMainThread([self, protectedSelf = RetainPtr { self }, layer = WTFMove(layer), isObscured] {
            ASSERT(_layers.contains(layer.get()));
            if (auto parent = _parent.get())
                parent->outputObscuredDueToInsufficientExternalProtectionChanged(isObscured);
        });
        return;
    }

    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

- (void)layerFailedToDecode:(NSNotification *)notification
{
    RetainPtr layer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(notification.object, PAL::getAVSampleBufferDisplayLayerClass());
    RetainPtr error = dynamic_objc_cast<NSError>([notification.userInfo valueForKey:AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey]);

    ensureOnMainThread([self, protectedSelf = RetainPtr { self }, layer = WTFMove(layer), error = WTFMove(error)] {
        if (!_layers.contains(layer.get()))
            return;
        if (auto parent = _parent.get())
            parent->layerDidReceiveError(layer.get(), error.get());
    });
}

- (void)layerRequiresFlushToResumeDecodingChanged:(NSNotification *)notification
{
    RetainPtr layer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(notification.object, PAL::getAVSampleBufferDisplayLayerClass());
    BOOL requiresFlush = [layer requiresFlushToResumeDecoding];

    ensureOnMainThread([self, protectedSelf = RetainPtr { self }, layer = WTFMove(layer), requiresFlush] {
        if (!_layers.contains(layer.get()))
            return;
        if (auto parent = _parent.get())
            parent->layerRequiresFlushToResumeDecodingChanged(layer.get(), requiresFlush);
    });
}

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
- (void)layerReadyForDisplayChanged:(NSNotification *)notification
{
    RetainPtr layer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(notification.object, PAL::getAVSampleBufferDisplayLayerClass());
    BOOL isReadyForDisplay = [layer isReadyForDisplay];

    ensureOnMainThread([self, protectedSelf = RetainPtr { self }, layer = WTFMove(layer), isReadyForDisplay] {
        if (!_layers.contains(layer.get()))
            return;
        if (auto parent = _parent.get())
            parent->layerReadyForDisplayChanged(layer.get(), isReadyForDisplay);
    });
}
#endif

- (void)audioRendererWasAutomaticallyFlushed:(NSNotification *)notification
{
    RetainPtr renderer = dynamic_objc_cast<AVSampleBufferAudioRenderer>(notification.object, PAL::getAVSampleBufferAudioRendererClass());
    CMTime flushTime = [[notification.userInfo valueForKey:AVSampleBufferAudioRendererFlushTimeKey] CMTimeValue];

    ensureOnMainThread([self, protectedSelf = RetainPtr { self }, renderer = WTFMove(renderer), flushTime] {
        if (!_renderers.contains(renderer.get()))
            return;
        if (auto parent = _parent.get())
            parent->rendererWasAutomaticallyFlushed(renderer.get(), flushTime);
    });
}

@end

namespace WebCore {

#pragma mark -
#pragma mark SourceBufferPrivateAVFObjC

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

Ref<SourceBufferPrivateAVFObjC> SourceBufferPrivateAVFObjC::create(MediaSourcePrivateAVFObjC& parent, Ref<SourceBufferParser>&& parser)
{
    return adoptRef(*new SourceBufferPrivateAVFObjC(parent, WTFMove(parser)));
}

SourceBufferPrivateAVFObjC::SourceBufferPrivateAVFObjC(MediaSourcePrivateAVFObjC& parent, Ref<SourceBufferParser>&& parser)
    : SourceBufferPrivate(parent)
    , m_parser(WTFMove(parser))
    , m_listener(adoptNS([[WebAVSampleBufferListener alloc] initWithParent:*this]))
    , m_appendQueue(WorkQueue::create("SourceBufferPrivateAVFObjC data parser queue"))
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    , m_keyStatusesChangedObserver(makeUniqueRef<Observer<void()>>([this] { keyStatusesChanged(); }))
    , m_streamDataParser(is<SourceBufferParserAVFObjC>(m_parser) ? downcast<SourceBufferParserAVFObjC>(m_parser)->streamDataParser() : nil)
#endif
#if !RELEASE_LOG_DISABLED
    , m_logger(parent.logger())
    , m_logIdentifier(parent.nextSourceBufferLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if !RELEASE_LOG_DISABLED
    m_parser->setLogger(m_logger.get(), m_logIdentifier);
#endif
}

SourceBufferPrivateAVFObjC::~SourceBufferPrivateAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    destroyStreamDataParser();
    destroyRenderers();
    clearTracks();

    abort();
}

void SourceBufferPrivateAVFObjC::setTrackChangeCallbacks(const InitializationSegment& segment, bool initialized)
{
    for (auto& videoTrackInfo : segment.videoTracks) {
        videoTrackInfo.track->setSelectedChangedCallback([weakThis = ThreadSafeWeakPtr { *this }, this, initialized] (VideoTrackPrivate& track, bool selected) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (initialized) {
                trackDidChangeSelected(track, selected);
                return;
            }
            m_pendingTrackChangeTasks.append([weakThis, trackRef = Ref { track }, selected] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->trackDidChangeSelected(trackRef, selected);
            });
        });
    }

    for (auto& audioTrackInfo : segment.audioTracks) {
        audioTrackInfo.track->setEnabledChangedCallback([weakThis = ThreadSafeWeakPtr { *this }, this, initialized] (AudioTrackPrivate& track, bool enabled) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (initialized) {
                trackDidChangeEnabled(track, enabled);
                return;
            }

            m_pendingTrackChangeTasks.append([weakThis, trackRef = Ref { track }, enabled] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->trackDidChangeEnabled(trackRef, enabled);
            });
        });
    }
}

bool SourceBufferPrivateAVFObjC::precheckInitialisationSegment(const InitializationSegment& segment)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (auto player = this->player(); player && player->shouldCheckHardwareSupport()) {
        for (auto& info : segment.videoTracks) {
            auto codec = FourCC::fromString(info.description->codec());
            if (!codec)
                continue;
            if (!codecsMeetHardwareDecodeRequirements({ { *codec } }, player->mediaContentTypesRequiringHardwareSupport()))
                return false;
        }
    }

    m_protectedTrackInitDataMap = std::exchange(m_pendingProtectedTrackInitDataMap, { });

    for (auto& videoTrackInfo : segment.videoTracks)
        m_videoTracks.set(videoTrackInfo.track->id(), videoTrackInfo.track);

    for (auto& audioTrackInfo : segment.audioTracks)
        m_audioTracks.set(audioTrackInfo.track->id(), audioTrackInfo.track);

    setTrackChangeCallbacks(segment, false);

    return true;
}

void SourceBufferPrivateAVFObjC::processInitialisationSegment(std::optional<InitializationSegment>&& segment)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!segment) {
        ERROR_LOG(LOGIDENTIFIER, "failed to process initialization segment");
        m_pendingTrackChangeTasks.clear();
        return;
    }

    auto tasks = std::exchange(m_pendingTrackChangeTasks, { });
    for (auto& task : tasks)
        task();

    setTrackChangeCallbacks(*segment, true);

    if (auto player = this->player())
        player->characteristicsChanged();

    ALWAYS_LOG(LOGIDENTIFIER, "initialization segment was processed");
}

void SourceBufferPrivateAVFObjC::didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&& mediaSample, uint64_t trackId, const String& mediaType)
{
    UNUSED_PARAM(mediaType);
    UNUSED_PARAM(trackId);

    didReceiveSample(WTFMove(mediaSample));
}

bool SourceBufferPrivateAVFObjC::isMediaSampleAllowed(const MediaSample& sample) const
{
    // FIXME(125161): We don't handle text tracks, and passing this sample up to SourceBuffer
    // will just confuse its state. Drop this sample until we can handle text tracks properly.
    auto trackId = parseIntegerAllowingTrailingJunk<uint64_t>(sample.trackID()).value_or(0);
    return trackId == m_enabledVideoTrackID || m_audioRenderers.contains(trackId);
}

void SourceBufferPrivateAVFObjC::processFormatDescriptionForTrackId(Ref<TrackInfo>&& formatDescription, uint64_t trackId)
{
    if (auto videoDescription = dynamicDowncast<VideoInfo>(formatDescription)) {
        auto result = m_videoTracks.find(AtomString::number(trackId));
        if (result != m_videoTracks.end())
            result->value->setFormatDescription(videoDescription.releaseNonNull());
        return;
    }

    if (auto audioDescription = dynamicDowncast<AudioInfo>(formatDescription)) {
        auto result = m_audioTracks.find(AtomString::number(trackId));
        if (result != m_audioTracks.end())
            result->value->setFormatDescription(audioDescription.releaseNonNull());
    }
}

void SourceBufferPrivateAVFObjC::didProvideContentKeyRequestInitializationDataForTrackID(Ref<SharedBuffer>&& initData, uint64_t trackID, Box<BinarySemaphore> hasSessionSemaphore)
{
    RefPtr player = this->player();
    if (!player)
        return;
    RefPtr mediaSource = downcast<MediaSourcePrivateAVFObjC>(m_mediaSource.get());
    if (!mediaSource)
        return;
#if HAVE(AVCONTENTKEYSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    ALWAYS_LOG(LOGIDENTIFIER, "track = ", trackID);

    m_protectedTrackID = trackID;
    m_initData = WTFMove(initData);
    mediaSource->sourceBufferKeyNeeded(this, *m_initData);

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

Ref<MediaPromise> SourceBufferPrivateAVFObjC::appendInternal(Ref<SharedBuffer>&& data)
{
    ALWAYS_LOG(LOGIDENTIFIER, "data length = ", data->size());

    ASSERT(!m_hasSessionSemaphore);
    ASSERT(!m_abortSemaphore);

    m_abortSemaphore = Box<Semaphore>::create(0);
    return invokeAsync(m_appendQueue, [data = WTFMove(data), parser = m_parser, weakThis = ThreadSafeWeakPtr { *this }, abortSemaphore = m_abortSemaphore]() mutable {
        parser->setDidParseInitializationDataCallback([weakThis] (InitializationSegment&& segment) {
            ASSERT(isMainThread());
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didReceiveInitializationSegment(WTFMove(segment));
        });

        parser->setDidProvideMediaDataCallback([weakThis] (Ref<MediaSampleAVFObjC>&& sample, uint64_t trackId, const String& mediaType) {
            ASSERT(isMainThread());
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didProvideMediaDataForTrackId(WTFMove(sample), trackId, mediaType);
        });

        parser->setDidUpdateFormatDescriptionForTrackIDCallback([weakThis] (Ref<TrackInfo>&& formatDescription, uint64_t trackId) {
            ASSERT(isMainThread());
            if (RefPtr protectedThis = weakThis.get(); protectedThis)
                protectedThis->didUpdateFormatDescriptionForTrackId(WTFMove(formatDescription), trackId);
        });


        parser->setWillProvideContentKeyRequestInitializationDataForTrackIDCallback([abortSemaphore] (uint64_t) mutable {
            // We must call synchronously to the main thread, as the AVStreamSession must be associated
            // with the streamDataParser before the delegate method returns.
            Box<BinarySemaphore> respondedSemaphore = Box<BinarySemaphore>::create();
            callOnMainThread([respondedSemaphore]() {
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

        parser->setDidProvideContentKeyRequestInitializationDataForTrackIDCallback([weakThis, abortSemaphore](Ref<SharedBuffer>&& initData, uint64_t trackID) mutable {
            // Called on the data parser queue.
            Box<BinarySemaphore> hasSessionSemaphore = Box<BinarySemaphore>::create();
            callOnMainThread([weakThis, initData = WTFMove(initData), trackID, hasSessionSemaphore] () mutable {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->didProvideContentKeyRequestInitializationDataForTrackID(WTFMove(initData), trackID, hasSessionSemaphore);
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

        parser->setDidProvideContentKeyRequestIdentifierForTrackIDCallback([weakThis] (Ref<SharedBuffer>&& initData, uint64_t trackID) {
            ASSERT(isMainThread());
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didProvideContentKeyRequestInitializationDataForTrackID(WTFMove(initData), trackID, nullptr);
        });

        return MediaPromise::createAndSettle(parser->appendData(WTFMove(data)));
    })->whenSettled(RunLoop::current(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->appendCompleted(!!result);
        return MediaPromise::createAndSettle(WTFMove(result));
    });
}

void SourceBufferPrivateAVFObjC::appendCompleted(bool success)
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

    if (auto player = this->player(); player && success)
        player->setLoadingProgresssed(true);
}

void SourceBufferPrivateAVFObjC::abort()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    // The parsing queue may be blocked waiting for the main thread to provide it an AVStreamSession.
    if (m_hasSessionSemaphore) {
        m_hasSessionSemaphore->signal();
        m_hasSessionSemaphore = nullptr;
    }
    if (m_abortSemaphore) {
        m_abortSemaphore->signal();
        m_abortSemaphore = nullptr;
    }
    SourceBufferPrivate::abort();
}

void SourceBufferPrivateAVFObjC::resetParserStateInternal()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_appendQueue->dispatch([parser = m_parser] {
        parser->resetParserState();
    });
}

void SourceBufferPrivateAVFObjC::destroyStreamDataParser()
{
    auto parser = this->streamDataParser();
    if (!parser)
        return;
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
        [m_listener stopObservingRenderer:renderer.get()];

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        if (m_cdmInstance && sampleBufferRenderersSupportKeySession())
            [m_cdmInstance->contentKeySession() removeContentKeyRecipient:renderer.get()];
#endif
    }

    [m_listener invalidate];
    m_listener = nullptr;

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

    destroyStreamDataParser();
    destroyRenderers();

    SourceBufferPrivate::removedFromMediaSource();
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
        if (m_decompressionSession)
            m_decompressionSession->stopRequestingMediaData();
    } else if (selected) {
        m_enabledVideoTrackID = trackID;
        if (m_decompressionSession) {
            m_decompressionSession->requestMediaDataWhenReady([weakThis = ThreadSafeWeakPtr { *this }, trackID] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->didBecomeReadyForMoreSamples(trackID);
            });
        }
    }

    if (RefPtr player = this->player())
        player->needsVideoLayerChanged();

    if (RefPtr mediaSource = downcast<MediaSourcePrivateAVFObjC>(m_mediaSource.get()))
        mediaSource->hasSelectedVideoChanged(*this);
}

void SourceBufferPrivateAVFObjC::trackDidChangeEnabled(AudioTrackPrivate& track, bool enabled)
{
    auto trackID = parseIntegerAllowingTrailingJunk<uint64_t>(track.id()).value_or(0);

    ALWAYS_LOG(LOGIDENTIFIER, "audio trackID = ", trackID, ", enabled = ", enabled);

    if (!enabled) {
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
        RetainPtr<AVSampleBufferAudioRenderer> renderer = m_audioRenderers.get(trackID);
ALLOW_NEW_API_WITHOUT_GUARDS_END
        if (RefPtr player = this->player())
            player->removeAudioRenderer(renderer.get());
    } else {
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
        RetainPtr<AVSampleBufferAudioRenderer> renderer;
ALLOW_NEW_API_WITHOUT_GUARDS_END
        if (!m_audioRenderers.contains(trackID)) {
            renderer = adoptNS([PAL::allocAVSampleBufferAudioRendererInstance() init]);

            if (!renderer) {
                ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferAudioRenderer init] returned nil! bailing!");
                if (RefPtr mediaSourcePrivate = downcast<MediaSourcePrivateAVFObjC>(m_mediaSource.get()))
                    mediaSourcePrivate->failedToCreateRenderer(MediaSourcePrivateAVFObjC::RendererType::Audio);
                if (RefPtr player = this->player())
                    player->setNetworkState(MediaPlayer::NetworkState::DecodeError);
                return;
            }

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        if (m_cdmInstance && sampleBufferRenderersSupportKeySession())
            [m_cdmInstance->contentKeySession() addContentKeyRecipient:renderer.get()];
#endif

            ThreadSafeWeakPtr weakThis { *this };
            [renderer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->didBecomeReadyForMoreSamples(trackID);
            }];
            m_audioRenderers.set(trackID, renderer);
            [m_listener beginObservingRenderer:renderer.get()];
        } else
            renderer = m_audioRenderers.get(trackID);

        if (RefPtr player = this->player())
            player->addAudioRenderer(renderer.get());
    }
}
#if (ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)) || ENABLE(LEGACY_ENCRYPTED_MEDIA)
void SourceBufferPrivateAVFObjC::setCDMSession(LegacyCDMSession* session)
{
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (session == m_session)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_session)
        m_session->removeSourceBuffer(this);

    m_session = toCDMSessionMediaSourceAVFObjC(session);

    if (m_session) {
        m_session->addSourceBuffer(this);
        if (m_hasSessionSemaphore) {
            m_hasSessionSemaphore->signal();
            m_hasSessionSemaphore = nullptr;
        }

        if (m_hdcpError) {
            callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
                if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_session && protectedThis->m_hdcpError) {

                    bool ignored = false;
                    protectedThis->m_session->layerDidReceiveError(nullptr, protectedThis->m_hdcpError.get(), ignored);
                }
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
#endif // (ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)) || ENABLE(LEGACY_ENCRYPTED_MEDIA)

bool SourceBufferPrivateAVFObjC::requiresFlush() const
{
#if PLATFORM(IOS_FAMILY)
    if (m_displayLayerWasInterrupted)
        return true;
#endif

    return m_layerRequiresFlush;
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

void SourceBufferPrivateAVFObjC::flushIfNeeded()
{
    if (!requiresFlush())
        return;

#if PLATFORM(IOS_FAMILY)
    m_displayLayerWasInterrupted = false;
#endif
    m_layerRequiresFlush = false;
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
    ERROR_LOG(LOGIDENTIFIER, error);

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

    if (RefPtr client = this->client())
        client->sourceBufferPrivateDidReceiveRenderingError(errorCode);
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
    if (RefPtr mediaSource = downcast<MediaSourcePrivateAVFObjC>(m_mediaSource.get()); mediaSource && mediaSource->cdmInstance()) {
        mediaSource->outputObscuredDueToInsufficientExternalProtectionChanged(obscured);
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
    ERROR_LOG(LOGIDENTIFIER, error);

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

void SourceBufferPrivateAVFObjC::layerRequiresFlushToResumeDecodingChanged(AVSampleBufferDisplayLayer *layer, bool requiresFlush)
{
    if (layer != m_displayLayer || m_layerRequiresFlush == requiresFlush)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, requiresFlush);
    m_layerRequiresFlush = requiresFlush;
}

void SourceBufferPrivateAVFObjC::layerReadyForDisplayChanged(AVSampleBufferDisplayLayer *layer, bool isReadyForDisplay)
{
    if (layer != m_displayLayer || !isReadyForDisplay)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    if (RefPtr player = this->player())
        player->setHasAvailableVideoFrame(true);
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
        m_decompressionSession->notifyWhenHasAvailableVideoFrame([weakThis = ThreadSafeWeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get()) {
                if (RefPtr player = protectedThis->player())
                    player->setHasAvailableVideoFrame(true);
            }
        });
    }

    m_cachedSize = std::nullopt;

    if (RefPtr player = this->player()) {
        player->setHasAvailableVideoFrame(false);
        player->flushPendingSizeChanges();
    }
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void SourceBufferPrivateAVFObjC::flushAudio(AVSampleBufferAudioRenderer *renderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    [renderer flush];

    if (RefPtr player = this->player())
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
    // if sample is unencrytped: enqueue sample
    if (!sample.isProtected())
        return true;

    // If sample buffers don't support AVContentKeySession: enqueue sample
    if (!sampleBufferRenderersSupportKeySession())
        return true;

    // if sample is encrypted, but we are not attached to a CDM: do not enqueue sample.
    if (!m_cdmInstance)
        return false;

    // DecompressionSessions doesn't support encrypted media.
    if (trackID == m_enabledVideoTrackID && !m_displayLayer)
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
    if (RefPtr sampleAVFObjC = dynamicDowncast<MediaSampleAVFObjC>(WTFMove(sample))) {
        if (!canEnqueueSample(trackID, *sampleAVFObjC)) {
            m_blockedSamples.append({ trackID, sampleAVFObjC.releaseNonNull() });
            return;
        }

        enqueueSample(sampleAVFObjC.releaseNonNull(), trackID);
    }
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

        RefPtr player = this->player();
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

        enqueueSampleBuffer(sample.get());

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
        if (RefPtr player = this->player(); player && !sample->isNonDisplaying())
            player->setHasAvailableAudioSample(renderer.get(), true);
    }
}

void SourceBufferPrivateAVFObjC::enqueueSampleBuffer(MediaSampleAVFObjC& sample)
{
    [m_displayLayer enqueueSampleBuffer:sample.platformSample().sample.cmSampleBuffer];

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
    // FIXME (117934497): Remove staging code once -[AVSampleBufferDisplayLayer isReadyForDisplay] is available in SDKs used by WebKit builders
    if ([m_displayLayer respondsToSelector:@selector(isReadyForDisplay)])
        return;
#endif

    RefPtr player = this->player();
    if (!player || player->hasAvailableVideoFrame() || sample.isNonDisplaying())
        return;

    DEBUG_LOG(LOGIDENTIFIER, "adding buffer attachment");

    [m_displayLayer prerollDecodeWithCompletionHandler:[this, weakThis = ThreadSafeWeakPtr { *this }, logSiteIdentifier = LOGIDENTIFIER] (BOOL success) mutable {
        callOnMainThread([this, weakThis = WTFMove(weakThis), logSiteIdentifier, success] () {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (!success) {
                ERROR_LOG(logSiteIdentifier, "prerollDecodeWithCompletionHandler failed");
                return;
            }

            layerReadyForDisplayChanged(m_displayLayer.get(), true);
        });
    }];
}

bool SourceBufferPrivateAVFObjC::isReadyForMoreSamples(const AtomString& trackIDString)
{
    auto trackID = parseIntegerAllowingTrailingJunk<uint64_t>(trackIDString).value_or(0);
    if (trackID == m_enabledVideoTrackID) {
        if (requiresFlush())
            return false;

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
    if (RefPtr mediaSource = m_mediaSource.get())
        return mediaSource->timeFudgeFactor();

    return SourceBufferPrivate::timeFudgeFactor();
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
    m_seeking = false; // m_seeking must be set to false first, otherwise reenqueueMediaForTime will drop the sample.
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
            m_decompressionSession->requestMediaDataWhenReady([weakThis = ThreadSafeWeakPtr { *this }, trackID] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->didBecomeReadyForMoreSamples(trackID);
            });
        }
        if (m_displayLayer) {
            ThreadSafeWeakPtr weakThis { *this };
            [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->didBecomeReadyForMoreSamples(trackID);
            }];
        }
    } else if (m_audioRenderers.contains(trackID)) {
        ThreadSafeWeakPtr weakThis { *this };
        [m_audioRenderers.get(trackID) requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didBecomeReadyForMoreSamples(trackID);
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

void SourceBufferPrivateAVFObjC::setVideoLayer(AVSampleBufferDisplayLayer* layer)
{
    if (layer == m_displayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "!!layer = ", !!layer);
    ASSERT(!layer || !m_decompressionSession || hasSelectedVideo());

    if (m_displayLayer) {
        [m_displayLayer flush];
        [m_displayLayer stopRequestingMediaData];
        [m_listener stopObservingLayer:m_displayLayer.get()];

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

        ThreadSafeWeakPtr weakThis { *this };
        [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didBecomeReadyForMoreSamples(m_enabledVideoTrackID);
        }];
        [m_listener beginObservingLayer:m_displayLayer.get()];
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

    m_decompressionSession->requestMediaDataWhenReady([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->didBecomeReadyForMoreSamples(protectedThis->m_enabledVideoTrackID);
    });
    m_decompressionSession->notifyWhenHasAvailableVideoFrame([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get()) {
            if (auto player = protectedThis->player())
                player->setHasAvailableVideoFrame(true);
        }
    });
    reenqueSamples(AtomString::number(m_enabledVideoTrackID));
}

RefPtr<MediaPlayerPrivateMediaSourceAVFObjC> SourceBufferPrivateAVFObjC::player() const
{
    if (RefPtr mediaSource = downcast<MediaSourcePrivateAVFObjC>(m_mediaSource.get()))
        return mediaSource->player();
    return nullptr;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateAVFObjC::logChannel() const
{
    return LogMediaSource;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
