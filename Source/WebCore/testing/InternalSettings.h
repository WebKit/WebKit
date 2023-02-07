/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include "FontGenericFamilies.h"
#include "InternalSettingsGenerated.h"
#include "Settings.h"

namespace WebCore {

class Page;

class InternalSettings : public InternalSettingsGenerated {
public:
    static Ref<InternalSettings> create(Page*);
    static InternalSettings* from(Page*);
    void hostDestroyed();
    void resetToConsistentState();

    // Settings
    ExceptionOr<void> setStandardFontFamily(const String& family, const String& script);
    ExceptionOr<void> setSerifFontFamily(const String& family, const String& script);
    ExceptionOr<void> setSansSerifFontFamily(const String& family, const String& script);
    ExceptionOr<void> setFixedFontFamily(const String& family, const String& script);
    ExceptionOr<void> setCursiveFontFamily(const String& family, const String& script);
    ExceptionOr<void> setFantasyFontFamily(const String& family, const String& script);
    ExceptionOr<void> setPictographFontFamily(const String& family, const String& script);

    ExceptionOr<void> setTextAutosizingWindowSizeOverride(int width, int height);

    ExceptionOr<void> setMinimumTimerInterval(double intervalInSeconds);
    ExceptionOr<void> setTimeWithoutMouseMovementBeforeHidingControls(double intervalInSeconds);

    using EditingBehaviorType = WebCore::EditingBehaviorType;
    ExceptionOr<void> setEditingBehavior(EditingBehaviorType);
    
    using StorageBlockingPolicy = WebCore::StorageBlockingPolicy;
    ExceptionOr<void> setStorageBlockingPolicy(StorageBlockingPolicy);
    
    using UserInterfaceDirectionPolicy = WebCore::UserInterfaceDirectionPolicy;
    ExceptionOr<void> setUserInterfaceDirectionPolicy(UserInterfaceDirectionPolicy);

    using SystemLayoutDirection = TextDirection;
    ExceptionOr<void> setSystemLayoutDirection(SystemLayoutDirection);

    using FontLoadTimingOverride = WebCore::FontLoadTimingOverride;
    ExceptionOr<void> setFontLoadTimingOverride(FontLoadTimingOverride);

    using ForcedAccessibilityValue = WebCore::ForcedAccessibilityValue;
    ForcedAccessibilityValue forcedColorsAreInvertedAccessibilityValue() const;
    void setForcedColorsAreInvertedAccessibilityValue(ForcedAccessibilityValue);
    ForcedAccessibilityValue forcedDisplayIsMonochromeAccessibilityValue() const;
    void setForcedDisplayIsMonochromeAccessibilityValue(ForcedAccessibilityValue);
    ForcedAccessibilityValue forcedPrefersContrastAccessibilityValue() const;
    void setForcedPrefersContrastAccessibilityValue(ForcedAccessibilityValue);
    ForcedAccessibilityValue forcedPrefersReducedMotionAccessibilityValue() const;
    void setForcedPrefersReducedMotionAccessibilityValue(ForcedAccessibilityValue);
    ForcedAccessibilityValue forcedSupportsHighDynamicRangeValue() const;
    void setForcedSupportsHighDynamicRangeValue(ForcedAccessibilityValue);

    // DeprecatedGlobalSettings.
    ExceptionOr<void> setFetchAPIKeepAliveEnabled(bool);
    ExceptionOr<void> setCustomPasteboardDataEnabled(bool);

    bool vp9DecoderEnabled() const;
    bool mediaSourceInlinePaintingEnabled() const;

    ExceptionOr<void> setShouldManageAudioSessionCategory(bool);

    // CaptionUserPreferences.
    enum class TrackKind : uint8_t { Subtitles, Captions, TextDescriptions };
    ExceptionOr<void> setShouldDisplayTrackKind(TrackKind, bool enabled);
    ExceptionOr<bool> shouldDisplayTrackKind(TrackKind);

    // Page
    ExceptionOr<void> setEditableRegionEnabled(bool);
    ExceptionOr<void> setCanStartMedia(bool);
    ExceptionOr<void> setUseDarkAppearance(bool);
    ExceptionOr<void> setUseElevatedUserInterfaceLevel(bool);

    // ScrollView
    ExceptionOr<void> setAllowUnclampedScrollPosition(bool);

    // PlatformMediaSessionManager.
    ExceptionOr<void> setShouldDeactivateAudioSession(bool);

    // RenderTheme/FontCache
    ExceptionOr<void> setShouldMockBoldSystemFontForAccessibility(bool);

    // AudioContext
    ExceptionOr<void> setDefaultAudioContextSampleRate(float);

    ExceptionOr<void> setAllowedMediaContainerTypes(const String&);
    ExceptionOr<void> setAllowedMediaCodecTypes(const String&);
    ExceptionOr<void> setAllowedMediaVideoCodecIDs(const String&);
    ExceptionOr<void> setAllowedMediaAudioCodecIDs(const String&);
    ExceptionOr<void> setAllowedMediaCaptionFormatTypes(const String&);

private:
    explicit InternalSettings(Page*);

    Settings& settings() const;
    static const char* supplementName();

    class Backup {
    public:
        explicit Backup(Settings&);
        void restoreTo(Settings&);

        // Settings
        // ScriptFontFamilyMaps are initially empty, only used if changed by a test.
        ScriptFontFamilyMap m_standardFontFamilies;
        ScriptFontFamilyMap m_fixedFontFamilies;
        ScriptFontFamilyMap m_serifFontFamilies;
        ScriptFontFamilyMap m_sansSerifFontFamilies;
        ScriptFontFamilyMap m_cursiveFontFamilies;
        ScriptFontFamilyMap m_fantasyFontFamilies;
        ScriptFontFamilyMap m_pictographFontFamilies;
        Seconds m_minimumDOMTimerInterval;
        Seconds m_originalTimeWithoutMouseMovementBeforeHidingControls;
        WebCore::EditingBehaviorType m_originalEditingBehavior;
        WebCore::StorageBlockingPolicy m_storageBlockingPolicy;
        WebCore::UserInterfaceDirectionPolicy m_userInterfaceDirectionPolicy;
        TextDirection m_systemLayoutDirection;
        WebCore::ForcedAccessibilityValue m_forcedColorsAreInvertedAccessibilityValue;
        WebCore::ForcedAccessibilityValue m_forcedDisplayIsMonochromeAccessibilityValue;
        WebCore::ForcedAccessibilityValue m_forcedPrefersContrastAccessibilityValue;
        WebCore::ForcedAccessibilityValue m_forcedPrefersReducedMotionAccessibilityValue;
        WebCore::FontLoadTimingOverride m_fontLoadTimingOverride;

        // DeprecatedGlobalSettings
        bool m_fetchAPIKeepAliveAPIEnabled;
        bool m_customPasteboardDataEnabled;
        bool m_originalMockScrollbarsEnabled;
#if USE(AUDIO_SESSION)
        bool m_shouldManageAudioSessionCategory;
#endif

        // PlatformMediaSessionManager
        bool m_shouldDeactivateAudioSession;
    };

    Page* m_page;
    Backup m_backup;
};

} // namespace WebCore
