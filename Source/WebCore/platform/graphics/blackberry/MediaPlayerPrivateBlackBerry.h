/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MediaPlayerPrivateBlackBerry_h
#define MediaPlayerPrivateBlackBerry_h

#if ENABLE(VIDEO)
#include "MediaPlayerPrivate.h"

#include <BlackBerryPlatformMMRPlayer.h>

namespace BlackBerry {
namespace WebKit {
class WebPageClient;
}
}

namespace WebCore {

class MediaPlayerPrivate : public MediaPlayerPrivateInterface, public BlackBerry::Platform::IMMRPlayerListener {
public:
    virtual ~MediaPlayerPrivate();

    static PassOwnPtr<MediaPlayerPrivateInterface> create(MediaPlayer*);
    static void registerMediaEngine(MediaEngineRegistrar);
    static void getSupportedTypes(HashSet<String>&);
    static MediaPlayer::SupportsType supportsType(const String&, const String&);
    static void notifyAppActivatedEvent(bool);
    static void setCertificatePath(const String&);

    virtual void load(const String& url);
    virtual void cancelLoad();

    virtual void prepareToPlay();
#if USE(ACCELERATED_COMPOSITING)
    virtual PlatformMedia platformMedia() const;
    virtual PlatformLayer* platformLayer() const;
    void drawBufferingAnimation(const TransformationMatrix&, int positionLocation, int texCoordLocation);
#endif

    virtual void play();
    virtual void pause();

    virtual bool supportsFullscreen() const;

    virtual IntSize naturalSize() const;

    virtual bool hasVideo() const;
    virtual bool hasAudio() const;

    virtual void setVisible(bool);

    virtual float duration() const;

    virtual float currentTime() const;
    virtual void seek(float time);
    virtual bool seeking() const;

    virtual void setRate(float);

    virtual bool paused() const;

    virtual void setVolume(float);

    virtual MediaPlayer::NetworkState networkState() const;
    virtual MediaPlayer::ReadyState readyState() const;

    virtual float maxTimeSeekable() const;
    virtual PassRefPtr<TimeRanges> buffered() const;

    virtual unsigned bytesLoaded() const;

    virtual void setSize(const IntSize&);

    virtual void paint(GraphicsContext*, const IntRect&);

    virtual bool hasAvailableVideoFrame() const;

#if USE(ACCELERATED_COMPOSITING)
    // Whether accelerated rendering is supported by the media engine for the current media.
    virtual bool supportsAcceleratedRendering() const { return true; }
    // Called when the rendering system flips the into or out of accelerated rendering mode.
    virtual void acceleratedRenderingStateChanged() { }
#endif

    virtual bool hasSingleSecurityOrigin() const;

    virtual MediaPlayer::MovieLoadType movieLoadType() const;

    void resizeSourceDimensions();
    void setFullscreenWebPageClient(BlackBerry::WebKit::WebPageClient*);
    BlackBerry::Platform::Graphics::Window* getWindow();
    BlackBerry::Platform::Graphics::Window* getPeerWindow(const char*) const;
    int getWindowPosition(unsigned& x, unsigned& y, unsigned& width, unsigned& height) const;
    const char* mmrContextName();
    float percentLoaded();
    unsigned sourceWidth();
    unsigned sourceHeight();
    void setAllowPPSVolumeUpdates(bool);

    // IMMRPlayerListener implementation.
    virtual void onStateChanged(BlackBerry::Platform::MMRPlayer::MpState);
    virtual void onMediaStatusChanged(BlackBerry::Platform::MMRPlayer::MMRPlayState);
    virtual void onError(BlackBerry::Platform::MMRPlayer::Error);
    virtual void onDurationChanged(float);
    virtual void onTimeChanged(float);
    virtual void onRateChanged(float);
    virtual void onVolumeChanged(float);
    virtual void onPauseStateChanged();
    virtual void onRepaint();
    virtual void onSizeChanged();
    virtual void onPlayNotified();
    virtual void onPauseNotified();
    virtual void onWaitMetadataNotified(bool hasFinished, int timeWaited);
#if USE(ACCELERATED_COMPOSITING)
    virtual void onBuffering(bool);
#endif

    virtual bool isFullscreen() const;
    virtual bool isTabVisible() const;
    virtual int showErrorDialog(BlackBerry::Platform::MMRPlayer::Error);
    virtual BlackBerry::Platform::Graphics::Window* platformWindow();

private:
    MediaPlayerPrivate(MediaPlayer*);

    FrameView* frameView() const;
    void updateStates();
    String userAgent(const String&) const;

    MediaPlayer* m_webCorePlayer;
    BlackBerry::Platform::MMRPlayer* m_platformPlayer;

    mutable MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;

    BlackBerry::WebKit::WebPageClient* m_fullscreenWebPageClient;
#if USE(ACCELERATED_COMPOSITING)
    void bufferingTimerFired(Timer<MediaPlayerPrivate>*);
    void setBuffering(bool);

    Timer<MediaPlayerPrivate> m_bufferingTimer;
    RefPtr<PlatformLayer> m_platformLayer;
    bool m_showBufferingImage;
    bool m_mediaIsBuffering;
#endif

    void userDrivenSeekTimerFired(Timer<MediaPlayerPrivate>*);
    Timer<MediaPlayerPrivate> m_userDrivenSeekTimer;
    float m_lastSeekTime;
    void waitMetadataTimerFired(Timer<MediaPlayerPrivate>*);
    Timer<MediaPlayerPrivate> m_waitMetadataTimer;
    int m_waitMetadataPopDialogCounter;
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
#endif // MediaPlayerPrivateBlackBerry_h
