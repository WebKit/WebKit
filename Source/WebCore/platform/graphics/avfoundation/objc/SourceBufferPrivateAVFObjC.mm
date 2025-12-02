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

Ref<SourceBufferPrivateAVFObjC> SourceBufferPrivateAVFObjC::create(MediaSourcePrivateAVFObjC& parent, const MediaSourceConfiguration& configuration, Ref<SourceBufferParser>&& parser, Ref<AudioVideoRenderer>&& renderer)
{
    return adoptRef(*new SourceBufferPrivateAVFObjC(parent, configuration, WTFMove(parser), WTFMove(renderer)));
}

SourceBufferPrivateAVFObjC::SourceBufferPrivateAVFObjC(MediaSourcePrivateAVFObjC& parent, const MediaSourceConfiguration& configuration, Ref<SourceBufferParser>&& parser, Ref<AudioVideoRenderer>&& renderer)
    : SourceBufferPrivate(parent, parent.queueSingleton())
    , m_parser(WTFMove(parser))
    , m_appendQueue(WorkQueue::create("SourceBufferPrivateAVFObjC data parser queue"_s))
    , m_configuration(configuration)
    , m_renderer(WTFMove(renderer))
#if !RELEASE_LOG_DISABLED
    , m_logger(parent.logger())
    , m_logIdentifier(parent.nextSourceBufferLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);

    configureParser(m_parser);
}

SourceBufferPrivateAVFObjC::~SourceBufferPrivateAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_renderer) {
        ensureOnDispatcher([trackIdentifiers = std::exchange(m_trackIdentifiers, { }), renderer = WTFMove(m_renderer)] {
            for (auto& pair : trackIdentifiers)
                renderer->removeTrack(pair.second);
        });
    }

    callOnMainThread([weakMediaSource = m_mediaSource, videoTracks = std::exchange(m_videoTracks, { }), audioTracks = std::exchange(m_audioTracks, { }), textTracks = std::exchange(m_textTracks, { })] {
        RefPtr mediaSource = weakMediaSource.get();
        if (!mediaSource)
            return;
        RefPtr player = downcast<MediaPlayerPrivateMediaSourceAVFObjC>(mediaSource->player());
        if (!player)
            return;
        for (auto& pair : videoTracks) {
            RefPtr track = pair.second;
            track->setSelectedChangedCallback(nullptr);
            if (player)
                player->removeVideoTrack(*track);
        }

        for (auto& pair : audioTracks) {
            RefPtr track = pair.second;
            track->setEnabledChangedCallback(nullptr);
            if (player)
                player->removeAudioTrack(*track);
        }

        for (auto& pair : textTracks) {
            RefPtr track = pair.second;
            if (player)
                player->removeTextTrack(*track);
        }
    });

    abort();
}

void SourceBufferPrivateAVFObjC::setTrackChangeCallbacks(const InitializationSegment& segment, bool initialized)
{
    for (auto& videoTrackInfo : segment.videoTracks) {
        Ref { *videoTrackInfo.track }->setSelectedChangedCallback([weakThis = ThreadSafeWeakPtr { *this }, initialized] (VideoTrackPrivate& track, bool selected) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            protectedThis->ensureWeakOnDispatcher([trackId = track.id(), initialized, selected](auto& buffer) {
                assertIsCurrent(buffer.m_dispatcher.get());

                if (initialized) {
                    buffer.videoTrackDidChangeSelected(trackId, selected);
                    return;
                }
                buffer.m_pendingTrackChangeTasks.append([weakThis = ThreadSafeWeakPtr { buffer }, trackId, selected] {
                    if (RefPtr protectedThis = weakThis.get())
                        protectedThis->videoTrackDidChangeSelected(trackId, selected);
                });
            });
        });
    }

    for (auto& audioTrackInfo : segment.audioTracks) {
        Ref { *audioTrackInfo.track }->setEnabledChangedCallback([weakThis = ThreadSafeWeakPtr { *this }, initialized] (AudioTrackPrivate& track, bool enabled) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            protectedThis->ensureWeakOnDispatcher([trackId = track.id(), initialized, enabled](auto& buffer) {
                if (initialized) {
                    buffer.audioTrackDidChangeEnabled(trackId, enabled);
                    return;
                }

                assertIsCurrent(buffer.m_dispatcher.get());

                buffer.m_pendingTrackChangeTasks.append([weakThis = ThreadSafeWeakPtr { buffer }, trackId, enabled] {
                    if (RefPtr protectedThis = weakThis.get())
                        protectedThis->audioTrackDidChangeEnabled(trackId, enabled);
                });
            });
        });
    }

    // When a text track mode changes we should continue to parse and add cues to HTMLMediaElement, it will ensure
    // that only the correct cues are made visible.
}

