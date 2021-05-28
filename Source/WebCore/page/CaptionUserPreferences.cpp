/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "AudioTrackList.h"
#include "DOMWrapperWorld.h"
#include "HTMLMediaElement.h"
#include "LocalizedStrings.h"
#include "MediaSelectionOption.h"
#include "Page.h"
#include "PageGroup.h"
#include "Settings.h"
#include "TextTrackList.h"
#include "UserContentController.h"
#include "UserContentTypes.h"
#include "UserStyleSheet.h"
#include "UserStyleSheetTypes.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCellInlines.h>
#include <JavaScriptCore/StructureInlines.h>
#include <wtf/Language.h>
#include <wtf/unicode/Collator.h>

namespace WebCore {

CaptionUserPreferences::CaptionUserPreferences(PageGroup& group)
    : m_pageGroup(group)
    , m_displayMode(ForcedOnly)
    , m_timer(*this, &CaptionUserPreferences::timerFired)
{
}

CaptionUserPreferences::~CaptionUserPreferences() = default;

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
        m_timer.startOneShot(0_s);
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

Page* CaptionUserPreferences::currentPage() const
{
    for (auto& page : m_pageGroup.pages())
        return &page;
    return nullptr;
}

bool CaptionUserPreferences::userPrefersCaptions() const
{
    auto* page = currentPage();
    if (!page)
        return false;

    return page->settings().shouldDisplayCaptions();
}

void CaptionUserPreferences::setUserPrefersCaptions(bool preference)
{
    auto* page = currentPage();
    if (!page)
        return;

    page->settings().setShouldDisplayCaptions(preference);
    notify();
}

bool CaptionUserPreferences::userPrefersSubtitles() const
{
    auto* page = currentPage();
    if (!page)
        return false;

    return page->settings().shouldDisplaySubtitles();
}

void CaptionUserPreferences::setUserPrefersSubtitles(bool preference)
{
    auto* page = currentPage();
    if (!page)
        return;

    page->settings().setShouldDisplaySubtitles(preference);
    notify();
}

bool CaptionUserPreferences::userPrefersTextDescriptions() const
{
    auto* page = currentPage();
    if (!page)
        return false;
    
    return page->settings().shouldDisplayTextDescriptions();
}

void CaptionUserPreferences::setUserPrefersTextDescriptions(bool preference)
{
    auto* page = currentPage();
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
    Vector<String> languages = userPreferredLanguages(ShouldMinimizeLanguages::No);
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
    if (track == &TextTrack::captionMenuOffItem())
        return textTrackOffMenuItemText();
    if (track == &TextTrack::captionMenuAutomaticItem())
        return textTrackAutomaticMenuItemText();

    if (track->label().isEmpty() && track->validBCP47Language().isEmpty())
        return trackNoLabelText();
    if (!track->label().isEmpty())
        return track->label();
    return track->validBCP47Language();
}

String CaptionUserPreferences::displayNameForTrack(TextTrack* track) const
{
    return trackDisplayName(track);
}

MediaSelectionOption CaptionUserPreferences::mediaSelectionOptionForTrack(TextTrack* track) const
{
    auto type = MediaSelectionOption::Type::Regular;
    if (track == &TextTrack::captionMenuOffItem())
        type = MediaSelectionOption::Type::LegibleOff;
    else if (track == &TextTrack::captionMenuAutomaticItem())
        type = MediaSelectionOption::Type::LegibleAuto;
    return { displayNameForTrack(track), type };
}
    
Vector<RefPtr<TextTrack>> CaptionUserPreferences::sortedTrackListForMenu(TextTrackList* trackList, HashSet<TextTrack::Kind> kinds)
{
    ASSERT(trackList);

    Vector<RefPtr<TextTrack>> tracksForMenu;

    for (unsigned i = 0, length = trackList->length(); i < length; ++i) {
        TextTrack* track = trackList->item(i);
        if (kinds.contains(track->kind()))
            tracksForMenu.append(track);
    }

    Collator collator;

    std::sort(tracksForMenu.begin(), tracksForMenu.end(), [&] (auto& a, auto& b) {
        return collator.collate(trackDisplayName(a.get()), trackDisplayName(b.get())) < 0;
    });

    if (kinds.contains(TextTrack::Kind::Subtitles) || kinds.contains(TextTrack::Kind::Captions) || kinds.contains(TextTrack::Kind::Descriptions)) {
        tracksForMenu.insert(0, &TextTrack::captionMenuOffItem());
        tracksForMenu.insert(1, &TextTrack::captionMenuAutomaticItem());
    }

    return tracksForMenu;
}

static String trackDisplayName(AudioTrack* track)
{
    if (track->label().isEmpty() && track->validBCP47Language().isEmpty())
        return trackNoLabelText();
    if (!track->label().isEmpty())
        return track->label();
    return track->validBCP47Language();
}

String CaptionUserPreferences::displayNameForTrack(AudioTrack* track) const
{
    return trackDisplayName(track);
}

MediaSelectionOption CaptionUserPreferences::mediaSelectionOptionForTrack(AudioTrack* track) const
{
    return { displayNameForTrack(track), MediaSelectionOption::Type::Regular };
}

Vector<RefPtr<AudioTrack>> CaptionUserPreferences::sortedTrackListForMenu(AudioTrackList* trackList)
{
    ASSERT(trackList);

    Vector<RefPtr<AudioTrack>> tracksForMenu;

    for (unsigned i = 0, length = trackList->length(); i < length; ++i) {
        AudioTrack* track = trackList->item(i);
        tracksForMenu.append(track);
    }

    Collator collator;

    std::sort(tracksForMenu.begin(), tracksForMenu.end(), [&] (auto& a, auto& b) {
        return collator.collate(trackDisplayName(a.get()), trackDisplayName(b.get())) < 0;
    });

    return tracksForMenu;
}

int CaptionUserPreferences::textTrackSelectionScore(TextTrack* track, HTMLMediaElement* mediaElement) const
{
    CaptionDisplayMode displayMode = captionDisplayMode();
    if (displayMode == Manual)
        return 0;

    bool legacyOverride = mediaElement->webkitClosedCaptionsVisible();
    if (displayMode == AlwaysOn && (!userPrefersSubtitles() && !userPrefersCaptions() && !legacyOverride))
        return 0;
    if (track->kind() != TextTrack::Kind::Captions && track->kind() != TextTrack::Kind::Subtitles && track->kind() != TextTrack::Kind::Forced)
        return 0;
    if (!track->isMainProgramContent())
        return 0;

    bool trackHasOnlyForcedSubtitles = track->containsOnlyForcedSubtitles();
    if (!legacyOverride && ((trackHasOnlyForcedSubtitles && displayMode != ForcedOnly) || (!trackHasOnlyForcedSubtitles && displayMode == ForcedOnly)))
        return 0;

    Vector<String> userPreferredCaptionLanguages = preferredLanguages();

    if ((displayMode == Automatic && !legacyOverride) || trackHasOnlyForcedSubtitles) {

        if (!mediaElement || !mediaElement->player())
            return 0;

        String textTrackLanguage = track->validBCP47Language();
        if (textTrackLanguage.isEmpty())
            return 0;

        Vector<String> languageList;
        languageList.reserveCapacity(1);

        String audioTrackLanguage;
        if (testingMode())
            audioTrackLanguage = primaryAudioTrackLanguageOverride();
        else
            audioTrackLanguage = mediaElement->player()->languageOfPrimaryAudioTrack();

        if (audioTrackLanguage.isEmpty())
            return 0;

        bool exactMatch;
        if (trackHasOnlyForcedSubtitles) {
            languageList.append(audioTrackLanguage);
            size_t offset = indexOfBestMatchingLanguageInList(textTrackLanguage, languageList, exactMatch);

            // Only consider a forced-only track if it IS in the same language as the primary audio track.
            if (offset)
                return 0;
        } else {
            languageList.append(defaultLanguage(ShouldMinimizeLanguages::No));

            // Only enable a text track if the current audio track is NOT in the user's preferred language ...
            size_t offset = indexOfBestMatchingLanguageInList(audioTrackLanguage, languageList, exactMatch);
            if (!offset)
                return 0;

            // and the text track matches the user's preferred language.
            offset = indexOfBestMatchingLanguageInList(textTrackLanguage, languageList, exactMatch);
            if (offset)
                return 0;
        }

        userPreferredCaptionLanguages = languageList;
    }

    int trackScore = 0;

    if (userPrefersCaptions()) {
        // When the user prefers accessibility tracks, rank is SDH, then CC, then subtitles.
        if (track->kind() == TextTrack::Kind::Subtitles)
            trackScore = 1;
        else if (track->isClosedCaptions())
            trackScore = 2;
        else
            trackScore = 3;
    } else {
        // When the user prefers translation tracks, rank is subtitles, then SDH, then CC tracks.
        if (track->kind() == TextTrack::Kind::Subtitles)
            trackScore = 3;
        else if (!track->isClosedCaptions())
            trackScore = 2;
        else
            trackScore = 1;
    }

    return trackScore + textTrackLanguageSelectionScore(track, userPreferredCaptionLanguages);
}

int CaptionUserPreferences::textTrackLanguageSelectionScore(TextTrack* track, const Vector<String>& preferredLanguages) const
{
    if (track->validBCP47Language().isEmpty())
        return 0;

    bool exactMatch;
    size_t languageMatchIndex = indexOfBestMatchingLanguageInList(track->validBCP47Language(), preferredLanguages, exactMatch);
    if (languageMatchIndex >= preferredLanguages.size())
        return 0;

    // Matching a track language is more important than matching track type, so this multiplier must be
    // greater than the maximum value returned by textTrackSelectionScore.
    int bonus = exactMatch ? 1 : 0;
    return (preferredLanguages.size() + bonus - languageMatchIndex) * 10;
}

void CaptionUserPreferences::setCaptionsStyleSheetOverride(const String& override)
{
    if (override == m_captionsStyleSheetOverride)
        return;

    m_captionsStyleSheetOverride = override;
    updateCaptionStyleSheetOverride();
    if (!m_timer.isActive())
        m_timer.startOneShot(0_s);
}

void CaptionUserPreferences::updateCaptionStyleSheetOverride()
{
    String captionsOverrideStyleSheet = captionsStyleSheetOverride();
    for (auto& page : m_pageGroup.pages())
        page.setCaptionUserPreferencesStyleSheet(captionsOverrideStyleSheet);
}

String CaptionUserPreferences::primaryAudioTrackLanguageOverride() const
{
    if (!m_primaryAudioTrackLanguageOverride.isEmpty())
        return m_primaryAudioTrackLanguageOverride;
    return defaultLanguage(ShouldMinimizeLanguages::No);
}
    
}

#endif // ENABLE(VIDEO)
