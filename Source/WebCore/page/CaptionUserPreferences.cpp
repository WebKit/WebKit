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
#include "Page.h"
#include "PageGroup.h"
#include "Settings.h"
#include "TextTrackList.h"
#include <wtf/NonCopyingSort.h>

namespace WebCore {

CaptionUserPreferences::CaptionUserPreferences(PageGroup* group)
    : m_pageGroup(group)
    , m_timer(this, &CaptionUserPreferences::timerFired)
    , m_testingMode(false)
    , m_havePreferences(false)
    , m_shouldShowCaptions(false)
{
}

CaptionUserPreferences::~CaptionUserPreferences()
{
}

bool CaptionUserPreferences::shouldShowCaptions() const
{
    return m_testingMode ? m_shouldShowCaptions : false;
}

void CaptionUserPreferences::timerFired(Timer<CaptionUserPreferences>*)
{
    captionPreferencesChanged();
}

void CaptionUserPreferences::notify()
{
    if (!m_testingMode)
        return;

    m_havePreferences = true;
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void CaptionUserPreferences::setShouldShowCaptions(bool preference)
{
    m_shouldShowCaptions = preference;
    notify();
}

bool CaptionUserPreferences::userPrefersCaptions() const
{
    Page* page = *(pageGroup()->pages().begin());
    if (!page)
        return false;

    return page->settings()->shouldDisplayCaptions();
}

void CaptionUserPreferences::setUserPrefersCaptions(bool preference)
{
    Page* page = *(pageGroup()->pages().begin());
    if (!page)
        return;

    page->settings()->setShouldDisplayCaptions(preference);
    notify();
}

bool CaptionUserPreferences::userPrefersSubtitles() const
{
    Page* page = *(pageGroup()->pages().begin());
    if (!page)
        return false;

    return page->settings()->shouldDisplaySubtitles();
}

void CaptionUserPreferences::setUserPrefersSubtitles(bool preference)
{
    Page* page = *(pageGroup()->pages().begin());
    if (!page)
        return;

    page->settings()->setShouldDisplaySubtitles(preference);
    notify();
}

bool CaptionUserPreferences::userPrefersTextDescriptions() const
{
    Page* page = *(pageGroup()->pages().begin());
    if (!page)
        return false;
    
    return page->settings()->shouldDisplayTextDescriptions();
}

void CaptionUserPreferences::setUserPrefersTextDescriptions(bool preference)
{
    Page* page = *(pageGroup()->pages().begin());
    if (!page)
        return;
    
    page->settings()->setShouldDisplayTextDescriptions(preference);
    notify();
}

void CaptionUserPreferences::captionPreferencesChanged()
{
    m_pageGroup->captionPreferencesChanged();
}

Vector<String> CaptionUserPreferences::preferredLanguages() const
{
    Vector<String> languages = userPreferredLanguages();
    if (m_testingMode && !m_userPreferredLanguage.isEmpty())
        languages.insert(0, m_userPreferredLanguage);

    return languages;
}

void CaptionUserPreferences::setPreferredLanguage(String language)
{
    m_userPreferredLanguage = language;
    notify();
}

static String trackDisplayName(TextTrack* track)
{
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
    
static bool textTrackCompare(const RefPtr<TextTrack>& a, const RefPtr<TextTrack>& b)
{
    return codePointCompare(trackDisplayName(a.get()), trackDisplayName(b.get())) < 0;
}

Vector<RefPtr<TextTrack> > CaptionUserPreferences::sortedTrackListForMenu(TextTrackList* trackList)
{
    ASSERT(trackList);

    Vector<RefPtr<TextTrack> > tracksForMenu;

    for (unsigned i = 0, length = trackList->length(); i < length; ++i)
        tracksForMenu.append(trackList->item(i));

    nonCopyingSort(tracksForMenu.begin(), tracksForMenu.end(), textTrackCompare);

    return tracksForMenu;
}

}

#endif // ENABLE(VIDEO_TRACK)