void SourceBufferPrivateAVFObjC::setAudioVideoRenderer(AudioVideoRenderer& renderer)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    assertIsCurrent(m_dispatcher.get());

    ASSERT(m_isDetached);
    m_renderer = &renderer;
}

void SourceBufferPrivateAVFObjC::detach()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ensureOnDispatcher([protectedThis = Ref { *this }] {
        assertIsCurrent(protectedThis->m_dispatcher.get());
        protectedThis->flush();
        protectedThis->destroyRendererTracks();
        protectedThis->m_renderer = nullptr;
        protectedThis->m_isDetached = true;
    });
}

bool SourceBufferPrivateAVFObjC::precheckInitializationSegment(const InitializationSegment& segment)
{
    assertIsCurrent(m_dispatcher.get());
    ALWAYS_LOG(LOGIDENTIFIER);

    Vector<WebCore::ContentType> mediaContentTypesRequiringHardwareSupport;
    // FIXME: shouldCheckHardwareSupport to be called on main thread. Make precheckInitialisationSegment async?
    callOnMainThreadAndWait([&] {
        RefPtr player = this->player();
        if (!player || !player->shouldCheckHardwareSupport())
            return;
        mediaContentTypesRequiringHardwareSupport = player->mediaContentTypesRequiringHardwareSupport();
    });

    if (!mediaContentTypesRequiringHardwareSupport.isEmpty()) {
        for (auto& info : segment.videoTracks) {
            auto codec = FourCC::fromString(Ref { *info.description }->codec());
            if (!codec)
                continue;
            if (!codecsMeetHardwareDecodeRequirements({ { *codec } }, mediaContentTypesRequiringHardwareSupport))
                return false;
        }
    }

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
    assertIsCurrent(m_dispatcher.get());
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
                videoTrackDidChangeSelected(trackId, true);
            }
        }

        for (auto& audioTrackInfo : segment->audioTracks) {
            if (auto it = m_trackSelectedValues.find(audioTrackInfo.track->id()); it != m_trackSelectedValues.end() && it->second)
                audioTrackDidChangeEnabled(audioTrackInfo.track->id(), it->second);
        }

        m_isDetached = false;
    } else {
        for (auto& task : std::exchange(m_pendingTrackChangeTasks, { }))
            task();

        setTrackChangeCallbacks(*segment, true);
    }
    callOnMainThreadWithPlayer([](auto& player) {
        player.characteristicsChanged();
    });

    ALWAYS_LOG(LOGIDENTIFIER, "initialization segment was processed");
}

void SourceBufferPrivateAVFObjC::didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&& mediaSample, TrackID, const String&)
{
    didReceiveSample(WTFMove(mediaSample));
}

bool SourceBufferPrivateAVFObjC::isMediaSampleAllowed(const MediaSample& sample) const
{
    assertIsCurrent(m_dispatcher.get());
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
    // Called from SourceBuffer's thread.
    ensureWeakOnDispatcher([trackIdPairs = WTFMove(trackIdPairs)](auto& buffer) mutable {
        assertIsCurrent(buffer.m_dispatcher.get());
        for (auto& trackIdPair : trackIdPairs) {
            auto oldId = trackIdPair.first;
            auto newId = trackIdPair.second;
            ASSERT(oldId != newId);
            if (buffer.m_enabledVideoTrackID && *buffer.m_enabledVideoTrackID == oldId)
                buffer.m_enabledVideoTrackID = newId;
            if (buffer.m_protectedTrackID && *buffer.m_protectedTrackID == oldId)
                buffer.m_protectedTrackID = newId;
            auto it = buffer.m_trackIdentifiers.find(oldId);
            if (it == buffer.m_trackIdentifiers.end())
                continue;
            auto trackIdentifierNode = buffer.m_trackIdentifiers.extract(oldId);
            ASSERT(trackIdentifierNode);
            trackIdentifierNode.key() = newId;
            buffer.m_trackIdentifiers.insert(WTFMove(trackIdentifierNode));
        }
        buffer.maybeUpdateNeedsVideoLayer();
        buffer.SourceBufferPrivate::updateTrackIds(WTFMove(trackIdPairs));
    });
}

