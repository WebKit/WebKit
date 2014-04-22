/*
 * Copyright (C) 2014 Alex Christensen <achristensen@webkit.org>
 * All rights reserved.
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

#include "config.h"
#include "MediaPlayerPrivateMediaFoundation.h"

#include "GraphicsContext.h"
#include "NotImplemented.h"

#if USE(MEDIA_FOUNDATION)

namespace WebCore {

// TODO: Implement video functionality using Media Foundation

MediaPlayerPrivateMediaFoundation::MediaPlayerPrivateMediaFoundation(MediaPlayer* player) 
    : m_player(player)
{
}

PassOwnPtr<MediaPlayerPrivateInterface> MediaPlayerPrivateMediaFoundation::create(MediaPlayer* player)
{
    return adoptPtr(new MediaPlayerPrivateMediaFoundation(player));
}

void MediaPlayerPrivateMediaFoundation::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType, 0, 0, 0, 0);
}

bool MediaPlayerPrivateMediaFoundation::isAvailable() 
{
    notImplemented();
    return true;
}

void MediaPlayerPrivateMediaFoundation::getSupportedTypes(HashSet<String>& types)
{
    notImplemented();
    types = HashSet<String>();
}

MediaPlayer::SupportsType MediaPlayerPrivateMediaFoundation::supportsType(const MediaEngineSupportParameters& parameters)
{
    notImplemented();
    return MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivateMediaFoundation::load(const String&)
{
    notImplemented();
}

void MediaPlayerPrivateMediaFoundation::cancelLoad()
{
    notImplemented();
}

void MediaPlayerPrivateMediaFoundation::play()
{
    notImplemented();
}

void MediaPlayerPrivateMediaFoundation::pause()
{
    notImplemented();
}

IntSize MediaPlayerPrivateMediaFoundation::naturalSize() const 
{
    notImplemented();
    return IntSize(0, 0);
}

bool MediaPlayerPrivateMediaFoundation::hasVideo() const
{
    notImplemented();
    return false;
}

bool MediaPlayerPrivateMediaFoundation::hasAudio() const
{
    notImplemented();
    return false;
}

void MediaPlayerPrivateMediaFoundation::setVisible(bool)
{
    notImplemented();
}

bool MediaPlayerPrivateMediaFoundation::seeking() const
{
    notImplemented();
    return false;
}

bool MediaPlayerPrivateMediaFoundation::paused() const
{
    notImplemented();
    return false;
}

MediaPlayer::NetworkState MediaPlayerPrivateMediaFoundation::networkState() const
{ 
    notImplemented();
    return MediaPlayer::Empty;
}

MediaPlayer::ReadyState MediaPlayerPrivateMediaFoundation::readyState() const
{
    notImplemented();
    return MediaPlayer::HaveNothing;
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateMediaFoundation::buffered() const
{ 
    notImplemented();
    return PlatformTimeRanges::create();
}

bool MediaPlayerPrivateMediaFoundation::didLoadingProgress() const
{
    notImplemented();
    return false;
}

void MediaPlayerPrivateMediaFoundation::setSize(const IntSize&)
{
    notImplemented();
}

void MediaPlayerPrivateMediaFoundation::paint(GraphicsContext* context, const IntRect& rect)
{
    if (context->paintingDisabled()
        || !m_player->visible())
        return;

    // TODO: Paint the contents of the video to the context
    notImplemented();
}

}

#endif
