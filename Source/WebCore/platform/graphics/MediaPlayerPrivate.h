/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "MediaPlayer.h"
#include "PlatformTimeRanges.h"

namespace WebCore {

class MediaPlayerPrivateInterface {
    WTF_MAKE_NONCOPYABLE(MediaPlayerPrivateInterface); WTF_MAKE_FAST_ALLOCATED;
public:
    MediaPlayerPrivateInterface() = default;
    virtual ~MediaPlayerPrivateInterface() = default;

    virtual void load(const String& url) = 0;
#if ENABLE(MEDIA_SOURCE)
    virtual void load(const String& url, MediaSourcePrivateClient*) = 0;
#endif
#if ENABLE(MEDIA_STREAM)
    virtual void load(MediaStreamPrivate&) = 0;
#endif
    virtual void cancelLoad() = 0;
    
    virtual void prepareToPlay() { }
    virtual PlatformLayer* platformLayer() const { return 0; }

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    virtual void setVideoFullscreenLayer(PlatformLayer*, WTF::Function<void()>&& completionHandler) { completionHandler(); }
    virtual void updateVideoFullscreenInlineImage() { }
    virtual void setVideoFullscreenFrame(FloatRect) { }
    virtual void setVideoFullscreenGravity(MediaPlayer::VideoGravity) { }
    virtual void setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode) { }
    virtual void videoFullscreenStandbyChanged() { }
#endif

#if PLATFORM(IOS_FAMILY)
    virtual NSArray *timedMetadata() const { return 0; }
    virtual String accessLog() const { return emptyString(); }
    virtual String errorLog() const { return emptyString(); }
#endif
    virtual long platformErrorCode() const { return 0; }

    virtual void play() = 0;
    virtual void pause() = 0;    
    virtual void setShouldBufferData(bool) { }

    virtual bool supportsPictureInPicture() const { return false; }
    virtual bool supportsFullscreen() const { return false; }
    virtual bool supportsScanning() const { return false; }
    virtual bool requiresImmediateCompositing() const { return false; }

    virtual bool canSaveMediaData() const { return false; }

    virtual FloatSize naturalSize() const = 0;

    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;

    virtual void setVisible(bool) = 0;

    virtual float duration() const { return 0; }
    virtual double durationDouble() const { return duration(); }
    virtual MediaTime durationMediaTime() const { return MediaTime::createWithDouble(durationDouble()); }

    virtual float currentTime() const { return 0; }
    virtual double currentTimeDouble() const { return currentTime(); }
    virtual MediaTime currentMediaTime() const { return MediaTime::createWithDouble(currentTimeDouble()); }

    virtual MediaTime getStartDate() const { return MediaTime::createWithDouble(std::numeric_limits<double>::quiet_NaN()); }

    virtual void seek(float) { }
    virtual void seekDouble(double time) { seek(time); }
    virtual void seek(const MediaTime& time) { seekDouble(time.toDouble()); }
    virtual void seekWithTolerance(const MediaTime& time, const MediaTime&, const MediaTime&) { seek(time); }

    virtual bool seeking() const = 0;

    virtual MediaTime startTime() const { return MediaTime::zeroTime(); }
    virtual MediaTime initialTime() const { return MediaTime::zeroTime(); }

    virtual void setRate(float) { }
    virtual void setRateDouble(double rate) { setRate(rate); }
    virtual double rate() const { return 0; }

    virtual void setPreservesPitch(bool) { }

    virtual bool paused() const = 0;

    virtual void setVolume(float) { }
    virtual void setVolumeDouble(double volume) { return setVolume(volume); }
#if PLATFORM(IOS_FAMILY) || USE(GSTREAMER)
    virtual float volume() const { return 1; }
#endif

    virtual bool supportsMuting() const { return false; }
    virtual void setMuted(bool) { }

    virtual bool hasClosedCaptions() const { return false; }    
    virtual void setClosedCaptionsVisible(bool) { }

    virtual double maxFastForwardRate() const { return std::numeric_limits<double>::infinity(); }
    virtual double minFastReverseRate() const { return -std::numeric_limits<double>::infinity(); }

    virtual MediaPlayer::NetworkState networkState() const = 0;
    virtual MediaPlayer::ReadyState readyState() const = 0;

    virtual std::unique_ptr<PlatformTimeRanges> seekable() const { return maxMediaTimeSeekable() == MediaTime::zeroTime() ? std::make_unique<PlatformTimeRanges>() : std::make_unique<PlatformTimeRanges>(minMediaTimeSeekable(), maxMediaTimeSeekable()); }
    virtual float maxTimeSeekable() const { return 0; }
    virtual MediaTime maxMediaTimeSeekable() const { return MediaTime::createWithDouble(maxTimeSeekable()); }
    virtual double minTimeSeekable() const { return 0; }
    virtual MediaTime minMediaTimeSeekable() const { return MediaTime::createWithDouble(minTimeSeekable()); }
    virtual std::unique_ptr<PlatformTimeRanges> buffered() const = 0;
    virtual double seekableTimeRangesLastModifiedTime() const { return 0; }
    virtual double liveUpdateInterval() const { return 0; }

    virtual unsigned long long totalBytes() const { return 0; }
    virtual bool didLoadingProgress() const = 0;

    virtual void setSize(const IntSize&) = 0;

    virtual void paint(GraphicsContext&, const FloatRect&) = 0;

