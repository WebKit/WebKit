/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Platform.h>
#if ENABLE(VIDEO) && HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

#include <WebCore/CSSPropertyNames.h>
#include <WebCore/CaptionPreferencesDelegate.h>
#include <WebCore/CaptionUserPreferences.h>
#include <WebCore/Color.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS WebCaptionUserPreferencesMediaAFWeakObserver;

namespace WebCore {

class CaptionUserPreferencesMediaAF : public CaptionUserPreferences {
    WTF_MAKE_TZONE_ALLOCATED(CaptionUserPreferencesMediaAF);
public:
    WEBCORE_EXPORT static Ref<CaptionUserPreferencesMediaAF> create(PageGroup&);
    virtual ~CaptionUserPreferencesMediaAF();

    CaptionDisplayMode captionDisplayMode() const override;
    void setCaptionDisplayMode(CaptionDisplayMode) override;

    WEBCORE_EXPORT static CaptionDisplayMode platformCaptionDisplayMode();
    WEBCORE_EXPORT static void platformSetCaptionDisplayMode(CaptionDisplayMode);
    WEBCORE_EXPORT static void NODELETE setCachedCaptionDisplayMode(CaptionDisplayMode);

    WEBCORE_EXPORT static Vector<String> platformProfileIDs();
    WEBCORE_EXPORT static String platformActiveProfileID();
    WEBCORE_EXPORT static bool canSetActiveProfileID();
    WEBCORE_EXPORT static bool setActiveProfileID(const String&);
    WEBCORE_EXPORT static String nameForProfileID(const String&);

    bool userPrefersCaptions() const override;
    bool userPrefersSubtitles() const override;
    bool userPrefersTextDescriptions() const final;

    float captionFontSizeScaleAndImportance(bool&) const override;
    bool captionStrokeWidthForFont(float fontSize, const String& language, float& strokeWidth, bool& important) const override;

    void setInterestedInCaptionPreferenceChanges() override;

    void setPreferredLanguage(const String&) override;
    Vector<String> preferredLanguages() const override;

    WEBCORE_EXPORT static Vector<String> platformPreferredLanguages();
    WEBCORE_EXPORT static void platformSetPreferredLanguage(const String&);
    WEBCORE_EXPORT static void setCachedPreferredLanguages(const Vector<String>&);

    void setPreferredAudioCharacteristic(const String&) override;
    Vector<String> preferredAudioCharacteristics() const override;

    void captionPreferencesChanged() override;

    bool shouldFilterTrackMenu() const { return true; }
    
    WEBCORE_EXPORT static void setCaptionPreferencesDelegate(std::unique_ptr<CaptionPreferencesDelegate>&&);

    bool testingMode() const final;

    static RefPtr<CaptionUserPreferencesMediaAF> extractCaptionUserPreferencesMediaAF(void* observer);

    WEBCORE_EXPORT String captionsStyleSheetOverride() const override;
    Vector<Ref<AudioTrack>> sortedTrackListForMenu(AudioTrackList*) override;
    Vector<Ref<TextTrack>> sortedTrackListForMenu(TextTrackList*, HashSet<TextTrack::Kind>) override;
    String displayNameForTrack(const AudioTrack&) const override;
    String displayNameForTrack(const TextTrack&) const override;
    String captionPreviewProfileID() const override;
    void setCaptionPreviewProfileID(const String&) override;
    String captionPreviewTitle() const override;

    WEBCORE_EXPORT String captionsWindowCSS() const;
    WEBCORE_EXPORT String captionsBackgroundCSS() const;
    WEBCORE_EXPORT String captionsTextColorCSS() const;
    WEBCORE_EXPORT Color captionsTextColor(bool&) const;
    WEBCORE_EXPORT String captionsDefaultFontCSS() const;
    WEBCORE_EXPORT String captionsFontSizeCSS() const;
    WEBCORE_EXPORT String windowRoundedCornerRadiusCSS() const;
    WEBCORE_EXPORT String captionsTextEdgeCSS() const;

private:
    explicit CaptionUserPreferencesMediaAF(PageGroup&);

    void updateTimerFired();
    bool hasNullCaptionProfile() const;

    static RetainPtr<WebCaptionUserPreferencesMediaAFWeakObserver> createWeakObserver(CaptionUserPreferencesMediaAF*);

    const RetainPtr<WebCaptionUserPreferencesMediaAFWeakObserver> m_observer;

    Timer m_updateStyleSheetTimer;
    bool m_listeningForPreferenceChanges { false };
    bool m_registeringForNotification { false };
    String m_previewProfileID;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
