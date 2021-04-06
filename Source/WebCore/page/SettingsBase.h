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
#include "EditableLinkBehavior.h"
#include "EditingBehaviorType.h"
#include "FontGenericFamilies.h"
#include "FontLoadTimingOverride.h"
#include "FontRenderingMode.h"
#include "ForcedAccessibilityValue.h"
#include "FrameFlattening.h"
#include "HTMLParserScriptingFlagPolicy.h"
#include "MediaPlayerEnums.h"
#include "PDFImageCachingPolicy.h"
#include "StorageBlockingPolicy.h"
#include "StorageMap.h"
#include "TextDirection.h"
#include "TextDirectionSubmenuInclusionBehavior.h"
#include "Timer.h"
#include "UserInterfaceDirectionPolicy.h"
#include <JavaScriptCore/RuntimeFlags.h>
#include <unicode/uscript.h>
#include <wtf/RefCounted.h>
#include <wtf/Seconds.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

#if ENABLE(DATA_DETECTION)
#include "DataDetectorType.h"
#endif

namespace WebCore {

class Page;

class SettingsBase {
    WTF_MAKE_NONCOPYABLE(SettingsBase); WTF_MAKE_FAST_ALLOCATED;
public:
    void pageDestroyed() { m_page = nullptr; }

#if ENABLE(MEDIA_SOURCE)
    WEBCORE_EXPORT static bool platformDefaultMediaSourceEnabled();
#endif

    static const unsigned defaultMaximumHTMLParserDOMTreeDepth = 512;
    static const unsigned defaultMaximumRenderTreeDepth = 512;

    virtual FontGenericFamilies& fontGenericFamilies() = 0;
    virtual const FontGenericFamilies& fontGenericFamilies() const = 0;

    WEBCORE_EXPORT void setStandardFontFamily(const String&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const String& standardFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setFixedFontFamily(const String&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const String& fixedFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setSerifFontFamily(const String&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const String& serifFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setSansSerifFontFamily(const String&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const String& sansSerifFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setCursiveFontFamily(const String&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const String& cursiveFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setFantasyFontFamily(const String&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const String& fantasyFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setPictographFontFamily(const String&, UScriptCode = USCRIPT_COMMON);
    WEBCORE_EXPORT const String& pictographFontFamily(UScriptCode = USCRIPT_COMMON) const;

    WEBCORE_EXPORT void setMinimumDOMTimerInterval(Seconds); // Initialized to DOMTimer::defaultMinimumInterval().
    Seconds minimumDOMTimerInterval() const { return m_minimumDOMTimerInterval; }

#if ENABLE(TEXT_AUTOSIZING)
    float oneLineTextMultiplierCoefficient() const { return m_oneLineTextMultiplierCoefficient; }
    float multiLineTextMultiplierCoefficient() const { return m_multiLineTextMultiplierCoefficient; }
    float maxTextAutosizingScaleIncrease() const { return m_maxTextAutosizingScaleIncrease; }
#endif

    WEBCORE_EXPORT void setMediaContentTypesRequiringHardwareSupport(const Vector<ContentType>&);
    WEBCORE_EXPORT void setMediaContentTypesRequiringHardwareSupport(const String&);
    const Vector<ContentType>& mediaContentTypesRequiringHardwareSupport() const { return m_mediaContentTypesRequiringHardwareSupport; }

protected:
    explicit SettingsBase(Page*);
    virtual ~SettingsBase();

    void initializeDefaultFontFamilies();

    void imageLoadingSettingsTimerFired();

    // Helpers used by generated Settings.cpp.
    void setNeedsRecalcStyleInAllFrames();
    void setNeedsRelayoutAllFrames();
    void mediaTypeOverrideChanged();
    void imagesEnabledChanged();
    void pluginsEnabledChanged();
    void userStyleSheetLocationChanged();
    void usesBackForwardCacheChanged();
    void dnsPrefetchingEnabledChanged();
    void storageBlockingPolicyChanged();
    void backgroundShouldExtendBeyondPageChanged();
    void scrollingPerformanceTestingEnabledChanged();
    void hiddenPageDOMTimerThrottlingStateChanged();
    void hiddenPageCSSAnimationSuspensionEnabledChanged();
    void resourceUsageOverlayVisibleChanged();
    void iceCandidateFilteringEnabledChanged();
#if ENABLE(TEXT_AUTOSIZING)
    void shouldEnableTextAutosizingBoostChanged();
    void textAutosizingUsesIdempotentModeChanged();
#endif
#if ENABLE(MEDIA_STREAM)
    void mockCaptureDevicesEnabledChanged();
#endif

    Page* m_page;

    Seconds m_minimumDOMTimerInterval;

    Timer m_setImageLoadingSettingsTimer;

    Vector<ContentType> m_mediaContentTypesRequiringHardwareSupport;

#if ENABLE(TEXT_AUTOSIZING)
    static constexpr const float boostedOneLineTextMultiplierCoefficient = 2.23125f;
    static constexpr const float boostedMultiLineTextMultiplierCoefficient = 2.48125f;
    static constexpr const float boostedMaxTextAutosizingScaleIncrease = 5.0f;
    static constexpr const float defaultOneLineTextMultiplierCoefficient = 1.7f;
    static constexpr const float defaultMultiLineTextMultiplierCoefficient = 1.95f;
    static constexpr const float defaultMaxTextAutosizingScaleIncrease = 1.7f;

    float m_oneLineTextMultiplierCoefficient { defaultOneLineTextMultiplierCoefficient };
    float m_multiLineTextMultiplierCoefficient { defaultMultiLineTextMultiplierCoefficient };
    float m_maxTextAutosizingScaleIncrease { defaultMaxTextAutosizingScaleIncrease };
#endif
};

} // namespace WebCore
