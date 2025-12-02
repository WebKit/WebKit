/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/HostingContext.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/MediaPromiseTypes.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/TrackInfo.h>
#include <WebCore/VideoPlaybackQualityMetrics.h>
#include <WebCore/VideoTarget.h>
#include <optional>
#include <wtf/AbstractThreadSafeRefCountedAndCanMakeWeakPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/MediaTime.h>
#include <wtf/NativePromise.h>
#include <wtf/ObjectIdentifier.h>

namespace WebCore {

class CDMInstance;
class FloatRect;
class GraphicsContext;
class LegacyCDMSession;
class LayoutRect;
class MediaSample;
class NativeImage;
class PlatformDynamicRangeLimit;
class ProcessIdentity;
class TextTrackRepresentation;
class VideoFrame;

class AudioInterface {
public:
    using PitchCorrectionAlgorithm = MediaPlayerPitchCorrectionAlgorithm;
    virtual ~AudioInterface() = default;
    virtual void setVolume(float) = 0;
    virtual void setMuted(bool) = 0;
    virtual void setPreservesPitchAndCorrectionAlgorithm(bool, std::optional<PitchCorrectionAlgorithm>) { }
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    virtual void setOutputDeviceId(const String&) { }
#endif
};

class VideoInterface {
public:
    virtual ~VideoInterface() = default;
    virtual void setIsVisible(bool) = 0;
    virtual void setPresentationSize(const IntSize&) = 0;
    virtual void setShouldMaintainAspectRatio(bool) { }
    virtual void renderingCanBeAcceleratedChanged(bool) { }
    virtual void contentBoxRectChanged(const LayoutRect&) { }
    virtual void notifyFirstFrameAvailable(Function<void()>&&) { }
    virtual void notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&&) { }
    virtual void notifyWhenRequiresFlushToResume(Function<void()>&&) { }
    virtual void notifyRenderingModeChanged(Function<void()>&&) { }
    virtual void expectMinimumUpcomingPresentationTime(const MediaTime&) { }
    virtual void notifySizeChanged(Function<void(const MediaTime&, FloatSize)>&&) { }
    virtual void setShouldDisableHDR(bool) { };
    virtual void setPlatformDynamicRangeLimit(const PlatformDynamicRangeLimit&) { };
    virtual void setResourceOwner(const ProcessIdentity&) { }
    virtual void flushAndRemoveImage() { };
    virtual RefPtr<VideoFrame> currentVideoFrame() const = 0;
    virtual void paintCurrentVideoFrameInContext(GraphicsContext&, const FloatRect&) { }
    virtual RefPtr<NativeImage> currentNativeImage() const { return nullptr; }
#if ENABLE(VIDEO)
    virtual std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() = 0;
#endif
    virtual PlatformLayer* platformVideoLayer() const { return nullptr; }

    using LayerHostingContextCallback = CompletionHandler<void(HostingContext)>;
    virtual void requestHostingContext(LayerHostingContextCallback&& completionHandler) { completionHandler({ }); }
    virtual HostingContext hostingContext() const { return { }; }
    virtual WebCore::FloatSize videoLayerSize() const { return { }; }
    virtual void notifyVideoLayerSizeChanged(Function<void(const MediaTime&, FloatSize)>&&) { }
    virtual void setVideoLayerSizeFenced(const FloatSize&, WTF::MachSendRightAnnotated&&) { }
};

class VideoFullscreenInterface {
public:
    virtual ~VideoFullscreenInterface() = default;
#if ENABLE(VIDEO_PRESENTATION_MODE)
    virtual void setVideoFullscreenLayer(PlatformLayer*, Function<void()>&&) { }
    virtual void setVideoFullscreenFrame(const FloatRect&) { }
    virtual Ref<GenericPromise> setVideoTarget(const PlatformVideoTarget&) { return GenericPromise::createAndReject(); }
    virtual void isInFullscreenOrPictureInPictureChanged(bool) { }
#endif
    virtual void setTextTrackRepresentation(TextTrackRepresentation*) { }
    virtual void syncTextTrackBounds() { }
};