    virtual void paintCurrentFrameInContext(GraphicsContext& c, const FloatRect& r) { paint(c, r); }
    virtual bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject, GC3Denum, GC3Dint, GC3Denum, GC3Denum, GC3Denum, bool, bool) { return false; }
    virtual NativeImagePtr nativeImageForCurrentTime() { return nullptr; }

    virtual void setPreload(MediaPlayer::Preload) { }

    virtual bool hasAvailableVideoFrame() const { return readyState() >= MediaPlayer::HaveCurrentData; }

    virtual bool canLoadPoster() const { return false; }
    virtual void setPoster(const String&) { }

#if USE(NATIVE_FULLSCREEN_VIDEO)
    virtual void enterFullscreen() { }
    virtual void exitFullscreen() { }
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

    virtual String wirelessPlaybackTargetName() const { return emptyString(); }
    virtual MediaPlayer::WirelessPlaybackTargetType wirelessPlaybackTargetType() const { return MediaPlayer::TargetTypeNone; }

    virtual bool wirelessVideoPlaybackDisabled() const { return true; }
    virtual void setWirelessVideoPlaybackDisabled(bool) { }

    virtual bool canPlayToWirelessPlaybackTarget() const { return false; }
    virtual bool isCurrentPlaybackTargetWireless() const { return false; }
    virtual void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) { }

    virtual void setShouldPlayToPlaybackTarget(bool) { }
#endif

#if USE(NATIVE_FULLSCREEN_VIDEO)
    virtual bool canEnterFullscreen() const { return false; }
#endif

    // whether accelerated rendering is supported by the media engine for the current media.
    virtual bool supportsAcceleratedRendering() const { return false; }
    // called when the rendering system flips the into or out of accelerated rendering mode.
    virtual void acceleratedRenderingStateChanged() { }

    virtual bool shouldMaintainAspectRatio() const { return true; }
    virtual void setShouldMaintainAspectRatio(bool) { }

    virtual bool hasSingleSecurityOrigin() const { return false; }
    virtual bool didPassCORSAccessCheck() const { return false; }
    virtual Optional<bool> wouldTaintOrigin(const SecurityOrigin&) const { return WTF::nullopt; }

    virtual MediaPlayer::MovieLoadType movieLoadType() const { return MediaPlayer::Unknown; }

    virtual void prepareForRendering() { }

    // Time value in the movie's time scale. It is only necessary to override this if the media
    // engine uses rational numbers to represent media time.
    virtual MediaTime mediaTimeForTimeValue(const MediaTime& timeValue) const { return timeValue; }

    // Overide this if it is safe for HTMLMediaElement to cache movie time and report
    // 'currentTime' as [cached time + elapsed wall time]. Returns the maximum wall time
    // it is OK to calculate movie time before refreshing the cached time.
    virtual double maximumDurationToCacheMediaTime() const { return 0; }

    virtual unsigned decodedFrameCount() const { return 0; }
    virtual unsigned droppedFrameCount() const { return 0; }
    virtual unsigned audioDecodedByteCount() const { return 0; }
    virtual unsigned videoDecodedByteCount() const { return 0; }

    HashSet<RefPtr<SecurityOrigin>> originsInMediaCache(const String&) { return { }; }
    void clearMediaCache(const String&, WallTime) { }
    void clearMediaCacheForOrigins(const String&, const HashSet<RefPtr<SecurityOrigin>>&) { }

    virtual void setPrivateBrowsingMode(bool) { }

    virtual String engineDescription() const { return emptyString(); }

#if ENABLE(WEB_AUDIO)
    virtual AudioSourceProvider* audioSourceProvider() { return 0; }
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    virtual std::unique_ptr<LegacyCDMSession> createSession(const String&, LegacyCDMSessionClient*) { return nullptr; }
    virtual void setCDMSession(LegacyCDMSession*) { }
    virtual void keyAdded() { }
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    virtual void cdmInstanceAttached(CDMInstance&) { }
    virtual void cdmInstanceDetached(CDMInstance&) { }
    virtual void attemptToDecryptWithInstance(CDMInstance&) { }
    virtual bool waitingForKey() const { return false; }
#endif

#if ENABLE(VIDEO_TRACK)
    virtual bool requiresTextTrackRepresentation() const { return false; }
    virtual void setTextTrackRepresentation(TextTrackRepresentation*) { }
    virtual void syncTextTrackBounds() { };
    virtual void tracksChanged() { };
#endif

#if USE(GSTREAMER)
    virtual void simulateAudioInterruption() { }
#endif

    virtual void beginSimulatedHDCPError() { }
    virtual void endSimulatedHDCPError() { }

    virtual String languageOfPrimaryAudioTrack() const { return emptyString(); }

    virtual size_t extraMemoryCost() const
    {
        MediaTime duration = this->durationMediaTime();
        if (!duration)
            return 0;

        unsigned long long extra = totalBytes() * buffered()->totalDuration().toDouble() / duration.toDouble();
        return static_cast<unsigned>(extra);
    }

    virtual unsigned long long fileSize() const { return 0; }

    virtual bool ended() const { return false; }

    virtual Optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() { return WTF::nullopt; }

#if ENABLE(AVF_CAPTIONS)
    virtual void notifyTrackModeChanged() { }
#endif

    virtual void notifyActiveSourceBuffersChanged() { }

    virtual void setShouldDisableSleep(bool) { }

    virtual void applicationWillResignActive() { }
    virtual void applicationDidBecomeActive() { }

#if USE(AVFOUNDATION)
    virtual AVPlayer *objCAVFoundationAVPlayer() const { return nullptr; }
#endif

    virtual bool performTaskAtMediaTime(WTF::Function<void()>&&, MediaTime) { return false; }
};

}

#endif
