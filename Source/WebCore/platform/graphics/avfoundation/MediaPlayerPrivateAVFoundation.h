/*
 * Copyright (C) 2011-2014 Apple Inc. All rights reserved.
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

#ifndef MediaPlayerPrivateAVFoundation_h
#define MediaPlayerPrivateAVFoundation_h

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#include "FloatSize.h"
#include "InbandTextTrackPrivateAVF.h"
#include "MediaPlayerPrivate.h"
#include "Timer.h"
#include <functional>
#include <wtf/Functional.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class InbandMetadataTextTrackPrivateAVF;
class InbandTextTrackPrivateAVF;
class GenericCueData;

class MediaPlayerPrivateAVFoundation : public MediaPlayerPrivateInterface, public AVFInbandTrackParent
{
public:

    virtual void repaint();
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
#if ENABLE(IOS_AIRPLAY)
    void playbackTargetIsWirelessChanged();
#endif
    
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

        Notification(std::function<void ()> function)
            : m_type(FunctionType)
            , m_finished(false)
            , m_function(function)
        {
        }
        
        Type type() { return m_type; }
        bool isValid() { return m_type != None; }
        MediaTime time() { return m_time; }
        bool finished() { return m_finished; }
        std::function<void ()>& function() { return m_function; }
        
    private:
        Type m_type;
        MediaTime m_time;
        bool m_finished;
        std::function<void ()> m_function;
    };

    void scheduleMainThreadNotification(Notification);
    void scheduleMainThreadNotification(Notification::Type, const MediaTime& = MediaTime::zeroTime());
    void scheduleMainThreadNotification(Notification::Type, bool completed);
    void dispatchNotification();
    void clearMainThreadPendingFlag();

#if ENABLE(ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA_V2)
    static bool extractKeyURIKeyIDAndCertificateFromInitData(Uint8Array* initData, String& keyURI, String& keyID, RefPtr<Uint8Array>& certificate);
#endif

protected:
    MediaPlayerPrivateAVFoundation(MediaPlayer*);
    virtual ~MediaPlayerPrivateAVFoundation();

    WeakPtr<MediaPlayerPrivateAVFoundation> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

    // MediaPlayerPrivatePrivateInterface overrides.
    virtual void load(const String& url) override;
#if ENABLE(MEDIA_SOURCE)
    virtual void load(const String&, MediaSourcePrivateClient*);
#endif
    virtual void cancelLoad() = 0;

    virtual void prepareToPlay() override;
    virtual PlatformMedia platformMedia() const = 0;

    virtual void play() override;
    virtual void pause() override;

    virtual IntSize naturalSize() const override;
    virtual bool hasVideo() const override { return m_cachedHasVideo; }
    virtual bool hasAudio() const override { return m_cachedHasAudio; }
    virtual void setVisible(bool) override;
    virtual MediaTime durationMediaTime() const override;
    virtual MediaTime currentMediaTime() const = 0;
    virtual void seek(const MediaTime&) override;
    virtual void seekWithTolerance(const MediaTime&, const MediaTime&, const MediaTime&) override;
    virtual bool seeking() const override;
    virtual void setRate(float) override;
    virtual bool paused() const override;
    virtual void setVolume(float) = 0;
    virtual bool hasClosedCaptions() const override { return m_cachedHasCaptions; }
    virtual void setClosedCaptionsVisible(bool) = 0;
    virtual MediaPlayer::NetworkState networkState() const override { return m_networkState; }
    virtual MediaPlayer::ReadyState readyState() const override { return m_readyState; }
    virtual MediaTime maxMediaTimeSeekable() const override;
    virtual MediaTime minMediaTimeSeekable() const override;
    virtual std::unique_ptr<PlatformTimeRanges> buffered() const override;
    virtual bool didLoadingProgress() const override;
    virtual void setSize(const IntSize&) override;
    virtual void paint(GraphicsContext*, const IntRect&) = 0;
    virtual void paintCurrentFrameInContext(GraphicsContext*, const IntRect&) = 0;
    virtual void setPreload(MediaPlayer::Preload) override;
    virtual PlatformLayer* platformLayer() const { return 0; }
    virtual bool supportsAcceleratedRendering() const = 0;
    virtual void acceleratedRenderingStateChanged() override;
    virtual bool shouldMaintainAspectRatio() const override { return m_shouldMaintainAspectRatio; }
    virtual void setShouldMaintainAspectRatio(bool) override;

    virtual MediaPlayer::MovieLoadType movieLoadType() const;
    virtual void prepareForRendering();

    virtual bool supportsFullscreen() const;
    virtual bool supportsScanning() const { return true; }
    unsigned long long fileSize() const { return totalBytes(); }

    // Required interfaces for concrete derived classes.
    virtual void createAVAssetForURL(const String&) = 0;
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

    virtual void platformSetVisible(bool) = 0;
    virtual void platformPlay() = 0;
    virtual void platformPause() = 0;
    virtual void checkPlayability() = 0;
    virtual void updateRate() = 0;
    virtual float rate() const = 0;
    virtual void seekToTime(const MediaTime&, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance) = 0;
    virtual unsigned long long totalBytes() const = 0;
    virtual std::unique_ptr<PlatformTimeRanges> platformBufferedTimeRanges() const = 0;
    virtual MediaTime platformMaxTimeSeekable() const = 0;
    virtual MediaTime platformMinTimeSeekable() const = 0;
    virtual MediaTime platformMaxTimeLoaded() const = 0;
    virtual MediaTime platformDuration() const = 0;

    virtual void beginLoadingMetadata() = 0;
    virtual void tracksChanged() = 0;
    virtual void sizeChanged() = 0;

    virtual void createContextVideoRenderer() = 0;
    virtual void destroyContextVideoRenderer() = 0;

    virtual void createVideoLayer() = 0;
    virtual void destroyVideoLayer() = 0;

    virtual bool hasAvailableVideoFrame() const = 0;

    virtual bool hasContextRenderer() const = 0;
    virtual bool hasLayerRenderer() const = 0;

    virtual void updateVideoLayerGravity() = 0;

protected:
    void updateStates();

    void setHasVideo(bool);
    void setHasAudio(bool);
    void setHasClosedCaptions(bool);
    void characteristicsChanged();
    void setDelayCharacteristicsChangedNotification(bool);
    void setDelayCallbacks(bool) const;
    void setIgnoreLoadStateChanges(bool delay) { m_ignoreLoadStateChanges = delay; }
    void setNaturalSize(IntSize);
    bool isLiveStream() const { return std::isinf(duration()); }

    enum MediaRenderingMode { MediaRenderingNone, MediaRenderingToContext, MediaRenderingToLayer };
    MediaRenderingMode currentRenderingMode() const;
    MediaRenderingMode preferredRenderingMode() const;

    bool metaDataAvailable() const { return m_readyState >= MediaPlayer::HaveMetadata; }
    float requestedRate() const { return m_requestedRate; }
    MediaTime maxTimeLoaded() const;
    bool isReadyForVideoSetup() const;
    virtual void setUpVideoRendering();
    virtual void tearDownVideoRendering();
    bool hasSetUpVideoRendering() const;

    static void mainThreadCallback(void*);
    
    void invalidateCachedDuration();

    const String& assetURL() const { return m_assetURL; }

    MediaPlayer* player() { return m_player; }

    virtual String engineDescription() const { return "AVFoundation"; }

    virtual void trackModeChanged() override;
#if ENABLE(AVF_CAPTIONS)
    virtual void notifyTrackModeChanged() { }
    virtual void synchronizeTextTrackState() { }
#endif
    void processNewAndRemovedTextTracks(const Vector<RefPtr<InbandTextTrackPrivateAVF>>&);
    void clearTextTracks();
    Vector<RefPtr<InbandTextTrackPrivateAVF>> m_textTracks;

private:
    MediaPlayer* m_player;

    WeakPtrFactory<MediaPlayerPrivateAVFoundation> m_weakPtrFactory;

    std::function<void()> m_pendingSeek;

    Vector<Notification> m_queuedNotifications;
    mutable Mutex m_queueMutex;

    mutable std::unique_ptr<PlatformTimeRanges> m_cachedLoadedTimeRanges;

    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;

    String m_assetURL;
    MediaPlayer::Preload m_preload;

    IntSize m_cachedNaturalSize;
    mutable MediaTime m_cachedMaxTimeLoaded;
    mutable MediaTime m_cachedMaxTimeSeekable;
    mutable MediaTime m_cachedMinTimeSeekable;
    mutable MediaTime m_cachedDuration;
    MediaTime m_reportedDuration;
    mutable MediaTime m_maxTimeLoadedAtLastDidLoadingProgress;
    float m_requestedRate;
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
    bool m_playWhenFramesAvailable;
    bool m_inbandTrackConfigurationPending;
    bool m_characteristicsChanged;
    bool m_shouldMaintainAspectRatio;
    bool m_seeking;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)

#endif // MediaPlayerPrivateAVFoundation_h
