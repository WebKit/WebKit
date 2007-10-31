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
#include "Movie.h"

#include "IntRect.h"
#include "MimeTypeRegistry.h"

#if PLATFORM(MAC)
#include "MoviePrivateQTKit.h"
#endif

namespace WebCore {
    
    Movie::Movie(MovieClient* client)
    : m_movieClient(client)
    , m_private(new MoviePrivate(this))
    , m_parentWidget(0)
    , m_visible(false)
    , m_rate(1.0f)
    , m_volume(0.5f)
    , m_muted(false)
{
}

Movie::~Movie()
{
    delete m_private;
}

void Movie::load(String url)
{
    m_private->load(url);
}    

void Movie::cancelLoad()
{
    m_private->cancelLoad();
}    

void Movie::play()
{
    m_private->play();
}

void Movie::pause()
{
    m_private->pause();
}

float Movie::duration() const
{
    return m_private->duration();
}

float Movie::currentTime() const
{
    return m_private->currentTime();  
}

void Movie::seek(float time)
{
    m_private->seek(time);
}

bool Movie::paused() const
{
    return m_private->paused();
}

bool Movie::seeking() const
{
    return m_private->seeking();
}

IntSize Movie::naturalSize()
{
    return m_private->naturalSize();
}

bool Movie::hasVideo()
{
    return m_private->hasVideo();
}

Movie::NetworkState Movie::networkState()
{
    return m_private->networkState();
}

Movie::ReadyState Movie::readyState()
{
    return m_private->readyState();
}

float Movie::volume() const
{
    return m_volume;
}

void Movie::setVolume(float volume)
{
    if (volume != m_volume) {
        m_volume = volume;
        m_private->setVolume(volume);   
    }
}

float Movie::rate() const
{
    return m_rate;
}

void Movie::setRate(float rate)
{
    if (rate == m_rate) 
        return;
    m_rate = rate;
    m_private->setRate(rate);   
}

bool Movie::muted() const
{
    return m_muted;
}

void Movie::setMuted(bool muted)
{
    if (muted == m_muted) 
        return;
    m_muted = muted;
    m_private->setMuted(muted);
}

int Movie::dataRate() const
{
    return m_private->dataRate();
}

void Movie::setEndTime(float time)
{
    m_private->setEndTime(time);
}

void Movie::addCuePoint(float time)
{
    if (m_cuePoints.contains(time))
        return;
    m_cuePoints.add(time);
    m_private->addCuePoint(time);
}

void Movie::removeCuePoint(float time)
{
    if (!m_cuePoints.contains(time))
        return;
    m_cuePoints.remove(time);
    m_private->removeCuePoint(time);
}

void Movie::clearCuePoints()
{
    m_cuePoints.clear();
    m_private->clearCuePoints();
}

float Movie::maxTimeBuffered()
{
    return m_private->maxTimeBuffered();
}

float Movie::maxTimeSeekable()
{
    return m_private->maxTimeSeekable();
}

unsigned Movie::bytesLoaded()
{
    return m_private->bytesLoaded();
}

bool Movie::totalBytesKnown()
{
    return m_private->totalBytesKnown();
}

unsigned Movie::totalBytes()
{
    return m_private->totalBytes();
}

void Movie::setRect(const IntRect& r) 
{ 
    if (m_rect == r)
        return;
    m_rect = r;
    m_private->setRect(r);
}

bool Movie::visible() const
{
    return m_visible;
}

void Movie::setVisible(bool b)
{
    if (m_visible == b)
        return;
    m_visible = b;
    m_private->setVisible(b);
}

void Movie::paint(GraphicsContext* p, const IntRect& r)
{
    m_private->paint(p, r);
}

void Movie::getSupportedTypes(HashSet<String>& types)
{
    MoviePrivate::getSupportedTypes(types);
} 

void Movie::networkStateChanged()
{
    if (m_movieClient)
        m_movieClient->movieNetworkStateChanged(this);
}

void Movie::readyStateChanged()
{
    if (m_movieClient)
        m_movieClient->movieReadyStateChanged(this);
}

void Movie::volumeChanged()
{
    if (m_movieClient)
        m_movieClient->movieVolumeChanged(this);
}

void Movie::didEnd()
{
    if (m_movieClient)
        m_movieClient->movieDidEnd(this);
}

void Movie::cuePointReached(float cueTime)
{
    if (m_movieClient)
        m_movieClient->movieCuePointReached(this, cueTime);
}

}
#endif
