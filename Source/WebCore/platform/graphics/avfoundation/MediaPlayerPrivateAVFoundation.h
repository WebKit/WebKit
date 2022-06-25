/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#include "FloatSize.h"
#include "InbandTextTrackPrivateAVF.h"
#include "MediaPlayerPrivate.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/LoggerHelper.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class InbandMetadataTextTrackPrivateAVF;
class InbandTextTrackPrivateAVF;

// Use eager initialization for the WeakPtrFactory since we construct WeakPtrs on another thread.
class MediaPlayerPrivateAVFoundation : public CanMakeWeakPtr<MediaPlayerPrivateAVFoundation, WeakPtrFactoryInitialization::Eager>, public MediaPlayerPrivateInterface, public AVFInbandTrackParent
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    virtual void metadataLoaded();
    virtual void playabilityKnown();
    virtual void rateChanged();
    virtual void loadedTimeRangesChanged();
    virtual void seekableTimeRangesChanged();
    virtual void timeChanged(const MediaTime&);
    virtual void seekCompleted(bool);
    virtual void didEnd();
    virtual void contentsNeedsDisplay() { }
    virtual void configureInbandTracks();
    virtual void setCurrentTextTrack(InbandTextTrackPrivateAVF*) { }
    virtual InbandTextTrackPrivateAVF* currentTextTrack() const = 0;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void playbackTargetIsWirelessChanged();
