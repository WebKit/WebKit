/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "CaptionUserPreferences.h"

#include "HTMLMediaElement.h"
#include "Page.h"
#include "PageCache.h"
#include "Settings.h"
#include "TextTrackList.h"

namespace WebCore {

CaptionUserPreferences::CaptionUserPreferences()
    : m_timer(this, &CaptionUserPreferences::timerFired)
    , m_displayMode(ForcedOnly)
    , m_testingMode(false)
{
}

CaptionUserPreferences::~CaptionUserPreferences()
{
}

void CaptionUserPreferences::timerFired(Timer<CaptionUserPreferences>&)
{
    captionPreferencesChanged();
}

void CaptionUserPreferences::notify()
{
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

CaptionUserPreferences::CaptionDisplayMode CaptionUserPreferences::captionDisplayMode() const
{
    return m_displayMode;
}

void CaptionUserPreferences::setCaptionDisplayMode(CaptionUserPreferences::CaptionDisplayMode mode)
{
    m_displayMode = mode;
    notify();
}

bool CaptionUserPreferences::userPrefersCaptions(Document& document) const
{
    Settings* settings = document.settings();
    if (!settings)
        return false;

    return settings->shouldDisplayCaptions();
}

bool CaptionUserPreferences::userPrefersSubtitles(Document& document) const
{
    Settings* settings = document.settings();
    if (!settings)
        return false;

    return settings->shouldDisplaySubtitles();
}

bool CaptionUserPreferences::userPrefersTextDescriptions(Document& document) const
{
    Settings* settings = document.settings();
    if (!settings)
        return false;

    return settings->shouldDisplayTextDescriptions();
}

Vector<String> CaptionUserPreferences::preferredLanguages() const
{
    Vector<String> languages = userPreferredLanguages();
    if (m_testingMode && !m_userPreferredLanguage.isEmpty())
        languages.insert(0, m_userPreferredLanguage);

    return languages;
}

void CaptionUserPreferences::setPreferredLanguage(const String& language)
{
    m_userPreferredLanguage = language;
    notify();
}

static String trackDisplayName(TextTrack* track)
{
    if (track == TextTrack::captionMenuOffItem())
        return textTrackOffMenuItemText();
    if (track == TextTrack::captionMenuAutomaticItem())
        return textTrackAutomaticMenuItemText();

    if (track->label().isEmpty() && track->language().isEmpty())
        return textTrackNoLabelText();
    if (!track->label().isEmpty())
        return track->label();
    return track->language();
}

String CaptionUserPreferences::displayNameForTrack(TextTrack* track) const
{
    return trackDisplayName(track);
}
    
Vector<RefPtr<TextTrack>> CaptionUserPreferences::sortedTrackListForMenu(TextTrackList* trackList)
{
    ASSERT(trackList);

    Vector<RefPtr<TextTrack>> tracksForMenu;

    for (unsigned i = 0, length = trackList->length(); i < length; ++i) {
        TextTrack* track = trackList->item(i);
        const AtomicString& kind = track->kind();
        if (kind == TextTrack::captionsKeyword() || kind == TextTrack::descriptionsKeyword() || kind == TextTrack::subtitlesKeyword())
            tracksForMenu.append(track);
    }

    std::sort(tracksForMenu.begin(), tracksForMenu.end(), [](const RefPtr<TextTrack>& a, const RefPtr<TextTrack>& b) {
        return codePointCompare(trackDisplayName(a.get()), trackDisplayName(b.get())) < 0;
    });

    tracksForMenu.insert(0, TextTrack::captionMenuOffItem());
    tracksForMenu.insert(1, TextTrack::captionMenuAutomaticItem());

    return tracksForMenu;
}

int CaptionUserPreferences::textTrackSelectionScore(TextTrack* track, HTMLMediaElement* element) const
{
    int trackScore = 0;

    if (track->kind() != TextTrack::captionsKeyword() && track->kind() != TextTrack::subtitlesKeyword())
        return trackScore;
    
    if (!userPrefersSubtitles(element->document()) && !userPrefersCaptions(element->document()))
        return trackScore;
    
    if (track->kind() == TextTrack::subtitlesKeyword() && userPrefersSubtitles(element->document()))
        trackScore = 1;
    else if (track->kind() == TextTrack::captionsKeyword() && userPrefersCaptions(element->document()))
        trackScore = 1;
    
    return trackScore + textTrackLanguageSelectionScore(track, preferredLanguages());
}

int CaptionUserPreferences::textTrackLanguageSelectionScore(TextTrack* track, const Vector<String>& preferredLanguages) const
{
    if (track->language().isEmpty())
        return 0;

    size_t languageMatchIndex = indexOfBestMatchingLanguageInList(track->language(), preferredLanguages);
    if (languageMatchIndex >= preferredLanguages.size())
        return 0;

    // Matching a track language is more important than matching track type, so this multiplier must be
    // greater than the maximum value returned by textTrackSelectionScore.
    return (preferredLanguages.size() - languageMatchIndex) * 10;
}

String CaptionUserPreferences::captionsStyleSheet()
{
    if (!m_captionsStyleSheetOverride.isEmpty() || testingMode())
        return m_captionsStyleSheetOverride;

    return platformCaptionsStyleSheet();
}

void CaptionUserPreferences::setCaptionsStyleSheetOverride(const String& override)
{
    m_captionsStyleSheetOverride = override;
    captionPreferencesChanged();
}

String CaptionUserPreferences::captionsStyleSheetOverride() const
{
    return m_captionsStyleSheetOverride;
}

String CaptionUserPreferences::primaryAudioTrackLanguageOverride() const
{
    if (!m_primaryAudioTrackLanguageOverride.isEmpty())
        return m_primaryAudioTrackLanguageOverride;
    return defaultLanguage();
}

void CaptionUserPreferences::captionPreferencesChanged()
{
    Page::updateStyleForAllPagesForCaptionPreferencesChanged();
    pageCache()->markPagesForCaptionPreferencesChanged();
}

} // namespace WebCore

#endif // ENABLE(VIDEO_TRACK)