void SourceBufferPrivateAVFObjC::processFormatDescriptionForTrackId(Ref<TrackInfo>&& formatDescription, TrackID trackId)
{
    assertIsCurrent(m_dispatcher.get());
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
    assertIsCurrent(m_dispatcher.get());

    RefPtr mediaSource = downcast<MediaSourcePrivateAVFObjC>(m_mediaSource.get());
    if (!mediaSource)
        return;

#if HAVE(AVCONTENTKEYSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    ALWAYS_LOG(LOGIDENTIFIER, "track = ", trackID);

    m_protectedTrackID = trackID;
    maybeUpdateNeedsVideoLayer();
    callOnMainThreadWithPlayer([initData](auto& player) {
        player.keyNeeded(initData);
    });
#endif

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    protectedRenderer()->setInitData(initData)->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, initData = WTFMove(initData)](auto&& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (result) {
            protectedThis->m_waitingForKey = false;
            protectedThis->callOnMainThreadWithPlayer([](auto& player) {
                player.waitingForKeyChanged();
            });
            return;
        }
        switch (result.error()) {
        case PlatformMediaError::CDMInstanceKeyNeeded: {
            auto keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsSinf(initData);
            String initDataType = CDMPrivateFairPlayStreaming::sinfName();
#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
            if (!keyIDs) {
                keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsMpts(initData);
                initDataType = CDMPrivateFairPlayStreaming::mptsName();
            }
#endif
            if (!keyIDs)
                return;
            protectedThis->m_waitingForKey = true;
            protectedThis->callOnMainThreadWithPlayer([initDataType = initDataType.isolatedCopy(), initData](auto& player) {
                player.initializationDataEncountered(initDataType, initData->tryCreateArrayBuffer());
                player.waitingForKeyChanged();
                player.needsVideoLayerChanged();
            });
            return;
        }
        default:
            ASSERT_NOT_REACHED();
            return;
        }
    });
#endif
    UNUSED_PARAM(trackID);
}

bool SourceBufferPrivateAVFObjC::needsVideoLayer() const
{
    // When video content is protected and keys are assigned through
    // the renderers, decoding content through decompression sessions
    // will fail. In this scenario, ask the player to create a layer
    // instead.
    return m_needsVideoLayer;
}

void SourceBufferPrivateAVFObjC::maybeUpdateNeedsVideoLayer()
{
    assertIsCurrent(m_dispatcher.get());
    m_needsVideoLayer = m_protectedTrackID && isEnabledVideoTrackID(*m_protectedTrackID);
}

Ref<MediaPromise> SourceBufferPrivateAVFObjC::appendInternal(Ref<SharedBuffer>&& data)
{
    ALWAYS_LOG(LOGIDENTIFIER, "data length = ", data->size());

    return invokeAsync(m_dispatcher, [data = WTFMove(data), weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return MediaPromise::createAndReject(PlatformMediaError::SourceRemoved);
        assertIsCurrent(protectedThis->m_dispatcher.get());
        return invokeAsync(protectedThis->m_appendQueue, [data = WTFMove(data), parser = protectedThis->m_parser]() mutable {
            Ref ensureDestroyedSharedBuffer = WTFMove(data);
            return MediaPromise::createAndSettle(parser->appendData(WTFMove(ensureDestroyedSharedBuffer)));
        });
    })->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->appendCompleted(!!result);
        return MediaPromise::createAndSettle(WTFMove(result));
    });
}

void SourceBufferPrivateAVFObjC::appendCompleted(bool success)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!success)
        return;
    callOnMainThreadWithPlayer([](auto& player) {
        player.setLoadingProgresssed(true);
    });
}

void SourceBufferPrivateAVFObjC::resetParserStateInternal()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ensureWeakOnDispatcher([](auto& buffer) {
        assertIsCurrent(buffer.m_dispatcher.get());
        buffer.m_appendQueue->dispatch([parser = buffer.m_parser] {
            parser->resetParserState();
        });
    });
}

