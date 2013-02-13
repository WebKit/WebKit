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

namespace WebCore {

void CaptionUserPreferences::registerForPreferencesChangedCallbacks(CaptionPreferencesChangedListener* listener)
{
    m_captionPreferenceChangeListeners.add(listener);
}

void CaptionUserPreferences::unregisterForPreferencesChangedCallbacks(CaptionPreferencesChangedListener* listener)
{
    if (m_captionPreferenceChangeListeners.isEmpty())
        return;

    m_captionPreferenceChangeListeners.remove(listener);
}

void CaptionUserPreferences::setUserPrefersCaptions(bool preference)
{
    m_userPrefersCaptions = preference;
    if (m_testingMode) {
        m_havePreferences = true;
        captionPreferencesChanged();
    }
}

void CaptionUserPreferences::captionPreferencesChanged()
{
    if (m_captionPreferenceChangeListeners.isEmpty())
        return;
    
    for (HashSet<CaptionPreferencesChangedListener*>::iterator i = m_captionPreferenceChangeListeners.begin(); i != m_captionPreferenceChangeListeners.end(); ++i)
        (*i)->captionPreferencesChanged();
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
    if (m_testingMode) {
        m_havePreferences = true;
        captionPreferencesChanged();
    }
}

String CaptionUserPreferences::displayNameForTrack(TextTrack* track) const
{
    if (track->label().isEmpty() && track->language().isEmpty())
        return textTrackNoLabelText();
    if (!track->label().isEmpty())
        return track->label();
    return track->language();
}

}

#endif // ENABLE(VIDEO_TRACK)
