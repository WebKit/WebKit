/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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
#import "AudioVideoRendererAVFObjC.h"
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
#import "SourceBufferParserWebM.h"
#import "SourceBufferPrivateClient.h"
#import "TimeRanges.h"
#import "VideoMediaSampleRenderer.h"
#import "VideoTrackPrivateMediaSourceAVFObjC.h"
#import "WebAVSampleBufferListener.h"
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
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/CString.h>

#pragma mark - Soft Linking

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

#pragma mark -
#pragma mark SourceBufferPrivateAVFObjC

#if ASSERT_ENABLED
static inline bool supportsAttachContentKey()
{
    static bool supportsAttachContentKey;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        supportsAttachContentKey = WTF::processHasEntitlement("com.apple.developer.web-browser-engine.rendering"_s) || WTF::processHasEntitlement("com.apple.private.coremedia.allow-fps-attachment"_s);
    });
    return supportsAttachContentKey;
}
#endif

Ref<SourceBufferPrivateAVFObjC> SourceBufferPrivateAVFObjC::create(MediaSourcePrivateAVFObjC& parent, Ref<SourceBufferParser>&& parser, Ref<AudioVideoRenderer>&& renderer)
{
    return adoptRef(*new SourceBufferPrivateAVFObjC(parent, WTFMove(parser), WTFMove(renderer)));
}

SourceBufferPrivateAVFObjC::SourceBufferPrivateAVFObjC(MediaSourcePrivateAVFObjC& parent, Ref<SourceBufferParser>&& parser, Ref<AudioVideoRenderer>&& renderer)
    : SourceBufferPrivate(parent)
    , m_parser(WTFMove(parser))
    , m_appendQueue(WorkQueue::create("SourceBufferPrivateAVFObjC data parser queue"_s))
    , m_renderer(WTFMove(renderer))
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

    RefPtr player = this->player();
    if (RefPtr webmParser = dynamicDowncast<SourceBufferParserWebM>(m_parser); webmParser && player && player->supportsLimitedMatroska())
        webmParser->allowLimitedMatroska();

#if !RELEASE_LOG_DISABLED
    m_parser->setLogger(m_logger.get(), m_logIdentifier);
#endif
}

SourceBufferPrivateAVFObjC::~SourceBufferPrivateAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    destroyRendererTracks();
    clearTracks();

    abort();
}

void SourceBufferPrivateAVFObjC::setTrackChangeCallbacks(const InitializationSegment& segment, bool initialized)
{
    for (auto& videoTrackInfo : segment.videoTracks) {
        Ref { *videoTrackInfo.track }->setSelectedChangedCallback([weakThis = ThreadSafeWeakPtr { *this }, initialized] (VideoTrackPrivate& track, bool selected) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (initialized) {
                protectedThis->trackDidChangeSelected(track, selected);
                return;
            }
            protectedThis->m_pendingTrackChangeTasks.append([weakThis, trackRef = Ref { track }, selected] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->trackDidChangeSelected(trackRef, selected);
            });
        });
    }

    for (auto& audioTrackInfo : segment.audioTracks) {
        Ref { *audioTrackInfo.track }->setEnabledChangedCallback([weakThis = ThreadSafeWeakPtr { *this }, initialized] (AudioTrackPrivate& track, bool enabled) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (initialized) {
                protectedThis->trackDidChangeEnabled(track, enabled);
                return;
            }

            protectedThis->m_pendingTrackChangeTasks.append([weakThis, trackRef = Ref { track }, enabled] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->trackDidChangeEnabled(trackRef, enabled);
            });
        });
    }

    // When a text track mode changes we should continue to parse and add cues to HTMLMediaElement, it will ensure
    // that only the correct cues are made visible.
}

void SourceBufferPrivateAVFObjC::setAudioVideoRenderer(AudioVideoRenderer& renderer)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(m_isDetached);
    m_renderer = renderer;
}