void SourceBufferPrivateAVFObjC::destroyRendererTracks()
{
    assertIsCurrent(m_dispatcher.get());
    ALWAYS_LOG(LOGIDENTIFIER);

    for (auto& pair : m_trackIdentifiers) {
        protectedRenderer()->removeTrack(pair.second);
    }
    m_trackIdentifiers.clear();
}

void SourceBufferPrivateAVFObjC::removedFromMediaSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ensureOnDispatcher([protectedThis = Ref { *this }, this] {
        destroyRendererTracks();
        SourceBufferPrivate::removedFromMediaSource();
    });
}

bool SourceBufferPrivateAVFObjC::hasSelectedVideo() const
{
    assertIsCurrent(m_dispatcher.get());
    return !!m_enabledVideoTrackID;
}

void SourceBufferPrivateAVFObjC::videoTrackDidChangeSelected(TrackID trackId, bool selected)
{
    assertIsCurrent(m_dispatcher.get());

    ALWAYS_LOG(LOGIDENTIFIER, "video trackID: ", trackId, ", selected: ", selected);

    if (selected) {
        if (m_enabledVideoTrackID == trackId)
            return;
        if (m_enabledVideoTrackID)
            removeTrackID(*m_enabledVideoTrackID);
        m_enabledVideoTrackID = trackId;
        m_trackIdentifiers.emplace(trackId, protectedRenderer()->addTrack(TrackInfo::TrackType::Video));
    }

    if (!selected && isEnabledVideoTrackID(trackId)) {
        removeTrackID(*m_enabledVideoTrackID);
        m_enabledVideoTrackID.reset();
    }

    maybeUpdateNeedsVideoLayer();
    if (RefPtr mediaSource = downcast<MediaSourcePrivateAVFObjC>(m_mediaSource.get()))
        mediaSource->hasSelectedVideoChanged(*this);
}

void SourceBufferPrivateAVFObjC::audioTrackDidChangeEnabled(TrackID trackId, bool enabled)
{
    assertIsCurrent(m_dispatcher.get());

    m_trackSelectedValues[trackId] = enabled;

    ALWAYS_LOG(LOGIDENTIFIER, "audio trackID: ", trackId, ", enabled: ", enabled);

    if (!enabled) {
        removeTrackID(trackId);
        return;
    }

    if (auto trackIdentifier = trackIdentifierFor(trackId))
        return;
    TrackIdentifier trackIdentifier = protectedRenderer()->addTrack(TrackInfo::TrackType::Audio);
    // FIXME: check if error has been set here.
    m_trackIdentifiers.emplace(trackId, trackIdentifier);
    protectedRenderer()->notifyTrackNeedsReenqueuing(trackIdentifier, [weakThis = ThreadSafeWeakPtr { *this }, trackId](TrackIdentifier, const MediaTime&) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->ensureWeakOnDispatcher([trackId](auto& buffer) {
            buffer.reenqueSamples(trackId);
        });
    });

    callOnMainThreadWithPlayer([trackIdentifier](auto& player) {
        player.addAudioTrack(trackIdentifier);
    });
}

std::optional<AudioVideoRenderer::TrackIdentifier> SourceBufferPrivateAVFObjC::trackIdentifierFor(TrackID trackId) const
{
    assertIsCurrent(m_dispatcher.get());

    if (auto it = m_trackIdentifiers.find(trackId); it != m_trackIdentifiers.end())
        return it->second;
    return std::nullopt;
}

void SourceBufferPrivateAVFObjC::setVideoRenderer(bool videoEnabled)
{
    assertIsCurrent(m_dispatcher.get());

    if (std::exchange(m_isSelectedForVideo, videoEnabled) == videoEnabled)
        return;

    if (videoEnabled && m_enabledVideoTrackID) {
        reenqueSamples(*m_enabledVideoTrackID);
        return;
    }
}

void SourceBufferPrivateAVFObjC::flush()
{
    assertIsCurrent(m_dispatcher.get());

    for (auto pair : m_trackIdentifiers)
        protectedRenderer()->flushTrack(pair.second);
}

