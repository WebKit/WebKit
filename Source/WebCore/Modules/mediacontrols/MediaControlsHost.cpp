/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_CONTROLS_SCRIPT)

#include "MediaControlsHost.h"

#include "CaptionUserPreferences.h"
#include "Element.h"
#include "HTMLMediaElement.h"
#include "Logging.h"
#include "MediaControlElements.h"
#include "Page.h"
#include "PageGroup.h"
#include "TextTrack.h"
#include "TextTrackList.h"
#include <runtime/JSCJSValueInlines.h>

namespace WebCore {

const AtomicString& MediaControlsHost::automaticKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, automatic, ("automatic", AtomicString::ConstructFromLiteral));
    return automatic;
}

const AtomicString& MediaControlsHost::forcedOnlyKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, forcedOn, ("forced-only", AtomicString::ConstructFromLiteral));
    return forcedOn;
}

const AtomicString& MediaControlsHost::alwaysOnKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, alwaysOn, ("always-on", AtomicString::ConstructFromLiteral));
    return alwaysOn;
}

PassRefPtr<MediaControlsHost> MediaControlsHost::create(HTMLMediaElement* mediaElement)
{
    return adoptRef(new MediaControlsHost(mediaElement));
}

MediaControlsHost::MediaControlsHost(HTMLMediaElement* mediaElement)
    : m_mediaElement(mediaElement)
{
    ASSERT(mediaElement);
}

MediaControlsHost::~MediaControlsHost()
{
}

Vector<RefPtr<TextTrack>> MediaControlsHost::sortedTrackListForMenu(TextTrackList* trackList)
{
    if (!trackList)
        return Vector<RefPtr<TextTrack>>();

    Page* page = m_mediaElement->document().page();
    if (!page)
        return Vector<RefPtr<TextTrack>>();

    CaptionUserPreferences* captionPreferences = page->group().captionPreferences();
    return captionPreferences->sortedTrackListForMenu(trackList);
}

String MediaControlsHost::displayNameForTrack(TextTrack* track)
{
    if (!track)
        return emptyString();

    Page* page = m_mediaElement->document().page();
    if (!page)
        return emptyString();

    CaptionUserPreferences* captionPreferences = page->group().captionPreferences();
    return captionPreferences->displayNameForTrack(track);
}

TextTrack* MediaControlsHost::captionMenuOffItem()
{
    return TextTrack::captionMenuOffItem();
}

TextTrack* MediaControlsHost::captionMenuAutomaticItem()
{
    return TextTrack::captionMenuAutomaticItem();
}

AtomicString MediaControlsHost::captionDisplayMode()
{
    Page* page = m_mediaElement->document().page();
    if (!page)
        return emptyAtom;

    switch (page->group().captionPreferences()->captionDisplayMode()) {
    case CaptionUserPreferences::Automatic:
        return automaticKeyword();
    case CaptionUserPreferences::ForcedOnly:
        return forcedOnlyKeyword();
    case CaptionUserPreferences::AlwaysOn:
        return alwaysOnKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyAtom;
    }
}

void MediaControlsHost::setSelectedTextTrack(TextTrack* track)
{
    m_mediaElement->setSelectedTextTrack(track);
}

Element* MediaControlsHost::textTrackContainer()
{
    if (!m_textTrackContainer) {
        m_textTrackContainer = MediaControlTextTrackContainerElement::create(m_mediaElement->document());
        m_textTrackContainer->setMediaController(m_mediaElement);
    }
    return m_textTrackContainer.get();
}

void MediaControlsHost::updateTextTrackContainer()
{
    if (m_textTrackContainer)
        m_textTrackContainer->updateDisplay();
}

bool MediaControlsHost::mediaPlaybackAllowsInline() const
{
    return !m_mediaElement->mediaSession().requiresFullscreenForVideoPlayback(*m_mediaElement);
}

bool MediaControlsHost::supportsFullscreen()
{
    return m_mediaElement->supportsFullscreen();
}

bool MediaControlsHost::userGestureRequired() const
{
    return !m_mediaElement->mediaSession().playbackPermitted(*m_mediaElement);
}

String MediaControlsHost::externalDeviceDisplayName() const
{
#if ENABLE(IOS_AIRPLAY)
    MediaPlayer* player = m_mediaElement->player();
    if (!player) {
        LOG(Media, "MediaControlsHost::externalDeviceDisplayName - returning \"\" because player is NULL");
        return emptyString();
    }
    
    String name = player->wirelessPlaybackTargetName();
    LOG(Media, "MediaControlsHost::externalDeviceDisplayName - returning \"%s\"", name.utf8().data());
    
    return name;
#else
    return emptyString();
#endif
}

String MediaControlsHost::externalDeviceType() const
{
    DEPRECATED_DEFINE_STATIC_LOCAL(String, none, (ASCIILiteral("none")));
    String type = none;
    
#if ENABLE(IOS_AIRPLAY)
    DEPRECATED_DEFINE_STATIC_LOCAL(String, airplay, (ASCIILiteral("airplay")));
    DEPRECATED_DEFINE_STATIC_LOCAL(String, tvout, (ASCIILiteral("tvout")));
    
    MediaPlayer* player = m_mediaElement->player();
    if (!player) {
        LOG(Media, "MediaControlsHost::externalDeviceType - returning \"none\" because player is NULL");
        return none;
    }
    
    switch (player->wirelessPlaybackTargetType()) {
    case MediaPlayer::TargetTypeNone:
        type = none;
        break;
    case MediaPlayer::TargetTypeAirPlay:
        type = airplay;
        break;
    case MediaPlayer::TargetTypeTVOut:
        type = tvout;
        break;
    }
#endif
    
    LOG(Media, "MediaControlsHost::externalDeviceType - returning \"%s\"", type.utf8().data());
    
    return type;
}

bool MediaControlsHost::controlsDependOnPageScaleFactor() const
{
    return m_mediaElement->mediaControlsDependOnPageScaleFactor();
}

void MediaControlsHost::setControlsDependOnPageScaleFactor(bool value)
{
    m_mediaElement->setMediaControlsDependOnPageScaleFactor(value);
}

}

#endif
