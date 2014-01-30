/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef CaptionUserPreferencesMediaAF_h
#define CaptionUserPreferencesMediaAF_h

#if ENABLE(VIDEO_TRACK)

#include "CSSPropertyNames.h"
#include "CaptionUserPreferences.h"
#include "Color.h"
#include <wtf/HashSet.h>

namespace WebCore {

class CaptionUserPreferencesMediaAF : public CaptionUserPreferences {
public:
    CaptionUserPreferencesMediaAF(PageGroup&);
    virtual ~CaptionUserPreferencesMediaAF();

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    virtual CaptionDisplayMode captionDisplayMode() const override;
    virtual void setCaptionDisplayMode(CaptionDisplayMode) override;

    virtual bool userPrefersCaptions() const override;
    virtual bool userPrefersSubtitles() const override;

    virtual float captionFontSizeScaleAndImportance(bool&) const override;

    virtual void setInterestedInCaptionPreferenceChanges() override;

    virtual void setPreferredLanguage(const String&) override;
    virtual Vector<String> preferredLanguages() const override;

    virtual void captionPreferencesChanged() override;

    bool shouldFilterTrackMenu() const { return true; }
#else
    bool shouldFilterTrackMenu() const { return false; }
#endif

    virtual String captionsStyleSheetOverride() const override;
    virtual int textTrackSelectionScore(TextTrack*, HTMLMediaElement*) const override;
    virtual Vector<RefPtr<TextTrack>> sortedTrackListForMenu(TextTrackList*) override;
    virtual String displayNameForTrack(TextTrack*) const override;

private:
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    String captionsWindowCSS() const;
    String captionsBackgroundCSS() const;
    String captionsTextColorCSS() const;
    Color captionsTextColor(bool&) const;
    String captionsDefaultFontCSS() const;
    Color captionsEdgeColorForTextColor(const Color&) const;
    String windowRoundedCornerRadiusCSS() const;
    String captionsTextEdgeCSS() const;
    String cssPropertyWithTextEdgeColor(CSSPropertyID, const String&, const Color&, bool) const;
    String colorPropertyCSS(CSSPropertyID, const Color&, bool) const;

    bool m_listeningForPreferenceChanges;
#endif
};

}
#endif

#endif