#endif

    void queueTaskOnEventLoop(Function<void()>&&);
    
    class Notification {
    public:
#define FOR_EACH_MEDIAPLAYERPRIVATEAVFOUNDATION_NOTIFICATION_TYPE(macro) \
    macro(None) \
    macro(ItemDidPlayToEndTime) \
    macro(ItemTracksChanged) \
    macro(ItemStatusChanged) \
    macro(ItemSeekableTimeRangesChanged) \
    macro(ItemLoadedTimeRangesChanged) \
    macro(ItemPresentationSizeChanged) \
    macro(ItemIsPlaybackLikelyToKeepUpChanged) \
    macro(ItemIsPlaybackBufferEmptyChanged) \
    macro(ItemIsPlaybackBufferFullChanged) \
    macro(AssetMetadataLoaded) \
    macro(AssetPlayabilityKnown) \
    macro(PlayerRateChanged) \
    macro(PlayerTimeChanged) \
    macro(SeekCompleted) \
    macro(DurationChanged) \
    macro(ContentsNeedsDisplay) \
    macro(InbandTracksNeedConfiguration) \
    macro(TargetIsWirelessChanged) \

        enum Type {
#define DEFINE_TYPE_ENUM(type) type,
            FOR_EACH_MEDIAPLAYERPRIVATEAVFOUNDATION_NOTIFICATION_TYPE(DEFINE_TYPE_ENUM)
#undef DEFINE_TYPE_ENUM
            FunctionType,
        };
        
        Notification()
            : m_type(None)
            , m_finished(false)
        {
        }

        Notification(Type type, const MediaTime& time)
            : m_type(type)
            , m_time(time)
            , m_finished(false)
        {
        }
        
        Notification(Type type, bool finished)
            : m_type(type)
            , m_finished(finished)
        {
        }

        Notification(Function<void()>&& function)
            : m_type(FunctionType)
            , m_finished(false)
            , m_function(WTFMove(function))
        {
        }
        
        Type type() { return m_type; }
        bool isValid() { return m_type != None; }
        MediaTime time() { return m_time; }
        bool finished() { return m_finished; }
        Function<void ()>& function() { return m_function; }
        
    private:
        Type m_type;
        MediaTime m_time;
        bool m_finished;
        Function<void()> m_function;
    };

    void scheduleMainThreadNotification(Notification&&);
    void scheduleMainThreadNotification(Notification::Type, const MediaTime& = MediaTime::zeroTime());
    void scheduleMainThreadNotification(Notification::Type, bool completed);
    void dispatchNotification();
    void clearMainThreadPendingFlag();

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    static bool extractKeyURIKeyIDAndCertificateFromInitData(Uint8Array* initData, String& keyURI, String& keyID, RefPtr<Uint8Array>& certificate);
#endif

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const override { return "MediaPlayerPrivateAVFoundation"; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const final;
#endif

    enum class MediaRenderingMode : uint8_t {
        MediaRenderingNone,
        MediaRenderingToContext,
        MediaRenderingToLayer
    };

protected:
    explicit MediaPlayerPrivateAVFoundation(MediaPlayer*);
    virtual ~MediaPlayerPrivateAVFoundation();

    // MediaPlayerPrivatePrivateInterface overrides.
    void load(const String& url) override;
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const ContentType&, MediaSourcePrivateClient&) override;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override { setNetworkState(MediaPlayer::NetworkState::FormatError); }
#endif
    void cancelLoad() override = 0;

    void prepareToPlay() override;

    void play() override;
    void pause() override;

    FloatSize naturalSize() const override;
    bool hasVideo() const override { return m_cachedHasVideo; }
    bool hasAudio() const override { return m_cachedHasAudio; }
    void setPageIsVisible(bool) final;
    MediaTime durationMediaTime() const override;
    MediaTime currentMediaTime() const override = 0;
    void seek(const MediaTime&) override;
    void seekWithTolerance(const MediaTime&, const MediaTime&, const MediaTime&) override;
    bool seeking() const override;
    bool paused() const override;
    void setVolume(float) override = 0;
    bool hasClosedCaptions() const override { return m_cachedHasCaptions; }
    MediaPlayer::NetworkState networkState() const override { return m_networkState; }
    MediaPlayer::ReadyState readyState() const override { return m_readyState; }
    MediaTime maxMediaTimeSeekable() const override;
    MediaTime minMediaTimeSeekable() const override;
    std::unique_ptr<PlatformTimeRanges> buffered() const override;
    bool didLoadingProgress() const override;
    void paint(GraphicsContext&, const FloatRect&) override = 0;
    DestinationColorSpace colorSpace() override = 0;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) override = 0;
    void setPreload(MediaPlayer::Preload) override;
    PlatformLayer* platformLayer() const override { return 0; }
    bool supportsAcceleratedRendering() const override = 0;
    void acceleratedRenderingStateChanged() override;
    bool shouldMaintainAspectRatio() const { return m_shouldMaintainAspectRatio; }
    void setShouldMaintainAspectRatio(bool) override;
    bool canSaveMediaData() const override;

    MediaPlayer::MovieLoadType movieLoadType() const override;
    void prepareForRendering() override;

    bool supportsPictureInPicture() const override { return true; }
    bool supportsFullscreen() const override;
    bool supportsScanning() const override { return true; }
    unsigned long long fileSize() const override { return totalBytes(); }

    bool hasSingleSecurityOrigin() const override;

    // Required interfaces for concrete derived classes.
    virtual void createAVAssetForURL(const URL&) = 0;
    virtual void createAVPlayer() = 0;
    virtual void createAVPlayerItem() = 0;

    enum ItemStatus {
        MediaPlayerAVPlayerItemStatusDoesNotExist,
        MediaPlayerAVPlayerItemStatusUnknown,
        MediaPlayerAVPlayerItemStatusFailed,
        MediaPlayerAVPlayerItemStatusReadyToPlay,
        MediaPlayerAVPlayerItemStatusPlaybackBufferEmpty,
        MediaPlayerAVPlayerItemStatusPlaybackBufferFull,
        MediaPlayerAVPlayerItemStatusPlaybackLikelyToKeepUp,
    };
    virtual ItemStatus playerItemStatus() const = 0;

    enum AssetStatus {
        MediaPlayerAVAssetStatusDoesNotExist,
        MediaPlayerAVAssetStatusUnknown,
        MediaPlayerAVAssetStatusLoading,
        MediaPlayerAVAssetStatusFailed,
        MediaPlayerAVAssetStatusCancelled,
        MediaPlayerAVAssetStatusLoaded,
        MediaPlayerAVAssetStatusPlayable,
    };
    virtual AssetStatus assetStatus() const = 0;
    virtual long assetErrorCode() const = 0;

    virtual void platformSetVisible(bool) = 0;
    virtual void platformPlay() = 0;
    virtual void platformPause() = 0;
    virtual bool platformPaused() const { return !rate(); }
    virtual void seekToTime(const MediaTime&, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance) = 0;
    unsigned long long totalBytes() const override = 0;
    virtual std::unique_ptr<PlatformTimeRanges> platformBufferedTimeRanges() const = 0;
    virtual MediaTime platformMaxTimeSeekable() const = 0;
    virtual MediaTime platformMinTimeSeekable() const = 0;
    virtual MediaTime platformMaxTimeLoaded() const = 0;
    virtual MediaTime platformDuration() const = 0;

    virtual void beginLoadingMetadata() = 0;
    virtual void sizeChanged() = 0;

    virtual void createContextVideoRenderer() = 0;
    virtual void destroyContextVideoRenderer() = 0;

    virtual void createVideoLayer() = 0;
    virtual void destroyVideoLayer() = 0;

    bool hasAvailableVideoFrame() const override = 0;

    virtual bool hasContextRenderer() const = 0;
    virtual bool hasLayerRenderer() const = 0;

    virtual void updateVideoLayerGravity() = 0;
    virtual void resolvedURLChanged() = 0;

    static bool isUnsupportedMIMEType(const String&);
    static const HashSet<String, ASCIICaseInsensitiveHash>& staticMIMETypeList();

    void updateStates();

    void setHasVideo(bool);
    void setHasAudio(bool);
    void setHasClosedCaptions(bool);
    void characteristicsChanged();
    void setDelayCharacteristicsChangedNotification(bool);
    void setDelayCallbacks(bool) const;
    void setIgnoreLoadStateChanges(bool delay) { m_ignoreLoadStateChanges = delay; }
    void setNaturalSize(FloatSize);
    bool isLiveStream() const { return std::isinf(duration()); }
    void setNetworkState(MediaPlayer::NetworkState);
    void setReadyState(MediaPlayer::ReadyState);

    MediaRenderingMode currentRenderingMode() const;
    MediaRenderingMode preferredRenderingMode() const;

    bool metaDataAvailable() const { return m_readyState >= MediaPlayer::ReadyState::HaveMetadata; }
    MediaTime maxTimeLoaded() const;
    bool isReadyForVideoSetup() const;
    virtual void setUpVideoRendering();
    virtual void tearDownVideoRendering();
    virtual bool haveBeenAskedToPaint() const { return false; }
    bool hasSetUpVideoRendering() const;

    void mainThreadCallback();
    
    void invalidateCachedDuration();

    const String& assetURL() const { return m_assetURL.string(); }

    MediaPlayer* player() { return m_player; }
    const MediaPlayer* player() const { return m_player; }

    String engineDescription() const override { return "AVFoundation"_s; }
    long platformErrorCode() const override { return assetErrorCode(); }

    void trackModeChanged() override;