void SourceBufferPrivateAVFObjC::flush(TrackID trackId)
{
    assertIsCurrent(m_dispatcher.get());
    DEBUG_LOG(LOGIDENTIFIER, trackId);

    if (auto trackIdentifier = trackIdentifierFor(trackId))
        protectedRenderer()->flushTrack(*trackIdentifier);
}

void SourceBufferPrivateAVFObjC::flushAndReenqueueVideo()
{
    assertIsCurrent(m_dispatcher.get());
    DEBUG_LOG(LOGIDENTIFIER);

    if (!m_isSelectedForVideo || !m_enabledVideoTrackID)
        return;
    reenqueSamples(*m_enabledVideoTrackID, NeedsFlush::Yes);
}

bool SourceBufferPrivateAVFObjC::isTextTrack(TrackID trackID) const
{
    assertIsCurrent(m_dispatcher.get());
    return m_textTracks.contains(trackID);
}

bool SourceBufferPrivateAVFObjC::hasTrackIdentifierFor(TrackID trackID) const
{
    assertIsCurrent(m_dispatcher.get());
    return m_trackIdentifiers.contains(trackID);
}

void SourceBufferPrivateAVFObjC::removeTrackID(TrackID trackID)
{
    assertIsCurrent(m_dispatcher.get());

    if (auto trackIdentifier = trackIdentifierFor(trackID)) {
        protectedRenderer()->removeTrack(*trackIdentifier);
        m_trackIdentifiers.erase(trackID);

        if (!m_audioTracks.contains(trackID))
            return;
        callOnMainThreadWithPlayer([trackIdentifier](auto& player) {
            player.removeAudioTrack(*trackIdentifier);
        });
    }
}

bool SourceBufferPrivateAVFObjC::canEnqueueSample(TrackID trackID, const MediaSampleAVFObjC&)
{
    assertIsCurrent(m_dispatcher.get());
    if (isEnabledVideoTrackID(trackID) && !m_isSelectedForVideo)
        return false;

    return true;
}

void SourceBufferPrivateAVFObjC::enqueueSample(Ref<MediaSample>&& sample, TrackID trackId)
{
    auto trackIdentifier = trackIdentifierFor(trackId);
    if (!trackIdentifier)
        return;

    ASSERT(is<MediaSampleAVFObjC>(sample));
    enqueueSample(downcast<MediaSampleAVFObjC>(WTFMove(sample)), trackId);
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

    if (auto trackIdentifier = trackIdentifierFor(trackId))
        protectedRenderer()->enqueueSample(*trackIdentifier, sample, mediaType == kCMMediaType_Video ? minimumUpcomingPresentationTimeForTrackID(trackId) : std::optional<MediaTime> { });
}

bool SourceBufferPrivateAVFObjC::isReadyForMoreSamples(TrackID trackId)
{
    if (auto trackIdentifier = trackIdentifierFor(trackId))
        return protectedRenderer()->isReadyForMoreSamples(*trackIdentifier);

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
    assertIsCurrent(m_dispatcher.get());
    return valueOrDefault(m_cachedSize);
}

void SourceBufferPrivateAVFObjC::didBecomeReadyForMoreSamples(TrackID trackId)
{
    INFO_LOG(LOGIDENTIFIER, trackId);

    provideMediaData(trackId);
}

