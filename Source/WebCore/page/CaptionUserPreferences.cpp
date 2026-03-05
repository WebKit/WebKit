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
#include <JavaScriptCore/JSObjectInlines.h>
#include <ranges>
#include <wtf/Language.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unicode/Collator.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CaptionUserPreferences);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CaptionUserPreferencesTestingModeToken);

Ref<CaptionUserPreferences> CaptionUserPreferences::create(PageGroup& group)
{
    return adoptRef(*new CaptionUserPreferences(group));
}

CaptionUserPreferences::CaptionUserPreferences(PageGroup& group)
    : m_pageGroup(group)
    , m_displayMode(CaptionDisplayMode::ForcedOnly)
    , m_timer(*this, &CaptionUserPreferences::timerFired)
{
}

CaptionUserPreferences::~CaptionUserPreferences() = default;

UniqueRef<CaptionUserPreferencesTestingModeToken> CaptionUserPreferences::createTestingModeToken()
{
    return makeUniqueRef<CaptionUserPreferencesTestingModeToken>(*this);
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
        m_timer.startOneShot(0_s);
}

CaptionUserPreferences::CaptionDisplayMode CaptionUserPreferences::captionDisplayMode() const
{
    return m_displayMode;
}

void CaptionUserPreferences::setCaptionDisplayMode(CaptionUserPreferences::CaptionDisplayMode mode)
{
    m_displayMode = mode;
    if (testingMode() && mode != CaptionDisplayMode::AlwaysOn) {
        setUserPrefersCaptions(false);
        setUserPrefersSubtitles(false);
    }
    notify();
}

RefPtr<Page> CaptionUserPreferences::currentPage() const
{
    for (Ref page : m_pageGroup->pages())
        return page;
    return nullptr;
}

bool CaptionUserPreferences::userPrefersCaptions() const
{
    RefPtr page = currentPage();
    if (!page)
        return false;

    return page->settings().shouldDisplayCaptions();
}

void CaptionUserPreferences::setUserPrefersCaptions(bool preference)
{
    RefPtr page = currentPage();
    if (!page)
        return;

    page->settings().setShouldDisplayCaptions(preference);
    notify();
}

bool CaptionUserPreferences::userPrefersSubtitles() const
{
    RefPtr page = currentPage();
    if (!page)
        return false;

    return page->settings().shouldDisplaySubtitles();
}

void CaptionUserPreferences::setUserPrefersSubtitles(bool preference)
{
    RefPtr page = currentPage();
    if (!page)
        return;

    page->settings().setShouldDisplaySubtitles(preference);
    notify();
}

bool CaptionUserPreferences::userPrefersTextDescriptions() const
{
    RefPtr page = currentPage();
    if (!page)
        return false;

    Ref settings = page->settings();
    return settings->shouldDisplayTextDescriptions() && (settings->audioDescriptionsEnabled() || settings->extendedAudioDescriptionsEnabled());
}

void CaptionUserPreferences::setUserPrefersTextDescriptions(bool preference)
{
    RefPtr page = currentPage();
    if (!page)
        return;
    
    page->settings().setShouldDisplayTextDescriptions(preference);
    notify();
}

void CaptionUserPreferences::captionPreferencesChanged()
{
    CheckedRef { m_pageGroup.get() }->captionPreferencesChanged();
}

Vector<String> CaptionUserPreferences::preferredLanguages() const
{
    Vector<String> languages = userPreferredLanguages(ShouldMinimizeLanguages::No);
    if (testingMode() && !m_userPreferredLanguage.isEmpty())
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
    if (testingMode() && !m_preferredAudioCharacteristicsForTesting.isEmpty())
        return m_preferredAudioCharacteristicsForTesting;

    Vector<String> characteristics;
    if (!m_userPreferredAudioCharacteristic.isEmpty())
        characteristics.append(m_userPreferredAudioCharacteristic);
    return characteristics;
}

static String trackDisplayName(const TextTrack& track)
{
    if (&track == &TextTrack::captionMenuOffItemSingleton())
        return textTrackOffMenuItemText();
    if (&track == &TextTrack::captionMenuOnItemSingleton())
        return textTrackOnMenuItemText();
    if (&track == &TextTrack::captionMenuAutomaticItemSingleton())
        return textTrackAutomaticMenuItemText();

    if (auto label = track.label().string().trim(isASCIIWhitespace); !label.isEmpty())
        return track.label();
    if (auto languageIdentifier = track.validBCP47Language(); !languageIdentifier.isEmpty())
        return languageIdentifier;
    return trackNoLabelText();
}

