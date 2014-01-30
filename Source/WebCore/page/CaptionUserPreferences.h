/*
 * Copyright (C) 2012, 2013  Apple Inc. All rights reserved.
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

#ifndef CaptionUserPreferences_h
#define CaptionUserPreferences_h

#if ENABLE(VIDEO_TRACK)

#include "Language.h"
#include "LocalizedStrings.h"
#include "TextTrack.h"
#include "Timer.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class HTMLMediaElement;
class PageGroup;
class TextTrackList;

class CaptionUserPreferences {
public:
    CaptionUserPreferences();
    virtual ~CaptionUserPreferences();

    String captionsStyleSheet();

    enum CaptionDisplayMode {
        Automatic,
        ForcedOnly,
        AlwaysOn
    };

    virtual CaptionDisplayMode captionDisplayMode() const;
    virtual void setCaptionDisplayMode(CaptionDisplayMode);

    virtual int textTrackSelectionScore(TextTrack*, HTMLMediaElement*) const;
    virtual int textTrackLanguageSelectionScore(TextTrack*, const Vector<String>&) const;
    virtual bool userPrefersCaptions(Document&) const;
    virtual bool userPrefersSubtitles(Document&) const;
    virtual bool userPrefersTextDescriptions(Document&) const;

    virtual float captionFontSizeScaleAndImportance(bool& important) const { important = false; return 0.05f; }

    virtual void setPreferredLanguage(const String&);
    virtual Vector<String> preferredLanguages() const;

    virtual String displayNameForTrack(TextTrack*) const;
    virtual Vector<RefPtr<TextTrack>> sortedTrackListForMenu(TextTrackList*);

    void captionPreferencesChanged();

    // These are used for testing mode only.
    void setCaptionsStyleSheetOverride(const String&);
    String captionsStyleSheetOverride() const;
    void setPrimaryAudioTrackLanguageOverride(const String& language) { m_primaryAudioTrackLanguageOverride = language;  }
    String primaryAudioTrackLanguageOverride() const;
    void setTestingMode(bool override) { m_testingMode = override; }
    bool testingMode() const { return m_testingMode; }
    
private:
    void timerFired(Timer<CaptionUserPreferences>&);
    void notify();

    virtual String platformCaptionsStyleSheet() { return String(); }

    Timer<CaptionUserPreferences> m_timer;

    // These are used for testing mode only.
    CaptionDisplayMode m_displayMode;
    String m_userPreferredLanguage;
    String m_captionsStyleSheetOverride;
    String m_primaryAudioTrackLanguageOverride;
    bool m_testingMode;
};

}
#endif

#endif
