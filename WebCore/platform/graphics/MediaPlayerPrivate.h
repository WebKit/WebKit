/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef MediaPlayerPrivate_h
#define MediaPlayerPrivate_h

#if ENABLE(VIDEO)

#include "MediaPlayer.h"

namespace WebCore {

class IntRect;
class IntSize;
class String;

class MediaPlayerPrivateInterface {
public:
    virtual ~MediaPlayerPrivateInterface() { }

    virtual void load(const String& url) = 0;
    virtual void cancelLoad() = 0;
    
    virtual void play() = 0;
    virtual void pause() = 0;    

    virtual bool supportsFullscreen() const { return false; };

    virtual IntSize naturalSize() const = 0;

    virtual bool hasVideo() const = 0;

    virtual void setVisible(bool) = 0;

    virtual float duration() const = 0;

    virtual float currentTime() const = 0;
    virtual void seek(float time) = 0;
    virtual bool seeking() const = 0;

    virtual float startTime() const { return 0; }

    virtual void setEndTime(float) = 0;

    virtual void setRate(float) = 0;
    virtual void setPreservesPitch(bool) { }

    virtual bool paused() const = 0;

    virtual void setVolume(float) = 0;

    virtual MediaPlayer::NetworkState networkState() const = 0;
    virtual MediaPlayer::ReadyState readyState() const = 0;

    virtual float maxTimeSeekable() const = 0;
    virtual float maxTimeBuffered() const = 0;

    virtual int dataRate() const = 0;

    virtual bool totalBytesKnown() const { return totalBytes() > 0; }
    virtual unsigned totalBytes() const = 0;
    virtual unsigned bytesLoaded() const = 0;

    virtual void setSize(const IntSize&) = 0;

    virtual void paint(GraphicsContext*, const IntRect&) = 0;

    virtual void paintCurrentFrameInContext(GraphicsContext* c, const IntRect& r) { paint(c, r); }

    virtual void setAutobuffer(bool) { };

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    virtual void setPoster(const String& url) = 0;
    virtual void deliverNotification(MediaPlayerProxyNotificationType) = 0;
    virtual void setMediaPlayerProxy(WebMediaPlayerProxy*) = 0;
#endif

#if USE(ACCELERATED_COMPOSITING)
    // whether accelerated rendering is supported by the media engine for the current media.
    virtual bool supportsAcceleratedRendering() const { return false; }
    // called when the rendering system flips the into or out of accelerated rendering mode.
    virtual void acceleratedRenderingStateChanged() { }
#endif

    virtual bool hasSingleSecurityOrigin() const { return false; }

    virtual MediaPlayer::MovieLoadType movieLoadType() const { return MediaPlayer::Unknown; }

};

}

#endif
#endif