String CaptionUserPreferences::displayNameForTrack(const TextTrack& track) const
{
    return trackDisplayName(track);
}

MediaSelectionOption CaptionUserPreferences::mediaSelectionOptionForTrack(const TextTrack& track) const
{
    auto legibleType = MediaSelectionOption::LegibleType::Regular;
    if (&track == &TextTrack::captionMenuOffItemSingleton())
        legibleType = MediaSelectionOption::LegibleType::LegibleOff;
    else if (&track == &TextTrack::captionMenuOnItemSingleton())
        legibleType = MediaSelectionOption::LegibleType::LegibleOn;
    else if (&track == &TextTrack::captionMenuAutomaticItemSingleton())
        legibleType = MediaSelectionOption::LegibleType::LegibleAuto;

    auto mediaType = MediaSelectionOption::MediaType::Unknown;
    switch (track.kind()) {
    case TextTrack::Kind::Forced:
    case TextTrack::Kind::Descriptions:
    case TextTrack::Kind::Subtitles:
        mediaType = MediaSelectionOption::MediaType::Subtitles;
        break;
    case TextTrack::Kind::Captions:
        mediaType = MediaSelectionOption::MediaType::Captions;
        break;
    case TextTrack::Kind::Metadata:
        mediaType = MediaSelectionOption::MediaType::Metadata;
        break;
    case TextTrack::Kind::Chapters:
        ASSERT_NOT_REACHED();
        break;
    }

    return { mediaType, displayNameForTrack(track), legibleType, track.validBCP47Language() };
}
    
Vector<Ref<TextTrack>> CaptionUserPreferences::sortedTrackListForMenu(TextTrackList* trackList, HashSet<TextTrack::Kind> kinds)
{
    ASSERT(trackList);

    Vector<Ref<TextTrack>> tracksForMenu;

    for (unsigned i = 0, length = trackList->length(); i < length; ++i) {
        Ref track = *trackList->item(i);
        if (kinds.contains(track->kind()))
            tracksForMenu.append(WTF::move(track));
    }

    Collator collator;

    std::ranges::sort(tracksForMenu, [&](auto& a, auto& b) {
        return collator.collate(trackDisplayName(a.get()), trackDisplayName(b.get())) < 0;
    });

    if (kinds.contains(TextTrack::Kind::Subtitles) || kinds.contains(TextTrack::Kind::Captions) || kinds.contains(TextTrack::Kind::Descriptions)) {
        tracksForMenu.insert(0, TextTrack::captionMenuOffItemSingleton());
        tracksForMenu.insert(1, TextTrack::captionMenuAutomaticItemSingleton());
    }

    return tracksForMenu;
}

static String trackDisplayName(const AudioTrack& track)
{
    if (auto label = track.label().string().trim(isASCIIWhitespace); !label.isEmpty())
        return track.label();
    if (auto languageIdentifier = track.validBCP47Language(); !languageIdentifier.isEmpty())
        return languageIdentifier;
    return trackNoLabelText();
}

String CaptionUserPreferences::displayNameForTrack(const AudioTrack& track) const
{
    return trackDisplayName(track);
}

MediaSelectionOption CaptionUserPreferences::mediaSelectionOptionForTrack(const AudioTrack& track) const
{
    return { MediaSelectionOption::MediaType::Audio, displayNameForTrack(track), MediaSelectionOption::LegibleType::Regular, track.validBCP47Language() };
}

Vector<Ref<AudioTrack>> CaptionUserPreferences::sortedTrackListForMenu(AudioTrackList* trackList)
{
    ASSERT(trackList);

    Vector<Ref<AudioTrack>> tracksForMenu;

    for (unsigned i = 0, length = trackList->length(); i < length; ++i)
        tracksForMenu.append(Ref { trackList->item(i) });

    Collator collator;

    std::ranges::sort(tracksForMenu, [&](auto& a, auto& b) {
        return collator.collate(trackDisplayName(a.get()), trackDisplayName(b.get())) < 0;
    });

    return tracksForMenu;
}