void SourceBufferPrivateAVFObjC::notifyClientWhenReadyForMoreSamples(TrackID trackId)
{
    if (auto trackIdentifier = trackIdentifierFor(trackId)) {
        protectedRenderer()->requestMediaDataWhenReady(*trackIdentifier)->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, trackId](auto&& result) {
            if (RefPtr protectedThis = weakThis.get(); protectedThis && result)
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
        protectedRenderer()->expectMinimumUpcomingPresentationTime(presentationTime);
}

bool SourceBufferPrivateAVFObjC::canSwitchToType(const ContentType& contentType)
{
    ASSERT(isOnCreationThread());
    ALWAYS_LOG(LOGIDENTIFIER, contentType);

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;
    if (MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(parameters) == MediaPlayer::SupportsType::IsNotSupported)
        return false;
    RefPtr parser = SourceBufferParser::create(contentType, m_configuration);
    if (!parser)
        return false;
    ensureWeakOnDispatcher([parser = parser.releaseNonNull()](auto& buffer) mutable {
        assertIsCurrent(buffer.m_dispatcher.get());
        buffer.configureParser(parser);
        buffer.m_parser = WTFMove(parser);
    });
    return true;
}

void SourceBufferPrivateAVFObjC::configureParser(SourceBufferParser& parser)
{
    parser.setCallOnClientThreadCallback([dispatcher = m_dispatcher](auto&& function) {
        dispatcher->dispatch(WTFMove(function));
    });

    parser.setDidParseInitializationDataCallback([weakThis = ThreadSafeWeakPtr { *this }, dispatcher = m_dispatcher] (InitializationSegment&& segment) {
        assertIsCurrent(dispatcher);
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->didReceiveInitializationSegment(WTFMove(segment));
    });

    parser.setDidProvideMediaDataCallback([weakThis = ThreadSafeWeakPtr { *this }, dispatcher = m_dispatcher] (Ref<MediaSampleAVFObjC>&& sample, TrackID trackId, const String& mediaType) {
        assertIsCurrent(dispatcher);
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->didProvideMediaDataForTrackId(WTFMove(sample), trackId, mediaType);
    });

    parser.setDidUpdateFormatDescriptionForTrackIDCallback([weakThis = ThreadSafeWeakPtr { *this }, dispatcher = m_dispatcher] (Ref<TrackInfo>&& formatDescription, TrackID trackId) {
        assertIsCurrent(dispatcher);
        if (RefPtr protectedThis = weakThis.get(); protectedThis)
            protectedThis->didUpdateFormatDescriptionForTrackId(WTFMove(formatDescription), trackId);
    });

    parser.setDidProvideContentKeyRequestInitializationDataForTrackIDCallback([weakThis = ThreadSafeWeakPtr { *this }, dispatcher = m_dispatcher](Ref<SharedBuffer>&& initData, TrackID trackID) {
        assertIsCurrent(dispatcher);
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->didProvideContentKeyRequestInitializationDataForTrackID(WTFMove(initData), trackID);
    });

    parser.setDidProvideContentKeyRequestIdentifierForTrackIDCallback([weakThis = ThreadSafeWeakPtr { *this }, dispatcher = m_dispatcher] (Ref<SharedBuffer>&& initData, TrackID trackID) {
        assertIsCurrent(dispatcher);
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->didProvideContentKeyRequestInitializationDataForTrackID(WTFMove(initData), trackID);
    });

    if (RefPtr webmParser = dynamicDowncast<SourceBufferParserWebM>(parser); webmParser && m_configuration.supportsLimitedMatroska)
        webmParser->allowLimitedMatroska();

#if !RELEASE_LOG_DISABLED
    parser.setLogger(m_logger.get(), m_logIdentifier);
#endif
}

RefPtr<MediaPlayerPrivateMediaSourceAVFObjC> SourceBufferPrivateAVFObjC::player() const
{
    assertIsMainThread();
    if (RefPtr mediaSource = m_mediaSource.get())
        return downcast<MediaPlayerPrivateMediaSourceAVFObjC>(mediaSource->player());
    return nullptr;
}

bool SourceBufferPrivateAVFObjC::isEnabledVideoTrackID(TrackID trackID) const
{
    assertIsCurrent(m_dispatcher.get());
    return m_enabledVideoTrackID && *m_enabledVideoTrackID == trackID;
}

RefPtr<AudioVideoRenderer> SourceBufferPrivateAVFObjC::protectedRenderer() const
{
    assertIsCurrent(m_dispatcher.get());
    return m_renderer;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateAVFObjC::logChannel() const
{
    return LogMediaSource;
}
#endif

void SourceBufferPrivateAVFObjC::ensureWeakOnDispatcher(Function<void(SourceBufferPrivateAVFObjC&)>&& function)
{
    auto weakWrapper = [function = WTFMove(function), weakThis = ThreadSafeWeakPtr(*this)] mutable {
        if (RefPtr protectedThis = weakThis.get())
            function(*protectedThis);
    };
    ensureOnDispatcher(WTFMove(weakWrapper));
}

void SourceBufferPrivateAVFObjC::callOnMainThreadWithPlayer(Function<void(MediaPlayerPrivateMediaSourceAVFObjC&)>&& callback)
{
    ensureOnMainThread([callback = WTFMove(callback), weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (RefPtr player = protectedThis->player())
            callback(*player);
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