#if ENABLE(AVF_CAPTIONS)
    void notifyTrackModeChanged() override { }
    virtual void synchronizeTextTrackState() { }
#endif
    void processNewAndRemovedTextTracks(const Vector<RefPtr<InbandTextTrackPrivateAVF>>&);
    void clearTextTracks();
    Vector<RefPtr<InbandTextTrackPrivateAVF>> m_textTracks;

    void setResolvedURL(URL&&);
    const URL& resolvedURL() const { return m_resolvedURL; }

    void setNeedsRenderingModeChanged();
    void renderingModeChanged();

    bool loadingMetadata() const { return m_loadingMetadata; }

    bool shouldEnableInheritURIQueryComponent() const;

private:
    MediaPlayer* m_player;

    Function<void()> m_pendingSeek;

    mutable Lock m_queuedNotificationsLock;
    Deque<Notification> m_queuedNotifications WTF_GUARDED_BY_LOCK(m_queuedNotificationsLock);

    mutable std::unique_ptr<PlatformTimeRanges> m_cachedLoadedTimeRanges;

    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;

    URL m_assetURL;
    URL m_resolvedURL;
    RefPtr<SecurityOrigin> m_requestedOrigin;
    RefPtr<SecurityOrigin> m_resolvedOrigin;

    MediaPlayer::Preload m_preload;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif

    FloatSize m_cachedNaturalSize;
    mutable MediaTime m_cachedMaxTimeLoaded;
    mutable MediaTime m_cachedMaxTimeSeekable;
    mutable MediaTime m_cachedMinTimeSeekable;
    mutable MediaTime m_cachedDuration;
    MediaTime m_reportedDuration;
    mutable MediaTime m_maxTimeLoadedAtLastDidLoadingProgress;
    mutable int m_delayCallbacks;
    int m_delayCharacteristicsChangedNotification;
    bool m_mainThreadCallPending;
    bool m_assetIsPlayable;
    bool m_visible;
    bool m_loadingMetadata;
    bool m_isAllowedToRender;
    bool m_cachedHasAudio;
    bool m_cachedHasVideo;
    bool m_cachedHasCaptions;
    bool m_ignoreLoadStateChanges;
    bool m_haveReportedFirstVideoFrame;
    bool m_inbandTrackConfigurationPending;
    bool m_characteristicsChanged;
    bool m_shouldMaintainAspectRatio;
    bool m_seeking;
    bool m_needsRenderingModeChanged { false };
};

String convertEnumerationToString(MediaPlayerPrivateAVFoundation::MediaRenderingMode);

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template<> struct EnumTraits<WebCore::MediaPlayerPrivateAVFoundation::MediaRenderingMode> {
using values = EnumValues<
    WebCore::MediaPlayerPrivateAVFoundation::MediaRenderingMode,
    WebCore::MediaPlayerPrivateAVFoundation::MediaRenderingMode::MediaRenderingNone,
    WebCore::MediaPlayerPrivateAVFoundation::MediaRenderingMode::MediaRenderingToContext,
    WebCore::MediaPlayerPrivateAVFoundation::MediaRenderingMode::MediaRenderingToLayer
    >;
};

}; // namespace WTF

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
