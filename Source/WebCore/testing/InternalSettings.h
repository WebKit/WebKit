/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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

// FIXME (121927): This include should not be needed.
#include <wtf/text/AtomStringHash.h>

#include "EditingBehaviorTypes.h"
#include "ExceptionOr.h"
#include "FontGenericFamilies.h"
#include "IntSize.h"
#include "InternalSettingsGenerated.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WritingMode.h"

namespace WebCore {

class Page;
class Settings;

class InternalSettings : public InternalSettingsGenerated {
public:
    static Ref<InternalSettings> create(Page*);
    static InternalSettings* from(Page*);
    void hostDestroyed();
    void resetToConsistentState();

    ExceptionOr<void> setUsesOverlayScrollbars(bool);
    ExceptionOr<void> setTouchEventEmulationEnabled(bool);
    ExceptionOr<void> setStandardFontFamily(const String& family, const String& script);
    ExceptionOr<void> setSerifFontFamily(const String& family, const String& script);
    ExceptionOr<void> setSansSerifFontFamily(const String& family, const String& script);
    ExceptionOr<void> setFixedFontFamily(const String& family, const String& script);
    ExceptionOr<void> setCursiveFontFamily(const String& family, const String& script);
    ExceptionOr<void> setFantasyFontFamily(const String& family, const String& script);
    ExceptionOr<void> setPictographFontFamily(const String& family, const String& script);
    ExceptionOr<void> setTextAutosizingEnabled(bool);
    ExceptionOr<void> setTextAutosizingWindowSizeOverride(int width, int height);
    ExceptionOr<void> setTextAutosizingUsesIdempotentMode(bool);
    ExceptionOr<void> setTextAutosizingFontScaleFactor(float);
    ExceptionOr<void> setMediaTypeOverride(const String&);
    ExceptionOr<void> setCanStartMedia(bool);
    ExceptionOr<void> setAllowsAirPlayForMediaPlayback(bool);
    ExceptionOr<void> setMediaCaptureRequiresSecureConnection(bool);
    void setDefaultAudioContextSampleRate(float);

    ExceptionOr<void> setEditingBehavior(const String&);
    ExceptionOr<void> setPreferMIMETypeForImages(bool);
    ExceptionOr<void> setPDFImageCachingPolicy(const String&);
    ExceptionOr<void> setShouldDisplayTrackKind(const String& kind, bool enabled);
    ExceptionOr<bool> shouldDisplayTrackKind(const String& kind);
    ExceptionOr<void> setUseDarkAppearance(bool);
    ExceptionOr<void> setStorageBlockingPolicy(const String&);
    ExceptionOr<void> setImagesEnabled(bool);
    ExceptionOr<void> setMinimumTimerInterval(double intervalInSeconds);
    ExceptionOr<void> setDefaultVideoPosterURL(const String&);
    ExceptionOr<void> setForcePendingWebGLPolicy(bool);
    ExceptionOr<void> setTimeWithoutMouseMovementBeforeHidingControls(double);
    ExceptionOr<void> setUseLegacyBackgroundSizeShorthandBehavior(bool);
    ExceptionOr<void> setAutoscrollForDragAndDropEnabled(bool);
    ExceptionOr<void> setFontFallbackPrefersPictographs(bool);
    enum class FontLoadTimingOverride { Block, Swap, Failure };
    ExceptionOr<void> setFontLoadTimingOverride(const FontLoadTimingOverride&);
    ExceptionOr<void> setShouldIgnoreFontLoadCompletions(bool);
    ExceptionOr<void> setQuickTimePluginReplacementEnabled(bool);
    ExceptionOr<void> setYouTubeFlashPluginReplacementEnabled(bool);
    ExceptionOr<void> setBackgroundShouldExtendBeyondPage(bool);
    ExceptionOr<void> setShouldConvertPositionStyleOnCopy(bool);
    ExceptionOr<void> setScrollingTreeIncludesFrames(bool);
    ExceptionOr<void> setAllowUnclampedScrollPosition(bool);
    ExceptionOr<void> setAllowsInlineMediaPlayback(bool);
    ExceptionOr<void> setAllowsInlineMediaPlaybackAfterFullscreen(bool);
    ExceptionOr<void> setInlineMediaPlaybackRequiresPlaysInlineAttribute(bool);
    ExceptionOr<String> userInterfaceDirectionPolicy();
    ExceptionOr<void> setUserInterfaceDirectionPolicy(const String&);
    ExceptionOr<String> systemLayoutDirection();
    ExceptionOr<void> setSystemLayoutDirection(const String&);
    ExceptionOr<void> setShouldMockBoldSystemFontForAccessibility(bool);
    ExceptionOr<void> setShouldManageAudioSessionCategory(bool);
    ExceptionOr<void> setCustomPasteboardDataEnabled(bool);
    ExceptionOr<void> setIncompleteImageBorderEnabled(bool);
    ExceptionOr<void> setShouldDispatchSyntheticMouseEventsWhenModifyingSelection(bool);
    ExceptionOr<void> setShouldDispatchSyntheticMouseOutAfterSyntheticClick(bool);
    ExceptionOr<void> setAnimatedImageDebugCanvasDrawingEnabled(bool);

    using FrameFlatteningValue = FrameFlattening;
    ExceptionOr<void> setFrameFlattening(FrameFlatteningValue);

    ExceptionOr<void> setEditableRegionEnabled(bool);
    
    static void setAllowsAnySSLCertificate(bool);

    ExceptionOr<bool> deferredCSSParserEnabled();
    ExceptionOr<void> setDeferredCSSParserEnabled(bool);