void SourceBufferPrivateAVFObjC::detach()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_isDetached = true;
    flush();
    destroyRendererTracks();
}

bool SourceBufferPrivateAVFObjC::precheckInitializationSegment(const InitializationSegment& segment)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (auto player = this->player(); player && player->shouldCheckHardwareSupport()) {
        for (auto& info : segment.videoTracks) {
            auto codec = FourCC::fromString(Ref { *info.description }->codec());
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

    for (auto& textTrackInfo : segment.textTracks)
        m_textTracks.try_emplace(textTrackInfo.track->id(), textTrackInfo.track);

    setTrackChangeCallbacks(segment, false);

    return true;
}

void SourceBufferPrivateAVFObjC::processInitializationSegment(std::optional<InitializationSegment>&& segment)
{
    ALWAYS_LOG(LOGIDENTIFIER, "isDetached: ", m_isDetached);

    if (!segment) {
        ERROR_LOG(LOGIDENTIFIER, "failed to process initialization segment");
        m_pendingTrackChangeTasks.clear();
        return;
    }

    if (m_isDetached) {
        ASSERT(m_pendingTrackChangeTasks.isEmpty());
        for (auto& videoTrackInfo : segment->videoTracks) {
            auto trackId = videoTrackInfo.track->id();
            if (m_enabledVideoTrackID == trackId) {
                m_enabledVideoTrackID.reset();
                trackDidChangeSelected(*videoTrackInfo.track, true);
            }
        }

        for (auto& audioTrackInfo : segment->audioTracks) {
            if (auto it = m_trackSelectedValues.find(audioTrackInfo.track->id()); it != m_trackSelectedValues.end() && it->second)
                trackDidChangeEnabled(*audioTrackInfo.track, it->second);
        }

        m_isDetached = false;
    } else {
        auto tasks = std::exchange(m_pendingTrackChangeTasks, { });
        for (auto& task : tasks)
            task();

        setTrackChangeCallbacks(*segment, true);
    }
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
    auto trackID = sample.trackID();
    if (isTextTrack(trackID)) {
        auto result = m_textTracks.find(trackID);
        if (result == m_textTracks.end())
            return false;

        if (RefPtr textTrack = downcast<InbandTextTrackPrivateAVF>(result->second)) {
            PlatformSample platformSample = sample.platformSample();
            textTrack->processVTTSample(platformSample.cmSampleBuffer(), sample.presentationTime());
        }

        return false;
    }

    return trackIdentifierFor(trackID).has_value();
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
        auto it = m_trackIdentifiers.find(oldId);
        if (it == m_trackIdentifiers.end())
            continue;
        auto trackIdentifierNode = m_trackIdentifiers.extract(oldId);
        ASSERT(trackIdentifierNode);
        trackIdentifierNode.key() = newId;
        m_trackIdentifiers.insert(WTFMove(trackIdentifierNode));
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

void SourceBufferPrivateAVFObjC::didProvideContentKeyRequestInitializationDataForTrackID(Ref<SharedBuffer>&& initData, TrackID trackID)
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
    m_initData = initData.copyRef();
    mediaSource->sourceBufferKeyNeeded(this, initData);

    if (player->cdmSession())
        return;
#endif

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    auto keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsSinf(initData);
    AtomString initDataType = CDMPrivateFairPlayStreaming::sinfName();
#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
    if (!keyIDs) {
        keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsMpts(initData);
        initDataType = CDMPrivateFairPlayStreaming::mptsName();
    }
#endif
    if (!keyIDs)
        return;

    m_pendingProtectedTrackInitDataMap.try_emplace(trackID, TrackInitData { initData.copyRef(), *keyIDs });

    if (RefPtr cdmInstance = m_cdmInstance) {
        if (RefPtr instanceSession = cdmInstance->sessionForKeyIDs(keyIDs.value())) {
            m_waitingForKey = false;
            return;
        }
    }

    m_keyIDs = WTFMove(keyIDs.value());
    player->initializationDataEncountered(initDataType, initData->tryCreateArrayBuffer());
    player->needsVideoLayerChanged();

    m_waitingForKey = true;
    player->waitingForKeyChanged();
#endif

    UNUSED_PARAM(initData);
    UNUSED_PARAM(trackID);
}

bool SourceBufferPrivateAVFObjC::needsVideoLayer() const
{
    // When video content is protected and keys are assigned through
    // the renderers, decoding content through decompression sessions
    // will fail. In this scenario, ask the player to create a layer
    // instead.
    return m_protectedTrackID && isEnabledVideoTrackID(*m_protectedTrackID);
}

Ref<MediaPromise> SourceBufferPrivateAVFObjC::appendInternal(Ref<SharedBuffer>&& data)
{
    ALWAYS_LOG(LOGIDENTIFIER, "data length = ", data->size());

    return invokeAsync(m_appendQueue, [data = WTFMove(data), parser = m_parser, weakThis = ThreadSafeWeakPtr { *this }]() mutable {
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

        parser->setDidProvideContentKeyRequestInitializationDataForTrackIDCallback([weakThis](Ref<SharedBuffer>&& initData, TrackID trackID) mutable {
            // Called on the data parser queue.
            callOnMainThread([weakThis, initData = WTFMove(initData), trackID] () mutable {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->didProvideContentKeyRequestInitializationDataForTrackID(WTFMove(initData), trackID);
            });
        });

        parser->setDidProvideContentKeyRequestIdentifierForTrackIDCallback([weakThis] (Ref<SharedBuffer>&& initData, TrackID trackID) {
            ASSERT(isMainThread());
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didProvideContentKeyRequestInitializationDataForTrackID(WTFMove(initData), trackID);
        });

        Ref ensureDestroyedSharedBuffer = WTFMove(data);
        return MediaPromise::createAndSettle(parser->appendData(WTFMove(ensureDestroyedSharedBuffer)));
    })->whenSettled(RunLoop::currentSingleton(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->appendCompleted(!!result);
        return MediaPromise::createAndSettle(WTFMove(result));
    });
}

void SourceBufferPrivateAVFObjC::appendCompleted(bool success)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (auto player = this->player(); player && success)
        player->setLoadingProgresssed(true);
}

