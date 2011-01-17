/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebMediaPlayerClientImpl_h
#define WebMediaPlayerClientImpl_h

#if ENABLE(VIDEO)

#include "MediaPlayerPrivate.h"
#include "VideoFrameChromium.h"
#include "VideoFrameProvider.h"
#include "VideoLayerChromium.h"
#include "WebMediaPlayerClient.h"
#include <wtf/OwnPtr.h>

namespace WebKit {

class WebMediaElement;
class WebMediaPlayer;

// This class serves as a bridge between WebCore::MediaPlayer and
// WebKit::WebMediaPlayer.
class WebMediaPlayerClientImpl : public WebCore::MediaPlayerPrivateInterface
#if USE(ACCELERATED_COMPOSITING)
                               , public WebCore::VideoFrameProvider
#endif
                               , public WebMediaPlayerClient {

public:
    static bool isEnabled();
    static void setIsEnabled(bool);
    static void registerSelf(WebCore::MediaEngineRegistrar);

    static WebMediaPlayerClientImpl* fromMediaElement(const WebMediaElement* element);

    // Returns the encapsulated WebKit::WebMediaPlayer.
    WebMediaPlayer* mediaPlayer() const;

    // WebMediaPlayerClient methods:
    virtual ~WebMediaPlayerClientImpl();
    virtual void networkStateChanged();
    virtual void readyStateChanged();
    virtual void volumeChanged(float);
    virtual void muteChanged(bool);
    virtual void timeChanged();
    virtual void repaint();
    virtual void durationChanged();
    virtual void rateChanged();
    virtual void sizeChanged();
    virtual void sawUnsupportedTracks();
    virtual float volume() const;

    // MediaPlayerPrivateInterface methods:
    virtual void load(const WTF::String& url);
    virtual void cancelLoad();
#if USE(ACCELERATED_COMPOSITING)
    virtual WebCore::PlatformLayer* platformLayer() const;
#endif
    virtual WebCore::PlatformMedia platformMedia() const;
    virtual void play();
    virtual void pause();
    virtual bool supportsFullscreen() const;
    virtual bool supportsSave() const;
    virtual WebCore::IntSize naturalSize() const;
    virtual bool hasVideo() const;
    virtual bool hasAudio() const;
    virtual void setVisible(bool);
    virtual float duration() const;
    virtual float currentTime() const;
    virtual void seek(float time);
    virtual bool seeking() const;
    virtual void setEndTime(float time);
    virtual void setRate(float);
    virtual bool paused() const;
    virtual void setVolume(float);
    virtual WebCore::MediaPlayer::NetworkState networkState() const;
    virtual WebCore::MediaPlayer::ReadyState readyState() const;
    virtual float maxTimeSeekable() const;
    virtual WTF::PassRefPtr<WebCore::TimeRanges> buffered() const;
    virtual int dataRate() const;
    virtual void setAutobuffer(bool);
    virtual bool totalBytesKnown() const;
    virtual unsigned totalBytes() const;
    virtual unsigned bytesLoaded() const;
    virtual void setSize(const WebCore::IntSize&);
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);
    virtual bool hasSingleSecurityOrigin() const;
    virtual WebCore::MediaPlayer::MovieLoadType movieLoadType() const;
#if USE(ACCELERATED_COMPOSITING)
    virtual bool supportsAcceleratedRendering() const;

    // VideoFrameProvider methods:
    virtual WebCore::VideoFrameChromium* getCurrentFrame();
    virtual void putCurrentFrame(WebCore::VideoFrameChromium*);
#endif

private:
    WebMediaPlayerClientImpl();

    static WebCore::MediaPlayerPrivateInterface* create(WebCore::MediaPlayer*);
    static void getSupportedTypes(WTF::HashSet<WTF::String>&);
    static WebCore::MediaPlayer::SupportsType supportsType(
        const WTF::String& type, const WTF::String& codecs);

    WebCore::MediaPlayer* m_mediaPlayer;
    OwnPtr<WebMediaPlayer> m_webMediaPlayer;
#if USE(ACCELERATED_COMPOSITING)
    RefPtr<WebCore::VideoLayerChromium> m_videoLayer;
    bool m_supportsAcceleratedCompositing;
#endif
    static bool m_isEnabled;
};

} // namespace WebKit

#endif

#endif
