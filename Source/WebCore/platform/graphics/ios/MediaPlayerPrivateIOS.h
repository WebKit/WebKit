/*
 * Copyright (C) 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef MediaPlayerPrivateIOS_h
#define MediaPlayerPrivateIOS_h

#if ENABLE(VIDEO) && PLATFORM(IOS)

#include "InbandTextTrackPrivateAVF.h"
#include "MediaPlayer.h"
#include "MediaPlayerPrivate.h"
#include "MediaPlayerProxy.h"
#include <objc/runtime.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS NSDictionary;
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS NSNumber;
OBJC_CLASS NSObject;
OBJC_CLASS WebCoreMediaPlayerNotificationHelper;

namespace WebCore {

class InbandTextTrackPrivateAVFIOS;

class MediaPlayerPrivateIOS : public MediaPlayerPrivateInterface, public AVFInbandTrackParent {
public:

    static void registerMediaEngine(MediaEngineRegistrar);
    virtual ~MediaPlayerPrivateIOS();

    virtual void deliverNotification(MediaPlayerProxyNotificationType) OVERRIDE;
    bool callbacksDelayed() { return m_delayCallbacks > 0; }
    virtual void prepareToPlay() OVERRIDE;
    void processDeferredRequests();

#if ENABLE(VIDEO_TRACK)
    void inbandTextTracksChanged(NSArray *);
    void processInbandTextTrackCue(NSArray *, double);
    void textTrackWasSelectedByPlugin(NSDictionary *);
#endif

private:
    MediaPlayerPrivateIOS(MediaPlayer*);

    // Engine support
    static PassOwnPtr<MediaPlayerPrivateInterface> create(MediaPlayer*);
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    static bool isAvailable();

    virtual IntSize naturalSize() const OVERRIDE;
    virtual bool hasVideo() const OVERRIDE;
    virtual bool hasAudio() const OVERRIDE;
    virtual bool supportsFullscreen() const OVERRIDE { return true; }

    virtual bool canLoadPoster() const OVERRIDE { return true; }
    virtual void setPoster(const String& url) OVERRIDE;

    virtual void setControls(bool) OVERRIDE;

    virtual void enterFullscreen() OVERRIDE;
    virtual void exitFullscreen() OVERRIDE;

    virtual bool hasClosedCaptions() const OVERRIDE;
    virtual void setClosedCaptionsVisible(bool) OVERRIDE;

    virtual void load(const String& url) OVERRIDE;
#if ENABLE(MEDIA_SOURCE)
    virtual void load(const String&, PassRefPtr<HTMLMediaSource>) OVERRIDE { }
#endif
    virtual void cancelLoad() OVERRIDE;

    virtual void play() OVERRIDE;
    virtual void pause() OVERRIDE;

    virtual bool paused() const OVERRIDE;
    virtual bool seeking() const OVERRIDE;

    virtual float duration() const OVERRIDE;
    virtual float currentTime() const OVERRIDE;

    virtual void seek(float time) OVERRIDE;
    void setEndTime(float);

    float rate() const;
    virtual void setRate(float inRate) OVERRIDE;
    virtual float volume() const OVERRIDE;
    virtual void setVolume(float inVolume) OVERRIDE;
    virtual void setMuted(bool inMute) OVERRIDE;

    int dataRate() const;

    virtual MediaPlayer::NetworkState networkState() const OVERRIDE;
    virtual MediaPlayer::ReadyState readyState() const OVERRIDE;
    
    float maxTimeBuffered() const;
    virtual float maxTimeSeekable() const OVERRIDE;
    virtual PassRefPtr<TimeRanges> buffered() const OVERRIDE;

    virtual bool didLoadingProgress() const OVERRIDE;
    bool totalBytesKnown() const;
    unsigned totalBytes() const;

    virtual void setVisible(bool) OVERRIDE;
    virtual void setSize(const IntSize&) OVERRIDE;
    
    virtual void paint(GraphicsContext*, const IntRect&) OVERRIDE;

#if ENABLE(IOS_AIRPLAY)
    virtual bool isCurrentPlaybackTargetWireless() const OVERRIDE;
    virtual void showPlaybackTargetPicker() OVERRIDE;

    virtual bool hasWirelessPlaybackTargets() const OVERRIDE;

    virtual bool wirelessVideoPlaybackDisabled() const OVERRIDE;
    virtual void setWirelessVideoPlaybackDisabled(bool) OVERRIDE;

    virtual void setHasPlaybackTargetAvailabilityListeners(bool) OVERRIDE;
#endif

#if USE(ACCELERATED_COMPOSITING)
    virtual bool supportsAcceleratedRendering() const OVERRIDE;
#endif

    virtual void setMediaPlayerProxy(WebMediaPlayerProxy*) OVERRIDE;
    void processPendingRequests();

    void setDelayCallbacks(bool doDelay) { m_delayCallbacks += (doDelay ? 1 : -1); }

    bool usingNetwork() const { return m_usingNetwork; }
    bool inFullscreen() const { return m_inFullScreen; }

    void addDeferredRequest(NSString *name, id value);

    virtual String engineDescription() const { String(ASCIILiteral("iOS")); }

    virtual void attributeChanged(const String& name, const String& value) OVERRIDE;
    virtual bool readyForPlayback() const OVERRIDE;

#if ENABLE(VIDEO_TRACK)
    virtual bool requiresTextTrackRepresentation() const;
    virtual void setTextTrackRepresentation(TextTrackRepresentation*);

    void clearTextTracks();
    void setSelectedTextTrack(NSNumber *);
    virtual void trackModeChanged() OVERRIDE;
    void setOutOfBandTextTracks(NSArray *);

    virtual bool implementsTextTrackControls() const { return true; }
    virtual PassRefPtr<PlatformTextTrackMenuInterface> textTrackMenu();

    void outOfBandTextTracksChanged();
    void textTrackWasSelectedByMediaElement(PassRefPtr<PlatformTextTrack>);
#endif

private:

    class PlatformTextTrackMenuInterfaceIOS : public PlatformTextTrackMenuInterface {
    public:
        static PassRefPtr<PlatformTextTrackMenuInterfaceIOS> create(MediaPlayerPrivateIOS* owner)
        {
            return adoptRef(new PlatformTextTrackMenuInterfaceIOS(owner));
        }

        virtual ~PlatformTextTrackMenuInterfaceIOS();

        virtual void tracksDidChange() OVERRIDE
        {
            if (m_owner)
                m_owner->outOfBandTextTracksChanged();
        }

        virtual void trackWasSelected(PassRefPtr<PlatformTextTrack> track) OVERRIDE
        {
            if (m_owner)
                m_owner->textTrackWasSelectedByMediaElement(track);
        }

        virtual void setClient(PlatformTextTrackMenuClient* client) OVERRIDE { m_client = client; }
        PlatformTextTrackMenuClient* client() { return m_client; }

    private:
        PlatformTextTrackMenuInterfaceIOS(MediaPlayerPrivateIOS*);

        PlatformTextTrackMenuClient* m_client;
        MediaPlayerPrivateIOS* m_owner;
    };

    MediaPlayer* m_mediaPlayer;
    RetainPtr<NSObject> m_mediaPlayerHelper; // This is the MediaPlayerProxy.
    RetainPtr<WebCoreMediaPlayerNotificationHelper> m_objcHelper;
    RetainPtr<NSMutableDictionary> m_deferredProperties;

    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;

    enum BufferingState { Empty, UnlikeleyToKeepUp, LikeleyToKeepUp, Full };

#if ENABLE(VIDEO_TRACK)
    InbandTextTrackPrivateAVFIOS* m_currentTrack;
    Vector<RefPtr<InbandTextTrackPrivateAVFIOS> > m_textTracks;
    RefPtr<PlatformTextTrackMenuInterfaceIOS> m_menuInterface;
#endif

    int m_delayCallbacks;
    int m_changingVolume;
    mutable unsigned m_bytesLoadedAtLastDidLoadingProgress;
    float m_requestedRate;
    BufferingState m_bufferingState;
    bool m_visible : 1;
    bool m_usingNetwork : 1;
    bool m_inFullScreen : 1;
    bool m_shouldPrepareToPlay : 1;
    bool m_preparingToPlay : 1;
    bool m_pauseRequested : 1;
};

} // namespace WebCore

#endif
#endif // MediaPlayerPrivateIOS