void SourceBufferPrivateAVFObjC::resetParserStateInternal()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_appendQueue->dispatch([parser = m_parser] {
        parser->resetParserState();
    });
}

void SourceBufferPrivateAVFObjC::destroyRendererTracks()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    for (auto& pair : m_trackIdentifiers) {
        Ref { m_renderer }->removeTrack(pair.second);
    }
    m_trackIdentifiers.clear();
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

    for (auto& pair : m_textTracks) {
        RefPtr track = pair.second;
        if (RefPtr player = this->player())
            player->removeTextTrack(*track);
    }
    m_textTracks.clear();
}

void SourceBufferPrivateAVFObjC::removedFromMediaSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    destroyRendererTracks();

    SourceBufferPrivate::removedFromMediaSource();
}

bool SourceBufferPrivateAVFObjC::hasSelectedVideo() const
{
    return !!m_enabledVideoTrackID;
}

void SourceBufferPrivateAVFObjC::trackDidChangeSelected(VideoTrackPrivate& track, bool selected)
{
    auto trackId = track.id();

    ALWAYS_LOG(LOGIDENTIFIER, "video trackID: ", trackId, ", selected: ", selected);

    if (selected) {
        if (m_enabledVideoTrackID == trackId)
            return;
        if (m_enabledVideoTrackID)
            removeTrackID(*m_enabledVideoTrackID);
        m_enabledVideoTrackID = trackId;
        m_trackIdentifiers.emplace(trackId, Ref { m_renderer }->addTrack(TrackInfo::TrackType::Video));
    }

    if (!selected && isEnabledVideoTrackID(trackId)) {
        removeTrackID(*m_enabledVideoTrackID);
        m_enabledVideoTrackID.reset();
    }

    if (RefPtr mediaSource = downcast<MediaSourcePrivateAVFObjC>(m_mediaSource.get()))
        mediaSource->hasSelectedVideoChanged(*this);
}

