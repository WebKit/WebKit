/*
 * Copyright (C) 2003-2016 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#include "ClipboardAccessPolicy.h"
#include "ContentType.h"
#include "EditingBehaviorTypes.h"
#include "IntSize.h"
#include "SecurityOrigin.h"
#include "StorageMap.h"
#include "TextFlags.h"
#include "Timer.h"
#include <wtf/URL.h>
#include "WritingMode.h"
#include <JavaScriptCore/RuntimeFlags.h>
#include <unicode/uscript.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

#if ENABLE(DATA_DETECTION)
#include "DataDetection.h"
#endif

namespace WebCore {

class FontGenericFamilies;
class Page;

enum EditableLinkBehavior {
    EditableLinkDefaultBehavior,
    EditableLinkAlwaysLive,
    EditableLinkOnlyLiveWithShiftKey,
    EditableLinkLiveWhenNotFocused,
    EditableLinkNeverLive
};

enum TextDirectionSubmenuInclusionBehavior {
    TextDirectionSubmenuNeverIncluded,
    TextDirectionSubmenuAutomaticallyIncluded,
    TextDirectionSubmenuAlwaysIncluded
};

enum DebugOverlayRegionFlags {
    NonFastScrollableRegion = 1 << 0,
    WheelEventHandlerRegion = 1 << 1,
};

enum class UserInterfaceDirectionPolicy {
    Content,
    System
};

enum PDFImageCachingPolicy {
    PDFImageCachingEnabled,
    PDFImageCachingBelowMemoryLimit,
    PDFImageCachingDisabled,
    PDFImageCachingClipBoundsOnly,
#if PLATFORM(IOS_FAMILY)
    PDFImageCachingDefault = PDFImageCachingBelowMemoryLimit
#else
    PDFImageCachingDefault = PDFImageCachingEnabled
#endif
};

enum class FrameFlattening {
    Disabled,
    EnabledForNonFullScreenIFrames,
    FullyEnabled
};

typedef unsigned DebugOverlayRegions;

class SettingsBase {
    WTF_MAKE_NONCOPYABLE(SettingsBase); WTF_MAKE_FAST_ALLOCATED;
public:
    ~SettingsBase();

    void pageDestroyed() { m_page = nullptr; }

    enum class FontLoadTimingOverride { None, Block, Swap, Failure };

    // FIXME: Move these default values to SettingsDefaultValues.h

    enum class ForcedAccessibilityValue { System, On, Off };
    static const SettingsBase::ForcedAccessibilityValue defaultForcedColorsAreInvertedAccessibilityValue = ForcedAccessibilityValue::System;
    static const SettingsBase::ForcedAccessibilityValue defaultForcedDisplayIsMonochromeAccessibilityValue = ForcedAccessibilityValue::System;
    static const SettingsBase::ForcedAccessibilityValue defaultForcedPrefersReducedMotionAccessibilityValue = ForcedAccessibilityValue::System;

    WEBCORE_EXPORT static bool defaultTextAutosizingEnabled();
    WEBCORE_EXPORT static float defaultMinimumZoomFontSize();
    WEBCORE_EXPORT static bool defaultDownloadableBinaryFontsEnabled();

#if ENABLE(MEDIA_SOURCE)
    static bool platformDefaultMediaSourceEnabled();
#endif

    static const unsigned defaultMaximumHTMLParserDOMTreeDepth = 512;
    static const unsigned defaultMaximumRenderTreeDepth = 512;

#if ENABLE(TEXT_AUTOSIZING)
    constexpr static const float boostedOneLineTextMultiplierCoefficient = 2.23125f;
    constexpr static const float boostedMultiLineTextMultiplierCoefficient = 2.48125f;
    constexpr static const float boostedMaxTextAutosizingScaleIncrease = 5.0f;
    constexpr static const float defaultOneLineTextMultiplierCoefficient = 1.7f;
    constexpr static const float defaultMultiLineTextMultiplierCoefficient = 1.95f;
    constexpr static const float defaultMaxTextAutosizingScaleIncrease = 1.7f;
#endif

    WEBCORE_EXPORT void setStandardFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const AtomicString& standardFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setFixedFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const AtomicString& fixedFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setSerifFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const AtomicString& serifFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setSansSerifFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const AtomicString& sansSerifFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setCursiveFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const AtomicString& cursiveFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setFantasyFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const AtomicString& fantasyFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setPictographFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const AtomicString& pictographFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setMinimumDOMTimerInterval(Seconds); // Initialized to DOMTimer::defaultMinimumInterval().
    Seconds minimumDOMTimerInterval() const { return m_minimumDOMTimerInterval; }

    WEBCORE_EXPORT void setLayoutInterval(Seconds);
    Seconds layoutInterval() const { return m_layoutInterval; }

#if ENABLE(TEXT_AUTOSIZING)
    float oneLineTextMultiplierCoefficient() const { return m_oneLineTextMultiplierCoefficient; }
    float multiLineTextMultiplierCoefficient() const { return m_multiLineTextMultiplierCoefficient; }
    float maxTextAutosizingScaleIncrease() const { return m_maxTextAutosizingScaleIncrease; }
#endif

    WEBCORE_EXPORT static const String& defaultMediaContentTypesRequiringHardwareSupport();
    WEBCORE_EXPORT void setMediaContentTypesRequiringHardwareSupport(const Vector<ContentType>&);
    WEBCORE_EXPORT void setMediaContentTypesRequiringHardwareSupport(const String&);
    const Vector<ContentType>& mediaContentTypesRequiringHardwareSupport() const { return m_mediaContentTypesRequiringHardwareSupport; }

protected:
    explicit SettingsBase(Page*);

    void initializeDefaultFontFamilies();

    void imageLoadingSettingsTimerFired();

    // Helpers used by generated Settings.cpp.
    void setNeedsRecalcStyleInAllFrames();
    void setNeedsRelayoutAllFrames();
    void mediaTypeOverrideChanged();
    void imagesEnabledChanged();
    void pluginsEnabledChanged();
    void userStyleSheetLocationChanged();
    void usesPageCacheChanged();
    void dnsPrefetchingEnabledChanged();
    void storageBlockingPolicyChanged();
    void backgroundShouldExtendBeyondPageChanged();
    void scrollingPerformanceLoggingEnabledChanged();
    void hiddenPageDOMTimerThrottlingStateChanged();
    void hiddenPageCSSAnimationSuspensionEnabledChanged();
    void resourceUsageOverlayVisibleChanged();
#if ENABLE(TEXT_AUTOSIZING)
    void shouldEnableTextAutosizingBoostChanged();
#endif

    Page* m_page;

    const std::unique_ptr<FontGenericFamilies> m_fontGenericFamilies;
    Seconds m_layoutInterval;
    Seconds m_minimumDOMTimerInterval;

    Timer m_setImageLoadingSettingsTimer;

    Vector<ContentType> m_mediaContentTypesRequiringHardwareSupport;

#if ENABLE(TEXT_AUTOSIZING)
    float m_oneLineTextMultiplierCoefficient { defaultOneLineTextMultiplierCoefficient };
    float m_multiLineTextMultiplierCoefficient { defaultMultiLineTextMultiplierCoefficient };
    float m_maxTextAutosizingScaleIncrease { defaultMaxTextAutosizingScaleIncrease };
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/SettingsAdditions.h>
#endif
};

} // namespace WebCore