class SynchronizerInterface {
public:
    virtual ~SynchronizerInterface() = default;
    virtual void play(std::optional<MonotonicTime> hostTime = std::nullopt) = 0;
    virtual void pause(std::optional<MonotonicTime> hostTime = std::nullopt) = 0;
    virtual bool paused() const = 0;
    virtual void setRate(double) = 0;
    virtual double effectiveRate() const = 0;
    virtual void stall() { };
    virtual void prepareToSeek() { }
    virtual Ref<MediaTimePromise> seekTo(const MediaTime&) = 0;
    virtual bool seeking() const = 0;
};

struct SamplesRendererTrackIdentifierType;
using SamplesRendererTrackIdentifier = AtomicObjectIdentifier<SamplesRendererTrackIdentifierType>;

class TracksRendererManager {
public:
    using TrackType = TrackInfo::TrackType;
    using TrackIdentifier = SamplesRendererTrackIdentifier;

    virtual ~TracksRendererManager() = default;

    virtual void setPreferences(VideoRendererPreferences) { }
    virtual void setHasProtectedVideoContent(bool) { }

    virtual TrackIdentifier addTrack(TrackType) = 0;
    virtual void removeTrack(TrackIdentifier) = 0;

    virtual void enqueueSample(TrackIdentifier, Ref<MediaSample>&&, std::optional<MediaTime> = std::nullopt) = 0;
    virtual bool isReadyForMoreSamples(TrackIdentifier) = 0;
    using RequestPromise = NativePromise<TrackIdentifier, PlatformMediaError>;
    virtual Ref<RequestPromise> requestMediaDataWhenReady(TrackIdentifier) = 0;
    virtual void notifyTrackNeedsReenqueuing(TrackIdentifier, Function<void(TrackIdentifier, const MediaTime&)>&&) { }

    virtual bool timeIsProgressing() const = 0;
    virtual void notifyEffectiveRateChanged(Function<void(double)>&&) { }
    virtual MediaTime currentTime() const = 0;
    virtual void notifyTimeReachedAndStall(const MediaTime&, Function<void(const MediaTime&)>&&) { }
    virtual void cancelTimeReachedAction() { }
    virtual void performTaskAtTime(const MediaTime&, Function<void(const MediaTime&)>&&) { }
    virtual void setTimeObserver(Seconds, Function<void(const MediaTime&)>&&) { }

    virtual void flush() = 0;
    virtual void flushTrack(TrackIdentifier) = 0;

    virtual void applicationWillResignActive() { }

    virtual void notifyWhenErrorOccurs(Function<void(PlatformMediaError)>&&) = 0;

    using SoundStageSize = MediaPlayerSoundStageSize;
    virtual void setSpatialTrackingInfo(bool /* prefersSpatialAudioExperience */, SoundStageSize, const String& /* sceneIdentifier */, const String& /* defaultLabel */, const String& /* label */) { }

#if ENABLE(ENCRYPTED_MEDIA)
    virtual void setCDMInstance(CDMInstance*) { }
    virtual Ref<MediaPromise> setInitData(Ref<SharedBuffer>) { return MediaPromise::createAndResolve(); }
    virtual void attemptToDecrypt() { }
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    virtual RefPtr<SharedBuffer> initData() const { return nullptr; }
    virtual void setCDMSession(LegacyCDMSession*) { }
#endif
};

class WEBCORE_EXPORT AudioVideoRenderer : public AudioInterface, public VideoInterface, public VideoFullscreenInterface, public SynchronizerInterface, public TracksRendererManager, public AbstractThreadSafeRefCountedAndCanMakeWeakPtr {
public:
    virtual ~AudioVideoRenderer() = default;
};

}