void SourceBufferPrivateAVFObjC::trackDidChangeEnabled(AudioTrackPrivate& track, bool enabled)
{
    auto trackId = track.id();

    m_trackSelectedValues[trackId] = enabled;

    ALWAYS_LOG(LOGIDENTIFIER, "audio trackID: ", trackId, ", enabled: ", enabled);

    if (!enabled) {
        removeTrackID(trackId);
        return;
    }

    if (auto trackIdentifier = trackIdentifierFor(trackId))
        return;
    TrackIdentifier trackIdentifier = Ref { m_renderer }->addTrack(TrackInfo::TrackType::Audio);
    // FIXME: check if error has been set here.
    m_trackIdentifiers.emplace(trackId, trackIdentifier);
    Ref { m_renderer }->notifyTrackNeedsReenqueuing(trackIdentifier, [weakThis = ThreadSafeWeakPtr { *this }, trackId](TrackIdentifier, const MediaTime&) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->reenqueSamples(trackId);
    });

    if (RefPtr player = this->player())
        player->addAudioTrack(trackIdentifier);
}

#if (ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)) || ENABLE(LEGACY_ENCRYPTED_MEDIA)
void SourceBufferPrivateAVFObjC::setCDMSession(LegacyCDMSession* session)
{
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr oldSession = m_session.get();
    if (session == oldSession)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    if (oldSession)
        oldSession->removeSourceBuffer(this);

    // FIXME: This is a false positive. Remove the suppression once rdar://145631564 is fixed.
    SUPPRESS_UNCOUNTED_ARG m_session = toCDMSessionAVContentKeySession(session);

    if (RefPtr session = m_session.get())
        session->addSourceBuffer(this);
#else
    UNUSED_PARAM(session);
#endif
}

void SourceBufferPrivateAVFObjC::setCDMInstance(CDMInstance* instance)
{
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    RefPtr fpsInstance = dynamicDowncast<CDMInstanceFairPlayStreamingAVFObjC>(instance);
    if (fpsInstance == m_cdmInstance)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    if (RefPtr cdmInstance = m_cdmInstance)
        cdmInstance->removeKeyStatusesChangedObserver(m_keyStatusesChangedObserver);

    m_cdmInstance = fpsInstance;
    if (fpsInstance)
        fpsInstance->addKeyStatusesChangedObserver(m_keyStatusesChangedObserver);

    attemptToDecrypt();
#else
    UNUSED_PARAM(instance);
#endif
}

void SourceBufferPrivateAVFObjC::attemptToDecrypt()
{
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (m_keyIDs.isEmpty() || !m_waitingForKey)
        return;

    if (RefPtr cdmInstance = m_cdmInstance) {
        RefPtr instanceSession = cdmInstance->sessionForKeyIDs(m_keyIDs);
        if (!instanceSession)
            return;
    } else if (!m_session.get())
        return;

    m_waitingForKey = false;

    tryToEnqueueBlockedSamples();
#endif
}
#endif // (ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)) || ENABLE(LEGACY_ENCRYPTED_MEDIA)

std::optional<AudioVideoRenderer::TrackIdentifier> SourceBufferPrivateAVFObjC::trackIdentifierFor(TrackID trackId) const
{
    if (auto it = m_trackIdentifiers.find(trackId); it != m_trackIdentifiers.end())
        return it->second;
    return std::nullopt;
}

void SourceBufferPrivateAVFObjC::registerForErrorNotifications(SourceBufferPrivateAVFObjCErrorClient* client)
{
    ASSERT(!m_errorClients.contains(client));
    m_errorClients.append(client);
}

void SourceBufferPrivateAVFObjC::unregisterForErrorNotifications(SourceBufferPrivateAVFObjCErrorClient* client)
{
    ASSERT(m_errorClients.contains(client));
    m_errorClients.removeFirst(client);
}

