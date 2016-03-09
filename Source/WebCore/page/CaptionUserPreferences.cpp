/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
#include "CaptionUserPreferences.h"

#if ENABLE(VIDEO_TRACK)

#include "AudioTrackList.h"
#include "DOMWrapperWorld.h"
#include "Page.h"
#include "PageGroup.h"
#include "Settings.h"
#include "TextTrackList.h"
#include "UserContentController.h"
#include "UserContentTypes.h"
#include "UserStyleSheet.h"
#include "UserStyleSheetTypes.h"
#include <runtime/JSCellInlines.h>
#include <runtime/StructureInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

CaptionUserPreferences::CaptionUserPreferences(PageGroup& group)
    : m_pageGroup(group)
    , m_displayMode(ForcedOnly)
    , m_timer(*this, &CaptionUserPreferences::timerFired)
    , m_testingMode(false)
    , m_havePreferences(false)
{
}

CaptionUserPreferences::~CaptionUserPreferences()
{
}

void CaptionUserPreferences::timerFired()
{
    captionPreferencesChanged();
}

void CaptionUserPreferences::beginBlockingNotifications()
{
    ++m_blockNotificationsCounter;
}

void CaptionUserPreferences::endBlockingNotifications()
{
    ASSERT(m_blockNotificationsCounter);
    --m_blockNotificationsCounter;
}

void CaptionUserPreferences::notify()
{
    if (m_blockNotificationsCounter)
        return;

    m_havePreferences = true;
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
    if (m_testingMode && mode != AlwaysOn) {
        setUserPrefersCaptions(false);
        setUserPrefersSubtitles(false);
    }
    notify();
}

bool CaptionUserPreferences::userPrefersCaptions() const
{
    Page* page = *(m_pageGroup.pages().begin());
    if (!page)
        return false;

    return page->settings().shouldDisplayCaptions();
}

void CaptionUserPreferences::setUserPrefersCaptions(bool preference)
{
    Page* page = *(m_pageGroup.pages().begin());
    if (!page)
        return;

    page->settings().setShouldDisplayCaptions(preference);
    notify();
}

bool CaptionUserPreferences::userPrefersSubtitles() const
{
    Page* page = *(pageGroup().pages().begin());
    if (!page)
        return false;

    return page->settings().shouldDisplaySubtitles();
}

void CaptionUserPreferences::setUserPrefersSubtitles(bool preference)
{
    Page* page = *(m_pageGroup.pages().begin());
    if (!page)
        return;

    page->settings().setShouldDisplaySubtitles(preference);
    notify();
}

bool CaptionUserPreferences::userPrefersTextDescriptions() const
{
    Page* page = *(m_pageGroup.pages().begin());
    if (!page)
        return false;
    
    return page->settings().shouldDisplayTextDescriptions();
}

void CaptionUserPreferences::setUserPrefersTextDescriptions(bool preference)
{
    Page* page = *(m_pageGroup.pages().begin());
    if (!page)
        return;
    
    page->settings().setShouldDisplayTextDescriptions(preference);
    notify();
}

void CaptionUserPreferences::captionPreferencesChanged()
{
    m_pageGroup.captionPreferencesChanged();
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

void CaptionUserPreferences::setPreferredAudioCharacteristic(const String& characteristic)
{
    m_userPreferredAudioCharacteristic = characteristic;
    notify();
}

Vector<String> CaptionUserPreferences::preferredAudioCharacteristics() const
{
    Vector<String> characteristics;
    if (!m_userPreferredAudioCharacteristic.isEmpty())
        characteristics.append(m_userPreferredAudioCharacteristic);
    return characteristics;
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

static String trackDisplayName(AudioTrack* track)
{
    if (track->label().isEmpty() && track->language().isEmpty())
        return audioTrackNoLabelText();
    if (!track->label().isEmpty())
        return track->label();
    return track->language();
}

String CaptionUserPreferences::displayNameForTrack(AudioTrack* track) const
{
    return trackDisplayName(track);
}

Vector<RefPtr<AudioTrack>> CaptionUserPreferences::sortedTrackListForMenu(AudioTrackList* trackList)
{
    ASSERT(trackList);

    Vector<RefPtr<AudioTrack>> tracksForMenu;

    for (unsigned i = 0, length = trackList->length(); i < length; ++i) {
        AudioTrack* track = trackList->item(i);
        tracksForMenu.append(track);
    }

    std::sort(tracksForMenu.begin(), tracksForMenu.end(), [](const RefPtr<AudioTrack>& a, const RefPtr<AudioTrack>& b) {
        return codePointCompare(trackDisplayName(a.get()), trackDisplayName(b.get())) < 0;
    });

    return tracksForMenu;
}

int CaptionUserPreferences::textTrackSelectionScore(TextTrack* track, HTMLMediaElement*) const
{
    if (track->kind() != TextTrack::captionsKeyword() && track->kind() != TextTrack::subtitlesKeyword())
        return 0;
    
    if (!userPrefersSubtitles() && !userPrefersCaptions())
        return 0;
    
    return textTrackLanguageSelectionScore(track, preferredLanguages()) + 1;
}

int CaptionUserPreferences::textTrackLanguageSelectionScore(TextTrack* track, const Vector<String>& preferredLanguages) const
{
    if (track->language().isEmpty())
        return 0;

    bool exactMatch;
    size_t languageMatchIndex = indexOfBestMatchingLanguageInList(track->language(), preferredLanguages, exactMatch);
    if (languageMatchIndex >= preferredLanguages.size())
        return 0;

    // Matching a track language is more important than matching track type, so this multiplier must be
    // greater than the maximum value returned by textTrackSelectionScore.
    int bonus = exactMatch ? 1 : 0;
    return (preferredLanguages.size() + bonus - languageMatchIndex) * 10;
}

void CaptionUserPreferences::setCaptionsStyleSheetOverride(const String& override)
{
    m_captionsStyleSheetOverride = override;
    updateCaptionStyleSheetOveride();
}

void CaptionUserPreferences::updateCaptionStyleSheetOveride()
{
    String captionsOverrideStyleSheet = captionsStyleSheetOverride();
    for (auto& page : m_pageGroup.pages())
        page->setCaptionUserPreferencesStyleSheet(captionsOverrideStyleSheet);
}

String CaptionUserPreferences::primaryAudioTrackLanguageOverride() const
{
    if (!m_primaryAudioTrackLanguageOverride.isEmpty())
        return m_primaryAudioTrackLanguageOverride;
    return defaultLanguage();
}
    
}

#endif // ENABLE(VIDEO_TRACK)
