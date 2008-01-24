/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "config.h"

#if ENABLE(VIDEO)
#include "MediaPlayer.h"

#include "IntRect.h"
#include "MIMETypeRegistry.h"

#if PLATFORM(MAC)
#include "MediaPlayerPrivateQTKit.h"
#elif PLATFORM(WIN)
#include "MediaPlayerPrivateQuickTimeWin.h"
#elif PLATFORM(GTK)
#include "MediaPlayerPrivateGStreamer.h"
#endif

namespace WebCore {
    
    MediaPlayer::MediaPlayer(MediaPlayerClient* client)
    : m_mediaPlayerClient(client)
    , m_private(new MediaPlayerPrivate(this))
    , m_parentWidget(0)
    , m_visible(false)
    , m_rate(1.0f)
    , m_volume(0.5f)
{
}

MediaPlayer::~MediaPlayer()
{
    delete m_private;
}

void MediaPlayer::load(String url)
{
    m_private->load(url);
}    

void MediaPlayer::cancelLoad()
{
    m_private->cancelLoad();
}    

void MediaPlayer::play()
{
    m_private->play();
}

void MediaPlayer::pause()
{
    m_private->pause();
}

float MediaPlayer::duration() const
{
    return m_private->duration();
}

float MediaPlayer::currentTime() const
{
    return m_private->currentTime();  
}

void MediaPlayer::seek(float time)
{
    m_private->seek(time);
}

bool MediaPlayer::paused() const
{
    return m_private->paused();
}

bool MediaPlayer::seeking() const
{
    return m_private->seeking();
}

IntSize MediaPlayer::naturalSize()
{
    return m_private->naturalSize();
}

bool MediaPlayer::hasVideo()
{
    return m_private->hasVideo();
}

MediaPlayer::NetworkState MediaPlayer::networkState()
{
    return m_private->networkState();
}

MediaPlayer::ReadyState MediaPlayer::readyState()
{
    return m_private->readyState();
}

float MediaPlayer::volume() const
{
    return m_volume;
}

void MediaPlayer::setVolume(float volume)
{
    if (volume != m_volume) {
        m_volume = volume;
        m_private->setVolume(volume);   
    }
}

float MediaPlayer::rate() const
{
    return m_rate;
}

void MediaPlayer::setRate(float rate)
{
    if (rate == m_rate) 
        return;
    m_rate = rate;
    m_private->setRate(rate);   
}

int MediaPlayer::dataRate() const
{
    return m_private->dataRate();
}

void MediaPlayer::setEndTime(float time)
{
    m_private->setEndTime(time);
}

float MediaPlayer::maxTimeBuffered()
{
    return m_private->maxTimeBuffered();
}

float MediaPlayer::maxTimeSeekable()
{
    return m_private->maxTimeSeekable();
}

unsigned MediaPlayer::bytesLoaded()
{
    return m_private->bytesLoaded();
}

bool MediaPlayer::totalBytesKnown()
{
    return m_private->totalBytesKnown();
}

unsigned MediaPlayer::totalBytes()
{
    return m_private->totalBytes();
}

void MediaPlayer::setRect(const IntRect& r) 
{ 
    if (m_rect == r)
        return;
    m_rect = r;
    m_private->setRect(r);
}

bool MediaPlayer::visible() const
{
    return m_visible;
}

void MediaPlayer::setVisible(bool b)
{
    if (m_visible == b)
        return;
    m_visible = b;
    m_private->setVisible(b);
}

void MediaPlayer::paint(GraphicsContext* p, const IntRect& r)
{
    m_private->paint(p, r);
}

void MediaPlayer::getSupportedTypes(HashSet<String>& types)
{
    MediaPlayerPrivate::getSupportedTypes(types);
} 
    
bool MediaPlayer::isAvailable()
{
    static bool availabityKnown = false;
    static bool isAvailable;
    if (!availabityKnown) {
        isAvailable = MediaPlayerPrivate::isAvailable();
        availabityKnown = true;
    }
    return isAvailable;
} 

void MediaPlayer::networkStateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerNetworkStateChanged(this);
}

void MediaPlayer::readyStateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerReadyStateChanged(this);
}

void MediaPlayer::volumeChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerVolumeChanged(this);
}

void MediaPlayer::timeChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerTimeChanged(this);
}

void MediaPlayer::repaint()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerRepaint(this);
}

}
#endif