void SourceBufferPrivateAVFObjC::setVideoRenderer(bool videoEnabled)
{
    if (std::exchange(m_isSelectedForVideo, videoEnabled) == videoEnabled)
        return;

    if (videoEnabled && m_enabledVideoTrackID) {
        reenqueSamples(*m_enabledVideoTrackID);
        return;
    }
}

void SourceBufferPrivateAVFObjC::flush()
{
    for (auto pair : m_trackIdentifiers)
        Ref { m_renderer }->flushTrack(pair.second);
}

void SourceBufferPrivateAVFObjC::flush(TrackID trackId)
{
    DEBUG_LOG(LOGIDENTIFIER, trackId);

    if (auto trackIdentifier = trackIdentifierFor(trackId))
        Ref { m_renderer }->flushTrack(*trackIdentifier);
}

void SourceBufferPrivateAVFObjC::flushAndReenqueueVideo()
{
    DEBUG_LOG(LOGIDENTIFIER);

    if (!m_isSelectedForVideo || !m_enabledVideoTrackID)
        return;
    reenqueSamples(*m_enabledVideoTrackID, NeedsFlush::Yes);
}

bool SourceBufferPrivateAVFObjC::isTextTrack(TrackID trackID) const
{
    return m_textTracks.contains(trackID);
}

bool SourceBufferPrivateAVFObjC::hasTrackIdentifierFor(TrackID trackID) const
{
    return m_trackIdentifiers.contains(trackID);
}

void SourceBufferPrivateAVFObjC::removeTrackID(TrackID trackID)
{
    if (auto trackIdentifier = trackIdentifierFor(trackID)) {
        Ref { m_renderer }->removeTrack(*trackIdentifier);
        m_trackIdentifiers.erase(trackID);

        if (m_audioTracks.contains(trackID)) {
            if (RefPtr player = this->player())
                player->removeAudioTrack(*trackIdentifier);
        }
    }
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
    if (isEnabledVideoTrackID(trackID) && !m_isSelectedForVideo)
        return false;

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    // if sample is unencrytped: enqueue sample
    if (!sample.isProtected())
        return true;

    // if sample is encrypted, but we are not attached to a CDM: do not enqueue sample.
    if (!m_cdmInstance && !m_session.get())
        return false;

    if (!isEnabledVideoTrackID(trackID))
        return false;

    // if sample is encrypted, and keyIDs match the current set of keyIDs: enqueue sample.
    if (auto findResult = m_currentTrackIDs.find(trackID); findResult != m_currentTrackIDs.end() && findResult->second == sample.keyIDs())
        return true;

    // if sample's set of keyIDs does not match the current set of keyIDs, consult with the CDM
    // to determine if the keyIDs are usable; if so, update the current set of keyIDs and enqueue sample.
    if (RefPtr cdmInstance = m_cdmInstance; cdmInstance && cdmInstance->isAnyKeyUsable(sample.keyIDs())) {
        m_currentTrackIDs.try_emplace(trackID, sample.keyIDs());
        return true;
    }

    if (RefPtr session = m_session.get(); session && session->isAnyKeyUsable(sample.keyIDs())) {
        m_currentTrackIDs.try_emplace(trackID, sample.keyIDs());
        return true;
    }

    return false;
#else
    return true;
#endif
}

void SourceBufferPrivateAVFObjC::enqueueSample(Ref<MediaSample>&& sample, TrackID trackId)
{
    auto trackIdentifier = trackIdentifierFor(trackId);
    if (!trackIdentifier)
        return;

    ASSERT(is<MediaSampleAVFObjC>(sample));
    if (RefPtr sampleAVFObjC = dynamicDowncast<MediaSampleAVFObjC>(WTFMove(sample))) {
        if (!canEnqueueSample(trackId, *sampleAVFObjC)) {
            m_blockedSamples.append({ trackId, sampleAVFObjC.releaseNonNull() });
            return;
        }

        enqueueSample(sampleAVFObjC.releaseNonNull(), trackId);
    }
}