int CaptionUserPreferences::textTrackSelectionScore(TextTrack& track, HTMLMediaElement& mediaElement) const
{
    RefPtr firstEnabledAudioTrack = mediaElement.audioTracks() ? mediaElement.audioTracks()->firstEnabled() : nullptr;
    return textTrackSelectionScore(track, captionDisplayMode(), firstEnabledAudioTrack.get());
}

int CaptionUserPreferences::textTrackSelectionScore(TextTrack& track, CaptionDisplayMode displayMode, AudioTrack* enabledAudioTrack) const
{
    auto kind = track.kind();
    auto prefersTextDescriptions = kind == TextTrack::Kind::Descriptions && userPrefersTextDescriptions();
    if (displayMode == CaptionDisplayMode::Manual && !prefersTextDescriptions)
        return 0;

    if (displayMode == CaptionDisplayMode::AlwaysOn && (!userPrefersSubtitles() && !userPrefersCaptions()))
        return 0;

    if (kind != TextTrack::Kind::Captions && kind != TextTrack::Kind::Subtitles && kind != TextTrack::Kind::Forced && !prefersTextDescriptions)
        return 0;
    if (!track.isMainProgramContent() && !prefersTextDescriptions)
        return 0;

    bool trackHasOnlyForcedSubtitles = track.containsOnlyForcedSubtitles();
    if (((trackHasOnlyForcedSubtitles && displayMode != CaptionDisplayMode::ForcedOnly) || (!trackHasOnlyForcedSubtitles && displayMode == CaptionDisplayMode::ForcedOnly)))
        return 0;

    Vector<String> userPreferredCaptionLanguages = preferredLanguages();

    if ((displayMode == CaptionDisplayMode::Automatic) || trackHasOnlyForcedSubtitles || prefersTextDescriptions) {

        String textTrackLanguage = track.validBCP47Language();
        if (textTrackLanguage.isEmpty())
            return 0;

        Vector<String> languageList;
        languageList.reserveCapacity(1);

        String audioTrackLanguage;
        if (testingMode())
            audioTrackLanguage = primaryAudioTrackLanguageOverride();
        else if (enabledAudioTrack)
            audioTrackLanguage = enabledAudioTrack->language();

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

    if (kind == TextTrack::Kind::Descriptions && userPrefersTextDescriptions())
        trackScore = 3;
    else if (userPrefersCaptions()) {
        // When the user prefers accessibility tracks, rank is SDH, then CC, then subtitles.
        if (kind == TextTrack::Kind::Subtitles)
            trackScore = 1;
        else if (track.isClosedCaptions())
            trackScore = 2;
        else
            trackScore = 3;
    } else {
        // When the user prefers translation tracks, rank is subtitles, then SDH, then CC tracks.
        if (kind == TextTrack::Kind::Subtitles)
            trackScore = 3;
        else if (!track.isClosedCaptions())
            trackScore = 2;
        else
            trackScore = 1;
    }

    return trackScore + textTrackLanguageSelectionScore(track, userPreferredCaptionLanguages);
}

int CaptionUserPreferences::textTrackLanguageSelectionScore(TextTrack& track, const Vector<String>& preferredLanguages) const
{
    if (track.validBCP47Language().isEmpty())
        return 0;

    bool exactMatch;
    size_t languageMatchIndex = indexOfBestMatchingLanguageInList(track.validBCP47Language(), preferredLanguages, exactMatch);
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
    for (Ref page : m_pageGroup->pages())
        page->setCaptionUserPreferencesStyleSheet(captionsOverrideStyleSheet);
}

String CaptionUserPreferences::primaryAudioTrackLanguageOverride() const
{
    if (!m_primaryAudioTrackLanguageOverride.isEmpty())
        return m_primaryAudioTrackLanguageOverride;
    return defaultLanguage(ShouldMinimizeLanguages::No);
}

String CaptionUserPreferences::captionPreviewTitle() const
{
    if (testingMode())
        return "This is a preview"_s;

    return captionStylePreview();
}

PageGroup& CaptionUserPreferences::pageGroup() const
{
    return m_pageGroup.get();
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
