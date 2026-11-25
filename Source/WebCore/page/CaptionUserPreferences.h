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

#if ENABLE(VIDEO)

#include <WebCore/AudioTrack.h>
#include <WebCore/TextTrack.h>
#include <WebCore/Timer.h>
#include <wtf/EnumTraits.h>
#include <wtf/HashSet.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CaptionUserPreferencesTestingModeToken;
class HTMLMediaElement;
class Page;
class PageGroup;
class AudioTrackList;
class TextTrackList;
struct MediaSelectionOption;

enum class CaptionUserPreferencesDisplayMode : uint8_t {
    Automatic,
    ForcedOnly,
    AlwaysOn,
    Manual,
};

class CaptionUserPreferences : public RefCountedAndCanMakeWeakPtr<CaptionUserPreferences> {
    WTF_MAKE_TZONE_ALLOCATED(CaptionUserPreferences);
public:
    NODELETE static Ref<CaptionUserPreferences> create(PageGroup&);
    virtual ~CaptionUserPreferences();

    using CaptionDisplayMode = CaptionUserPreferencesDisplayMode;
    virtual CaptionDisplayMode captionDisplayMode() const;
    virtual void setCaptionDisplayMode(CaptionDisplayMode);

    virtual int textTrackSelectionScore(TextTrack&, CaptionDisplayMode, AudioTrack* enabledAudioTrack = nullptr) const;
    virtual int textTrackSelectionScore(TextTrack&, HTMLMediaElement&) const;
    virtual int textTrackLanguageSelectionScore(TextTrack&, const Vector<RefPtr<StringImpl>>&) const;

    virtual bool userPrefersCaptions() const;
    virtual void setUserPrefersCaptions(bool);

    virtual bool userPrefersSubtitles() const;
    virtual void setUserPrefersSubtitles(bool preference);
    
    virtual bool userPrefersTextDescriptions() const;
    virtual void setUserPrefersTextDescriptions(bool preference);

    virtual float captionFontSizeScaleAndImportance(bool& important) const { important = false; return 0.05f; }

    virtual bool captionStrokeWidthForFont(float, StringImpl*, float&, bool&) const { return false; }

    virtual RefPtr<StringImpl> captionsStyleSheetOverride() const { return m_captionsStyleSheetOverride; }
    virtual void setCaptionsStyleSheetOverride(StringImpl*);

    virtual void setInterestedInCaptionPreferenceChanges() { }

    virtual void captionPreferencesChanged();

    virtual void setPreferredLanguage(StringImpl*);
    virtual Vector<RefPtr<StringImpl>> preferredLanguages() const;

    virtual void setPreferredAudioCharacteristic(StringImpl*);
    virtual Vector<RefPtr<StringImpl>> preferredAudioCharacteristics() const;

    virtual RefPtr<StringImpl> displayNameForTrack(const TextTrack&) const;
    MediaSelectionOption mediaSelectionOptionForTrack(const TextTrack&) const;
    virtual Vector<Ref<TextTrack>> sortedTrackListForMenu(TextTrackList*, HashSet<TextTrack::Kind>);
    RefPtr<TextTrack> bestTextTrackToEnable(const TextTrackList&) const;

    virtual RefPtr<StringImpl> displayNameForTrack(const AudioTrack&) const;
    MediaSelectionOption mediaSelectionOptionForTrack(const AudioTrack&) const;
    virtual Vector<Ref<AudioTrack>> sortedTrackListForMenu(AudioTrackList*);

    void setPrimaryAudioTrackLanguageOverride(StringImpl* language) { m_primaryAudioTrackLanguageOverride = language;  }
    RefPtr<StringImpl> primaryAudioTrackLanguageOverride() const;

    void setPreferredAudioCharacteristicsForTesting(const Vector<RefPtr<StringImpl>>& characteristics) { m_preferredAudioCharacteristicsForTesting = characteristics; }
    const Vector<RefPtr<StringImpl>>& preferredAudioCharacteristicsForTesting() const LIFETIME_BOUND { return m_preferredAudioCharacteristicsForTesting; }

    virtual bool testingMode() const { return m_testingModeCount; }

    friend class CaptionUserPreferencesTestingModeToken;
    NODELETE WEBCORE_EXPORT UniqueRef<CaptionUserPreferencesTestingModeToken> createTestingModeToken();

    virtual RefPtr<StringImpl> captionPreviewTitle() const;
    virtual RefPtr<StringImpl> captionPreviewProfileID() const { return emptyString(); }
    virtual void setCaptionPreviewProfileID(StringImpl*) { }

    NODELETE PageGroup& pageGroup() const;

protected:
    explicit CaptionUserPreferences(PageGroup&);

    void updateCaptionStyleSheetOverride();
    NODELETE void beginBlockingNotifications();
    NODELETE void endBlockingNotifications();

private:
    void incrementTestingModeCount() { ++m_testingModeCount; }
    void decrementTestingModeCount()
    {
        ASSERT(m_testingModeCount);
        if (m_testingModeCount)
            --m_testingModeCount;
    }

    void timerFired();
    void notify();
    NODELETE RefPtr<Page> currentPage() const;

    WeakRef<PageGroup> m_pageGroup;
    mutable CaptionDisplayMode m_displayMode;
    Timer m_timer;
    RefPtr<StringImpl> m_userPreferredLanguage;
    RefPtr<StringImpl> m_userPreferredAudioCharacteristic;
    RefPtr<StringImpl> m_captionsStyleSheetOverride;
    RefPtr<StringImpl> m_primaryAudioTrackLanguageOverride;
    Vector<RefPtr<StringImpl>> m_preferredAudioCharacteristicsForTesting;
    unsigned m_blockNotificationsCounter { 0 };
    bool m_havePreferences { false };
    unsigned m_testingModeCount { 0 };
};

class CaptionUserPreferencesTestingModeToken {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(CaptionUserPreferencesTestingModeToken, WEBCORE_EXPORT);
public:
    explicit CaptionUserPreferencesTestingModeToken(CaptionUserPreferences& parent)
        : m_parent(parent)
    {
        parent.incrementTestingModeCount();
    }
    ~CaptionUserPreferencesTestingModeToken()
    {
        if (m_parent)
            m_parent->decrementTestingModeCount();
    }
private:
    WeakPtr<CaptionUserPreferences> m_parent;
};
    
}

namespace WTF {

template<> struct EnumTraits<WebCore::CaptionUserPreferences::CaptionDisplayMode> {
    static std::optional<WebCore::CaptionUserPreferences::CaptionDisplayMode> fromString(StringImpl* mode)
    {
        if (equalLettersIgnoringASCIICase(mode, "forcedonly"_s))
            return WebCore::CaptionUserPreferences::CaptionDisplayMode::ForcedOnly;
        if (equalLettersIgnoringASCIICase(mode, "manual"_s))
            return WebCore::CaptionUserPreferences::CaptionDisplayMode::Manual;
        if (equalLettersIgnoringASCIICase(mode, "automatic"_s))
            return WebCore::CaptionUserPreferences::CaptionDisplayMode::Automatic;
        if (equalLettersIgnoringASCIICase(mode, "alwayson"_s))
            return WebCore::CaptionUserPreferences::CaptionDisplayMode::AlwaysOn;
        return std::nullopt;
    }
};

} // namespace WTF

#endif