void SourceBufferPrivateAVFObjC::enqueueSample(Ref<MediaSampleAVFObjC>&& sample, TrackID trackId)
{
    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, "track ID = ", trackId, ", sample = ", sample.get());

    PlatformSample platformSample = sample->platformSample();

    CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(platformSample.cmSampleBuffer());
    ASSERT(formatDescription);
    if (!formatDescription) {
        ERROR_LOG(logSiteIdentifier, "Received sample with a null formatDescription. Bailing.");
        return;
    }
    auto mediaType = PAL::CMFormatDescriptionGetMediaType(formatDescription);

    if (auto trackIdentifier = trackIdentifierFor(trackId)) {
        attachContentKeyToSampleIfNeeded(sample);
        Ref { m_renderer }->enqueueSample(*trackIdentifier, sample, mediaType == kCMMediaType_Video ? minimumUpcomingPresentationTimeForTrackID(trackId) : std::optional<MediaTime> { });
    }
}

void SourceBufferPrivateAVFObjC::attachContentKeyToSampleIfNeeded(const MediaSampleAVFObjC& sample)
{
    ASSERT((!m_cdmInstance && !m_session.get()) || supportsAttachContentKey());

    if (RefPtr cdmInstance = m_cdmInstance)
        cdmInstance->attachContentKeyToSample(sample);
    else if (RefPtr session = m_session.get())
        session->attachContentKeyToSample(sample);
}

bool SourceBufferPrivateAVFObjC::isReadyForMoreSamples(TrackID trackId)
{
    if (auto trackIdentifier = trackIdentifierFor(trackId))
        return Ref { m_renderer }->isReadyForMoreSamples(*trackIdentifier);

    return false;
}

MediaTime SourceBufferPrivateAVFObjC::timeFudgeFactor() const
{
    if (RefPtr mediaSource = m_mediaSource.get())
        return mediaSource->timeFudgeFactor();

    return SourceBufferPrivate::timeFudgeFactor();
}

FloatSize SourceBufferPrivateAVFObjC::naturalSize()
{
    return valueOrDefault(m_cachedSize);
}

void SourceBufferPrivateAVFObjC::didBecomeReadyForMoreSamples(TrackID trackId)
{
    INFO_LOG(LOGIDENTIFIER, trackId);

    if (auto trackIdentifier = trackIdentifierFor(trackId))
        Ref { m_renderer }->stopRequestingMediaData(*trackIdentifier);
    else
        return;

    if (trackIsBlocked(trackId))
        return;

    provideMediaData(trackId);
}

void SourceBufferPrivateAVFObjC::notifyClientWhenReadyForMoreSamples(TrackID trackId)
{
    if (auto trackIdentifier = trackIdentifierFor(trackId)) {
        Ref { m_renderer }->requestMediaDataWhenReady(*trackIdentifier, [weakThis = ThreadSafeWeakPtr { *this }, trackId](auto) {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didBecomeReadyForMoreSamples(trackId);
        });
    }
}

bool SourceBufferPrivateAVFObjC::canSetMinimumUpcomingPresentationTime(TrackID trackId) const
{
    return isEnabledVideoTrackID(trackId);
}

void SourceBufferPrivateAVFObjC::setMinimumUpcomingPresentationTime(TrackID trackId, const MediaTime& presentationTime)
{
    ASSERT_UNUSED(canSetMinimumUpcomingPresentationTime(trackId), trackId);
    if (auto trackIdentifier = trackIdentifierFor(trackId))
        Ref { m_renderer }->expectMinimumUpcomingPresentationTime(presentationTime);
}

bool SourceBufferPrivateAVFObjC::canSwitchToType(const ContentType& contentType)
{
    ALWAYS_LOG(LOGIDENTIFIER, contentType);

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;
    return MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(parameters) != MediaPlayer::SupportsType::IsNotSupported;
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
