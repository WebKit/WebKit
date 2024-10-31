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
#import "VideoMediaSampleRenderer.h"
#import "VideoTrackPrivateMediaSourceAVFObjC.h"
#import "WebAVSampleBufferListener.h"
#import "WebCoreDecompressionSession.h"
#import "WebSampleBufferVideoRendering.h"
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
#import <wtf/cocoa/Entitlements.h>
#import <wtf/text/CString.h>

#pragma mark - Soft Linking

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (WebCoreSampleBufferKeySession) <AVContentKeyRecipient>
@end

@interface AVSampleBufferAudioRenderer (WebCoreSampleBufferKeySession) <AVContentKeyRecipient>
@end


namespace WebCore {

#pragma mark -
#pragma mark SourceBufferPrivateAVFObjC

static inline bool supportsAttachContentKey()
{
    static bool supportsAttachContentKey;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        supportsAttachContentKey = WTF::processHasEntitlement("com.apple.developer.web-browser-engine.rendering"_s) || WTF::processHasEntitlement("com.apple.private.coremedia.allow-fps-attachment"_s);
    });
    return supportsAttachContentKey;
}

static inline bool shouldAddContentKeyRecipients()
{
    return MediaSessionManagerCocoa::shouldUseModernAVContentKeySession() && !supportsAttachContentKey();
}

Ref<SourceBufferPrivateAVFObjC> SourceBufferPrivateAVFObjC::create(MediaSourcePrivateAVFObjC& parent, Ref<SourceBufferParser>&& parser)
{
    return adoptRef(*new SourceBufferPrivateAVFObjC(parent, WTFMove(parser)));
}

SourceBufferPrivateAVFObjC::SourceBufferPrivateAVFObjC(MediaSourcePrivateAVFObjC& parent, Ref<SourceBufferParser>&& parser)
    : SourceBufferPrivate(parent)
    , m_parser(WTFMove(parser))
    , m_listener(WebAVSampleBufferListener::create(*this))
    , m_appendQueue(WorkQueue::create("SourceBufferPrivateAVFObjC data parser queue"_s))
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    , m_keyStatusesChangedObserver(makeUniqueRef<Observer<void()>>([this] { tryToEnqueueBlockedSamples(); }))
    , m_streamDataParser([&] {
        auto* sourceBufferParser = dynamicDowncast<SourceBufferParserAVFObjC>(m_parser.get());
        return sourceBufferParser ? sourceBufferParser->streamDataParser() : nil;
    }())
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
    m_listener->invalidate();

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

bool SourceBufferPrivateAVFObjC::precheckInitializationSegment(const InitializationSegment& segment)
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
        m_videoTracks.try_emplace(videoTrackInfo.track->id(), videoTrackInfo.track);

    for (auto& audioTrackInfo : segment.audioTracks)
        m_audioTracks.try_emplace(audioTrackInfo.track->id(), audioTrackInfo.track);

    setTrackChangeCallbacks(segment, false);

    return true;
}

void SourceBufferPrivateAVFObjC::processInitializationSegment(std::optional<InitializationSegment>&& segment)
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

void SourceBufferPrivateAVFObjC::didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&& mediaSample, TrackID, const String&)
{
    didReceiveSample(WTFMove(mediaSample));
}

bool SourceBufferPrivateAVFObjC::isMediaSampleAllowed(const MediaSample& sample) const
{
    // FIXME(125161): We don't handle text tracks, and passing this sample up to SourceBuffer
    // will just confuse its state. Drop this sample until we can handle text tracks properly.
    auto trackID = sample.trackID();
    return isEnabledVideoTrackID(trackID) || audioRendererForTrackID(trackID);
}

void SourceBufferPrivateAVFObjC::updateTrackIds(Vector<std::pair<TrackID, TrackID>>&& trackIdPairs)
{
    for (auto& trackIdPair : trackIdPairs) {
        auto oldId = trackIdPair.first;
        auto newId = trackIdPair.second;
        ASSERT(oldId != newId);
        if (m_enabledVideoTrackID && *m_enabledVideoTrackID == oldId)
            m_enabledVideoTrackID = newId;
        if (m_protectedTrackID && *m_protectedTrackID == oldId)
            m_protectedTrackID = newId;
        auto audioRendererNode = m_audioRenderers.extract(oldId);
        if (!audioRendererNode)
            continue;
        audioRendererNode.key() = newId;
        m_audioRenderers.insert(WTFMove(audioRendererNode));
    }
    SourceBufferPrivate::updateTrackIds(WTFMove(trackIdPairs));
}