    enum class ForcedAccessibilityValue { System, On, Off };
    ForcedAccessibilityValue forcedColorsAreInvertedAccessibilityValue() const;
    void setForcedColorsAreInvertedAccessibilityValue(ForcedAccessibilityValue);
    ForcedAccessibilityValue forcedDisplayIsMonochromeAccessibilityValue() const;
    void setForcedDisplayIsMonochromeAccessibilityValue(ForcedAccessibilityValue);
    ForcedAccessibilityValue forcedPrefersReducedMotionAccessibilityValue() const;
    void setForcedPrefersReducedMotionAccessibilityValue(ForcedAccessibilityValue);
    ForcedAccessibilityValue forcedSupportsHighDynamicRangeValue() const;
    void setForcedSupportsHighDynamicRangeValue(ForcedAccessibilityValue);

    // RuntimeEnabledFeatures.
    static void setWebGL2Enabled(bool);
    static void setWebGPUEnabled(bool);
    static void setPictureInPictureAPIEnabled(bool);
    static void setFetchAPIKeepAliveEnabled(bool);

    void setShouldDeactivateAudioSession(bool);
    
    void setStorageAccessAPIPerPageScopeEnabled(bool);

private:
    explicit InternalSettings(Page*);

    Settings& settings() const;
    static const char* supplementName();

    void setUseDarkAppearanceInternal(bool);

    class Backup {
    public:
        explicit Backup(Settings&);
        void restoreTo(Settings&);

        EditingBehaviorType m_originalEditingBehavior;

        // Initially empty, only used if changed by a test.
        ScriptFontFamilyMap m_standardFontFamilies;
        ScriptFontFamilyMap m_fixedFontFamilies;
        ScriptFontFamilyMap m_serifFontFamilies;
        ScriptFontFamilyMap m_sansSerifFontFamilies;
        ScriptFontFamilyMap m_cursiveFontFamilies;
        ScriptFontFamilyMap m_fantasyFontFamilies;
        ScriptFontFamilyMap m_pictographFontFamilies;

#if ENABLE(TEXT_AUTOSIZING)
        bool m_originalTextAutosizingEnabled;
        IntSize m_originalTextAutosizingWindowSizeOverride;
        bool m_originalTextAutosizingUsesIdempotentMode;
#endif

        String m_originalMediaTypeOverride;
        bool m_originalCanvasUsesAcceleratedDrawing;
        bool m_originalMockScrollbarsEnabled;
        bool m_originalUsesOverlayScrollbars;
        bool m_imagesEnabled;
        bool m_preferMIMETypeForImages;
        Seconds m_minimumDOMTimerInterval;
#if ENABLE(VIDEO)
        bool m_shouldDisplaySubtitles;
        bool m_shouldDisplayCaptions;
        bool m_shouldDisplayTextDescriptions;
#endif
        String m_defaultVideoPosterURL;
        bool m_forcePendingWebGLPolicy;
        Seconds m_originalTimeWithoutMouseMovementBeforeHidingControls;
        bool m_useLegacyBackgroundSizeShorthandBehavior;
        bool m_autoscrollForDragAndDropEnabled;
        bool m_quickTimePluginReplacementEnabled;
        bool m_youTubeFlashPluginReplacementEnabled;
        bool m_shouldConvertPositionStyleOnCopy;
        bool m_fontFallbackPrefersPictographs;
        bool m_shouldIgnoreFontLoadCompletions;
        bool m_backgroundShouldExtendBeyondPage;
        SecurityOrigin::StorageBlockingPolicy m_storageBlockingPolicy;
        bool m_scrollingTreeIncludesFrames;
#if ENABLE(TOUCH_EVENTS)
        bool m_touchEventEmulationEnabled;
#endif
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        bool m_allowsAirPlayForMediaPlayback;
#endif
        bool m_allowsInlineMediaPlayback;
        bool m_allowsInlineMediaPlaybackAfterFullscreen;
        bool m_inlineMediaPlaybackRequiresPlaysInlineAttribute;
        bool m_deferredCSSParserEnabled;
        bool m_inputEventsEnabled;
        bool m_incompleteImageBorderEnabled;
        bool m_shouldDispatchSyntheticMouseEventsWhenModifyingSelection;
        bool m_shouldDispatchSyntheticMouseOutAfterSyntheticClick { false };
        bool m_shouldDeactivateAudioSession;
        bool m_animatedImageDebugCanvasDrawingEnabled;
        UserInterfaceDirectionPolicy m_userInterfaceDirectionPolicy;
        TextDirection m_systemLayoutDirection;
        PDFImageCachingPolicy m_pdfImageCachingPolicy;
        Settings::ForcedAccessibilityValue m_forcedColorsAreInvertedAccessibilityValue;
        Settings::ForcedAccessibilityValue m_forcedDisplayIsMonochromeAccessibilityValue;
        Settings::ForcedAccessibilityValue m_forcedPrefersReducedMotionAccessibilityValue;
        Settings::FontLoadTimingOverride m_fontLoadTimingOverride;
        FrameFlattening m_frameFlattening;

        // Runtime enabled settings.
        bool m_webGL2Enabled;
        bool m_fetchAPIKeepAliveAPIEnabled;
        
        bool m_shouldMockBoldSystemFontForAccessibility;
#if USE(AUDIO_SESSION)
        bool m_shouldManageAudioSessionCategory;
#endif
        bool m_customPasteboardDataEnabled;
    };

    Page* m_page;
    Backup m_backup;
};

} // namespace WebCore
