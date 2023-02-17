/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include "InspectorMediaPlayer.h"

#include "HTMLMediaElement.h"
#include "MediaPlayer.h"
#include <variant>
#include <wtf/Function.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

using namespace Inspector;

Ref<InspectorMediaPlayer> InspectorMediaPlayer::create(HTMLMediaElement& mediaElement)
{
    return adoptRef(*new InspectorMediaPlayer(mediaElement));
}

InspectorMediaPlayer::InspectorMediaPlayer(HTMLMediaElement& mediaElement)
    : m_mediaElement(mediaElement)
{
}

String InspectorMediaPlayer::identifier() const
{
    return m_mediaElement->identifier().loggingString();
}

HTMLMediaElement* InspectorMediaPlayer::mediaElement() const
{
    return m_mediaElement.get();
}

MediaPlayer* InspectorMediaPlayer::mediaPlayer() const
{
    return m_mediaElement->player().get();
}

void InspectorMediaPlayer::play()
{
    m_mediaElement->play();
}

void InspectorMediaPlayer::pause()
{
    m_mediaElement->pause();
}

Ref<Inspector::Protocol::Media::MediaPlayer> InspectorMediaPlayer::buildObjectForMediaPlayer()
{
    auto player = Protocol::Media::MediaPlayer::create()
        .setPlayerId(identifier())
        .setOriginUrl(mediaPlayer()->url().string())
        .setContentType(mediaPlayer()->contentMIMEType())
        .setDuration(mediaPlayer()->duration().toJSONObject())
        .setEngine(mediaPlayer()->engineDescription())
        .release();;
    
    return player;
}

} // namespace WebCore