void SourceBufferPrivateAVFObjC::processFormatDescriptionForTrackId(Ref<TrackInfo>&& formatDescription, TrackID trackId)
{
    if (auto videoDescription = dynamicDowncast<VideoInfo>(formatDescription)) {
        auto result = m_videoTracks.find(trackId);
        if (result != m_videoTracks.end())
            result->second->setFormatDescription(videoDescription.releaseNonNull());
        return;
    }

    if (auto audioDescription = dynamicDowncast<AudioInfo>(formatDescription)) {
        auto result = m_audioTracks.find(trackId);
        if (result != m_audioTracks.end())
            result->second->setFormatDescription(audioDescription.releaseNonNull());
    }
}

void SourceBufferPrivateAVFObjC::didProvideContentKeyRequestInitializationDataForTrackID(Ref<SharedBuffer>&& initData, TrackID trackID, Box<BinarySemaphore> hasSessionSemaphore)
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
        if (MediaSessionManagerCocoa::shouldUseModernAVContentKeySession()) {
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
    AtomString initDataType = CDMPrivateFairPlayStreaming::sinfName();
#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
    if (!keyIDs) {
        keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsMpts(*m_initData);
        initDataType = CDMPrivateFairPlayStreaming::mptsName();
    }
#endif
    if (!keyIDs)
        return;

    m_pendingProtectedTrackInitDataMap.try_emplace(trackID, TrackInitData { m_initData.copyRef(), *keyIDs });

    if (m_cdmInstance) {
        if (auto instanceSession = m_cdmInstance->sessionForKeyIDs(keyIDs.value())) {
            if (MediaSessionManagerCocoa::shouldUseModernAVContentKeySession()) {
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
    player->initializationDataEncountered(initDataType, m_initData->tryCreateArrayBuffer());
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
    if (!m_protectedTrackID)
        return false;

    if (!isEnabledVideoTrackID(*m_protectedTrackID))
        return false;

    // When video content is protected and keys are assigned through
    // the renderers, decoding content through decompression sessions
    // will fail. In this scenario, ask the player to create a layer
    // instead.
    return MediaSessionManagerCocoa::shouldUseModernAVContentKeySession();
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

        parser->setDidProvideMediaDataCallback([weakThis] (Ref<MediaSampleAVFObjC>&& sample, TrackID trackId, const String& mediaType) {
            ASSERT(isMainThread());
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didProvideMediaDataForTrackId(WTFMove(sample), trackId, mediaType);
        });

        parser->setDidUpdateFormatDescriptionForTrackIDCallback([weakThis] (Ref<TrackInfo>&& formatDescription, TrackID trackId) {
            ASSERT(isMainThread());
            if (RefPtr protectedThis = weakThis.get(); protectedThis)
                protectedThis->didUpdateFormatDescriptionForTrackId(WTFMove(formatDescription), trackId);
        });

        parser->setWillProvideContentKeyRequestInitializationDataForTrackIDCallback([abortSemaphore] (TrackID) mutable {
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

        parser->setDidProvideContentKeyRequestInitializationDataForTrackIDCallback([weakThis, abortSemaphore](Ref<SharedBuffer>&& initData, TrackID trackID) mutable {
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

        parser->setDidProvideContentKeyRequestIdentifierForTrackIDCallback([weakThis] (Ref<SharedBuffer>&& initData, TrackID trackID) {
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
    if (m_videoRenderer)
        setVideoRenderer(nullptr);

    if (m_decompressionSession)
        setDecompressionSession(nullptr);

    for (auto& pair : m_audioRenderers) {
        RetainPtr renderer = pair.second;
        if (auto player = this->player())
            player->removeAudioRenderer(renderer.get());
        [renderer flush];
        [renderer stopRequestingMediaData];
        m_listener->stopObservingAudioRenderer(renderer.get());

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        if (m_cdmInstance && shouldAddContentKeyRecipients())
            [m_cdmInstance->contentKeySession() removeContentKeyRecipient:renderer.get()];
#endif
    }

    m_audioRenderers.clear();
}

void SourceBufferPrivateAVFObjC::clearTracks()
{
    for (auto& pair : m_videoTracks) {
        RefPtr track = pair.second;
        track->setSelectedChangedCallback(nullptr);
        if (auto player = this->player())
            player->removeVideoTrack(*track);
    }
    m_videoTracks.clear();

    for (auto& pair : m_audioTracks) {
        RefPtr track = pair.second;
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

bool SourceBufferPrivateAVFObjC::hasSelectedVideo() const
{
    return !!m_enabledVideoTrackID;
}

void SourceBufferPrivateAVFObjC::trackDidChangeSelected(VideoTrackPrivate& track, bool selected)
{
    auto trackID = track.id();

    ALWAYS_LOG(LOGIDENTIFIER, "video trackID = ", trackID, ", selected = ", selected);

    if (!selected && isEnabledVideoTrackID(trackID)) {
        m_enabledVideoTrackID.reset();
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
    auto trackID = track.id();

    ALWAYS_LOG(LOGIDENTIFIER, "audio trackID = ", trackID, ", enabled = ", enabled);

    if (!enabled) {
        if (RetainPtr renderer = audioRendererForTrackID(trackID)) {
            if (RefPtr player = this->player())
                player->removeAudioRenderer(renderer.get());
        }
    } else {
        RetainPtr renderer = audioRendererForTrackID(trackID);
        if (!renderer) {
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
        if (m_cdmInstance && shouldAddContentKeyRecipients())
            [m_cdmInstance->contentKeySession() addContentKeyRecipient:renderer.get()];
#endif

            ThreadSafeWeakPtr weakThis { *this };
            [renderer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->didBecomeReadyForMoreSamples(trackID);
            }];
            m_audioRenderers.try_emplace(trackID, renderer);
            m_listener->beginObservingAudioRenderer(renderer.get());
        }

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
                    protectedThis->m_session->videoRendererDidReceiveError(nullptr, protectedThis->m_hdcpError.get(), ignored);
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

    if (m_cdmInstance) {
        if (shouldAddContentKeyRecipients()) {
            if (m_videoRenderer)
                [m_cdmInstance->contentKeySession() removeContentKeyRecipient:m_videoRenderer->displayLayer()];

            for (auto& pair : m_audioRenderers)
                [m_cdmInstance->contentKeySession() removeContentKeyRecipient:pair.second.get()];
        }
        m_cdmInstance->removeKeyStatusesChangedObserver(*m_keyStatusesChangedObserver);
    }

    m_cdmInstance = fpsInstance;

    if (m_cdmInstance) {
        if (shouldAddContentKeyRecipients()) {
            if (m_videoRenderer)
                [m_cdmInstance->contentKeySession() addContentKeyRecipient:m_videoRenderer->displayLayer()];

            for (auto& pair : m_audioRenderers)
                [m_cdmInstance->contentKeySession() addContentKeyRecipient:pair.second.get()];
        }
        m_cdmInstance->addKeyStatusesChangedObserver(*m_keyStatusesChangedObserver);
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

    if (!MediaSessionManagerCocoa::shouldUseModernAVContentKeySession()) {
        if (auto parser = this->streamDataParser())
            [instanceSession->contentKeySession() addContentKeyRecipient:parser];
    }
    if (m_hasSessionSemaphore) {
        m_hasSessionSemaphore->signal();
        m_hasSessionSemaphore = nullptr;
    }
    m_waitingForKey = false;

    tryToEnqueueBlockedSamples();
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

    for (auto& pair : m_audioRenderers)
        flushAudio(pair.second.get());
}

void SourceBufferPrivateAVFObjC::flushIfNeeded()
{
    if (!requiresFlush())
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

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
    if (m_videoRenderer)
        m_videoRenderer->stopRequestingMediaData();

    if (m_enabledVideoTrackID)
        reenqueSamples(*m_enabledVideoTrackID);
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

void SourceBufferPrivateAVFObjC::videoRendererDidReceiveError(WebSampleBufferVideoRendering *renderer, NSError *error)
{
    ERROR_LOG(LOGIDENTIFIER, error);

#if PLATFORM(IOS_FAMILY)
    if (renderer.status == AVQueuedSampleBufferRenderingStatusFailed && [error.domain isEqualToString:@"AVFoundationErrorDomain"] && error.code == AVErrorOperationInterrupted) {
        m_displayLayerWasInterrupted = true;
        if (RefPtr player = this->player())
            player->setNeedsPlaceholderImage(true);
        return;
    }
#endif

    // FIXME(142246): Remove the following once <rdar://problem/20027434> is resolved.
    bool anyIgnored = false;
    for (auto& client : m_errorClients) {
        bool shouldIgnore = false;
        client->videoRendererDidReceiveError(renderer, error, shouldIgnore);
        anyIgnored |= shouldIgnore;
    }
    if (anyIgnored)
        return;

    int errorCode = [[[error userInfo] valueForKey:@"OSStatus"] intValue];

    if (RefPtr client = this->client())
        client->sourceBufferPrivateDidReceiveRenderingError(errorCode);
}

void SourceBufferPrivateAVFObjC::audioRendererWasAutomaticallyFlushed(AVSampleBufferAudioRenderer *renderer, const CMTime& time)
{
    auto mediaTime = PAL::toMediaTime(time);
    ERROR_LOG(LOGIDENTIFIER, mediaTime);
    std::optional<TrackID> trackId;
    for (auto& pair : m_audioRenderers) {
        if (pair.second.get() == renderer) {
            trackId = pair.first;
            break;
        }
    }
    if (!trackId) {
        ERROR_LOG(LOGIDENTIFIER, "Couldn't find track attached to Audio Renderer.");
        return;
    }
    reenqueSamples(*trackId);
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

    RetainPtr error = [NSError errorWithDomain:@"com.apple.WebKit" code:'HDCP' userInfo:nil];
    videoRendererDidReceiveError(m_videoRenderer ? m_videoRenderer->renderer() : nil, error.get());
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void SourceBufferPrivateAVFObjC::audioRendererDidReceiveError(AVSampleBufferAudioRenderer *renderer, NSError *error)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    ERROR_LOG(LOGIDENTIFIER, error);

    if ([error code] == 'HDCP')
        m_hdcpError = error;

    // FIXME(142246): Remove the following once <rdar://problem/20027434> is resolved.
    bool anyIgnored = false;
    for (auto& client : m_errorClients) {
        bool shouldIgnore = false;
        client->audioRendererDidReceiveError(renderer, error, shouldIgnore);
        anyIgnored |= shouldIgnore;
    }
    if (anyIgnored)
        return;
}

void SourceBufferPrivateAVFObjC::videoRendererRequiresFlushToResumeDecodingChanged(WebSampleBufferVideoRendering *renderer, bool requiresFlush)
{
    if ((m_videoRenderer && renderer != m_videoRenderer->renderer()) || m_layerRequiresFlush == requiresFlush)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, requiresFlush);
    m_layerRequiresFlush = requiresFlush;
}

void SourceBufferPrivateAVFObjC::videoRendererReadyForDisplayChanged(WebSampleBufferVideoRendering *renderer, bool isReadyForDisplay)
{
    if (!m_videoRenderer || renderer != m_videoRenderer->renderer() || !isReadyForDisplay)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    if (RefPtr player = this->player())
        player->setHasAvailableVideoFrame(true);
}

void SourceBufferPrivateAVFObjC::flush(TrackID trackID)
{
    DEBUG_LOG(LOGIDENTIFIER, trackID);

    if (isEnabledVideoTrackID(trackID)) {
        flushVideo();
    } else if (auto renderer = audioRendererForTrackID(trackID))
        flushAudio(renderer.get());
}

void SourceBufferPrivateAVFObjC::flushVideo()
{
    DEBUG_LOG(LOGIDENTIFIER);
    if (m_videoRenderer)
        m_videoRenderer->flush();

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
RetainPtr<AVSampleBufferAudioRenderer> SourceBufferPrivateAVFObjC::audioRendererForTrackID(TrackID trackID) const
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    auto it = m_audioRenderers.find(trackID);
    return it != m_audioRenderers.end() ? it->second : nil;
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void SourceBufferPrivateAVFObjC::flushAudio(AVSampleBufferAudioRenderer *renderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    [renderer flush];

    if (RefPtr player = this->player())
        player->setHasAvailableAudioSample(renderer, false);
}

bool SourceBufferPrivateAVFObjC::trackIsBlocked(TrackID trackID) const
{
    for (auto& samplePair : m_blockedSamples) {
        if (samplePair.first == trackID)
            return true;
    }
    return false;
}

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
void SourceBufferPrivateAVFObjC::tryToEnqueueBlockedSamples()
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

bool SourceBufferPrivateAVFObjC::canEnqueueSample(TrackID trackID, const MediaSampleAVFObjC& sample)
{
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    // if sample is unencrytped: enqueue sample
    if (!sample.isProtected())
        return true;

    // If sample buffers don't support modern AVContentKeySession: enqueue sample
    if (!MediaSessionManagerCocoa::shouldUseModernAVContentKeySession())
        return true;

    // if sample is encrypted, but we are not attached to a CDM: do not enqueue sample.
    if (!m_cdmInstance && !m_session)
        return false;

    // DecompressionSessions doesn't support encrypted media.
    if (isEnabledVideoTrackID(trackID) && !m_videoRenderer)
        return false;

    // if sample is encrypted, and keyIDs match the current set of keyIDs: enqueue sample.
    if (auto findResult = m_currentTrackIDs.find(trackID); findResult != m_currentTrackIDs.end() && findResult->second == sample.keyIDs())
        return true;

    // if sample's set of keyIDs does not match the current set of keyIDs, consult with the CDM
    // to determine if the keyIDs are usable; if so, update the current set of keyIDs and enqueue sample.
    if (m_cdmInstance && m_cdmInstance->isAnyKeyUsable(sample.keyIDs())) {
        m_currentTrackIDs.try_emplace(trackID, sample.keyIDs());
        return true;
    }

    if (m_session && m_session->isAnyKeyUsable(sample.keyIDs())) {
        m_currentTrackIDs.try_emplace(trackID, sample.keyIDs());
        return true;
    }

    return false;
#else
    return true;
#endif
}

void SourceBufferPrivateAVFObjC::enqueueSample(Ref<MediaSample>&& sample, TrackID trackID)
{
    if (!isEnabledVideoTrackID(trackID) && !audioRendererForTrackID(trackID))
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

void SourceBufferPrivateAVFObjC::enqueueSample(Ref<MediaSampleAVFObjC>&& sample, TrackID trackID)
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

    if (isEnabledVideoTrackID(trackID)) {
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

        if (!m_videoRenderer)
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

        attachContentKeyToSampleIfNeeded(sample);

        if (auto renderer = audioRendererForTrackID(trackID)) {
            [renderer enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
            if (RefPtr player = this->player(); player && !sample->isNonDisplaying())
                player->setHasAvailableAudioSample(renderer.get(), true);
        }
    }
}

void SourceBufferPrivateAVFObjC::enqueueSampleBuffer(MediaSampleAVFObjC& sample)
{
    attachContentKeyToSampleIfNeeded(sample);
    WebSampleBufferVideoRendering *renderer = nil;
    if (m_videoRenderer) {
        m_videoRenderer->enqueueSample(sample.platformSample().sample.cmSampleBuffer, !sample.isNonDisplaying());

        // Enqueuing a sample for display my synchronously fire an error, which can cause m_videoRenderer to become null.
        if (!m_videoRenderer)
            return;
        renderer = m_videoRenderer->renderer();
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
        if (AVSampleBufferDisplayLayer *displayLayer = m_videoRenderer->displayLayer()) {
            // FIXME (117934497): Remove staging code once -[AVSampleBufferDisplayLayer isReadyForDisplay] is available in SDKs used by WebKit builders
            if ([displayLayer respondsToSelector:@selector(isReadyForDisplay)])
                return;
        }
#endif
    }
    RefPtr player = this->player();
    if (!player || player->hasAvailableVideoFrame() || sample.isNonDisplaying())
        return;

    DEBUG_LOG(LOGIDENTIFIER, "adding buffer attachment");

    [renderer prerollDecodeWithCompletionHandler:[this, weakThis = ThreadSafeWeakPtr { *this }, logSiteIdentifier = LOGIDENTIFIER] (BOOL success) mutable {
        callOnMainThread([this, weakThis = WTFMove(weakThis), logSiteIdentifier, success] () {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (!success) {
                ERROR_LOG(logSiteIdentifier, "prerollDecodeWithCompletionHandler failed");
                return;
            }

            if (!m_videoRenderer) {
                ERROR_LOG(logSiteIdentifier, "prerollDecodeWithCompletionHandler called after renderer destroyed");
                return;
            }

            videoRendererReadyForDisplayChanged(m_videoRenderer->renderer(), true);
        });
    }];
}

void SourceBufferPrivateAVFObjC::attachContentKeyToSampleIfNeeded(const MediaSampleAVFObjC& sample)
{
    if (!MediaSessionManagerCocoa::shouldUseModernAVContentKeySession() || !supportsAttachContentKey())
        return;

    if (m_cdmInstance)
        m_cdmInstance->attachContentKeyToSample(sample);
    else if (m_session)
        m_session->attachContentKeyToSample(sample);
}

bool SourceBufferPrivateAVFObjC::isReadyForMoreSamples(TrackID trackID)
{
    if (isEnabledVideoTrackID(trackID)) {
        if (requiresFlush())
            return false;

        if (m_decompressionSession)
            return m_decompressionSession->isReadyForMoreMediaData();

        return m_videoRenderer && m_videoRenderer->isReadyForMoreMediaData();
    }

    if (auto renderer = audioRendererForTrackID(trackID))
        return [renderer isReadyForMoreMediaData];

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

void SourceBufferPrivateAVFObjC::didBecomeReadyForMoreSamples(TrackID trackID)
{
    INFO_LOG(LOGIDENTIFIER, trackID);

    if (isEnabledVideoTrackID(trackID)) {
        if (m_decompressionSession)
            m_decompressionSession->stopRequestingMediaData();
        if (m_videoRenderer)
            m_videoRenderer->stopRequestingMediaData();
    } else if (auto renderer = audioRendererForTrackID(trackID))
        [renderer stopRequestingMediaData];
    else
        return;

    if (trackIsBlocked(trackID))
        return;

    provideMediaData(trackID);
}

void SourceBufferPrivateAVFObjC::notifyClientWhenReadyForMoreSamples(TrackID trackID)
{
    if (requiresFlush())
        return;

    if (isEnabledVideoTrackID(trackID)) {
        if (m_decompressionSession) {
            m_decompressionSession->requestMediaDataWhenReady([weakThis = ThreadSafeWeakPtr { *this }, trackID] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->didBecomeReadyForMoreSamples(trackID);
            });
        }
        if (m_videoRenderer) {
            m_videoRenderer->requestMediaDataWhenReady([weakThis = ThreadSafeWeakPtr { *this }, trackID] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->didBecomeReadyForMoreSamples(trackID);
            });
        }
    } else if (auto renderer = audioRendererForTrackID(trackID)) {
        ThreadSafeWeakPtr weakThis { *this };
        [renderer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didBecomeReadyForMoreSamples(trackID);
        }];
    }
}

bool SourceBufferPrivateAVFObjC::canSetMinimumUpcomingPresentationTime(TrackID trackID) const
{
    return isEnabledVideoTrackID(trackID);
}

void SourceBufferPrivateAVFObjC::setMinimumUpcomingPresentationTime(TrackID trackID, const MediaTime& presentationTime)
{
    ASSERT(canSetMinimumUpcomingPresentationTime(trackID));
    if (canSetMinimumUpcomingPresentationTime(trackID) && m_videoRenderer)
        m_videoRenderer->expectMinimumUpcomingSampleBufferPresentationTime(presentationTime);
}

void SourceBufferPrivateAVFObjC::clearMinimumUpcomingPresentationTime(TrackID trackID)
{
    ASSERT(canSetMinimumUpcomingPresentationTime(trackID));
    if (canSetMinimumUpcomingPresentationTime(trackID) && m_videoRenderer)
        m_videoRenderer->resetUpcomingSampleBufferPresentationTimeExpectations();
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

void SourceBufferPrivateAVFObjC::configureVideoRenderer(VideoMediaSampleRenderer& videoRenderer)
{
    videoRenderer.setResourceOwner(m_resourceOwner);
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (m_cdmInstance && shouldAddContentKeyRecipients())
        [m_cdmInstance->contentKeySession() addContentKeyRecipient:videoRenderer.displayLayer()];
#endif
    videoRenderer.requestMediaDataWhenReady([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_enabledVideoTrackID)
            protectedThis->didBecomeReadyForMoreSamples(*protectedThis->m_enabledVideoTrackID);
    });
    m_listener->beginObservingVideoRenderer(videoRenderer.renderer());
}

void SourceBufferPrivateAVFObjC::invalidateVideoRenderer(VideoMediaSampleRenderer& videoRenderer)
{
    videoRenderer.flush();
    videoRenderer.stopRequestingMediaData();
    m_listener->stopObservingVideoRenderer(videoRenderer.renderer());

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (m_cdmInstance && shouldAddContentKeyRecipients())
        [m_cdmInstance->contentKeySession() removeContentKeyRecipient:videoRenderer.displayLayer()];
#endif
}

void SourceBufferPrivateAVFObjC::setVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    if (m_videoRenderer && renderer == m_videoRenderer->renderer()) {
        if (RefPtr expiringVideoRenderer = std::exchange(m_expiringVideoRenderer, nullptr))
            invalidateVideoRenderer(*expiringVideoRenderer);
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, "!!renderer = ", !!renderer);
    ASSERT(!renderer || !m_decompressionSession || hasSelectedVideo());

    if (m_videoRenderer)
        invalidateVideoRenderer(*std::exchange(m_videoRenderer, nullptr));

    if (!renderer)
        return;

    m_videoRenderer = VideoMediaSampleRenderer::create(renderer);
    configureVideoRenderer(*m_videoRenderer);
    if (m_enabledVideoTrackID)
        reenqueSamples(*m_enabledVideoTrackID);
}

void SourceBufferPrivateAVFObjC::stageVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    if (m_videoRenderer && renderer == m_videoRenderer->renderer())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "!!renderer = ", !!renderer);
    ASSERT(!renderer || !m_decompressionSession || hasSelectedVideo());

    if (m_expiringVideoRenderer)
        invalidateVideoRenderer(*std::exchange(m_expiringVideoRenderer, nullptr));

    m_expiringVideoRenderer = WTFMove(m_videoRenderer);
    m_videoRenderer = VideoMediaSampleRenderer::create(renderer);
    configureVideoRenderer(*m_videoRenderer);
    if (m_enabledVideoTrackID)
        reenqueSamples(*m_enabledVideoTrackID, NeedsFlush::No);
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
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_enabledVideoTrackID)
            protectedThis->didBecomeReadyForMoreSamples(*protectedThis->m_enabledVideoTrackID);
    });
    m_decompressionSession->notifyWhenHasAvailableVideoFrame([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get()) {
            if (auto player = protectedThis->player())
                player->setHasAvailableVideoFrame(true);
        }
    });
    if (m_enabledVideoTrackID)
        reenqueSamples(*m_enabledVideoTrackID);
}

RefPtr<MediaPlayerPrivateMediaSourceAVFObjC> SourceBufferPrivateAVFObjC::player() const
{
    if (RefPtr mediaSource = m_mediaSource.get())
        return downcast<MediaPlayerPrivateMediaSourceAVFObjC>(mediaSource->player());
    return nullptr;
}

bool SourceBufferPrivateAVFObjC::isEnabledVideoTrackID(TrackID trackID) const
{
    return m_enabledVideoTrackID && *m_enabledVideoTrackID == trackID;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateAVFObjC::logChannel() const
{
    return LogMediaSource;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
