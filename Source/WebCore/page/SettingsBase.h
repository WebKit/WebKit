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
#include "URL.h"
#include "WritingMode.h"
#include <runtime/RuntimeFlags.h>
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
#if PLATFORM(IOS)
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

    enum class ForcedAccessibilityValue { System, On, Off };
    static const SettingsBase::ForcedAccessibilityValue defaultForcedColorsAreInvertedAccessibilityValue = ForcedAccessibilityValue::System;
    static const SettingsBase::ForcedAccessibilityValue defaultForcedDisplayIsMonochromeAccessibilityValue = ForcedAccessibilityValue::System;
    static const SettingsBase::ForcedAccessibilityValue defaultForcedPrefersReducedMotionAccessibilityValue = ForcedAccessibilityValue::System;

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


    WEBCORE_EXPORT static bool defaultTextAutosizingEnabled();
    WEBCORE_EXPORT static float defaultMinimumZoomFontSize();

#if PLATFORM(WIN)
    static void setShouldUseHighResolutionTimers(bool);
    static bool shouldUseHighResolutionTimers() { return gShouldUseHighResolutionTimers; }
#endif

    static bool isPostLoadCPUUsageMeasurementEnabled();
    static bool isPostBackgroundingCPUUsageMeasurementEnabled();
    static bool isPerActivityStateCPUUsageMeasurementEnabled();

    static bool isPostLoadMemoryUsageMeasurementEnabled();
    static bool isPostBackgroundingMemoryUsageMeasurementEnabled();

    static bool globalConstRedeclarationShouldThrow();

#if USE(AVFOUNDATION)
    WEBCORE_EXPORT static void setAVFoundationEnabled(bool flag);
    static bool isAVFoundationEnabled() { return gAVFoundationEnabled; }
    WEBCORE_EXPORT static void setAVFoundationNSURLSessionEnabled(bool flag);
    static bool isAVFoundationNSURLSessionEnabled() { return gAVFoundationNSURLSessionEnabled; }
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT static void setQTKitEnabled(bool flag);
    static bool isQTKitEnabled() { return gQTKitEnabled; }
#else
    static bool isQTKitEnabled() { return false; }
#endif

#if USE(GSTREAMER)
    WEBCORE_EXPORT static void setGStreamerEnabled(bool flag);
    static bool isGStreamerEnabled() { return gGStreamerEnabled; }
#endif

    static const unsigned defaultMaximumHTMLParserDOMTreeDepth = 512;
    static const unsigned defaultMaximumRenderTreeDepth = 512;

    WEBCORE_EXPORT static void setMockScrollbarsEnabled(bool flag);
    WEBCORE_EXPORT static bool mockScrollbarsEnabled();

    WEBCORE_EXPORT static void setUsesOverlayScrollbars(bool flag);
    static bool usesOverlayScrollbars();

    WEBCORE_EXPORT static void setUsesMockScrollAnimator(bool);
    static bool usesMockScrollAnimator();

    WEBCORE_EXPORT static void setShouldRespectPriorityInCSSAttributeSetters(bool);
    static bool shouldRespectPriorityInCSSAttributeSetters();

    static bool lowPowerVideoAudioBufferSizeEnabled() { return gLowPowerVideoAudioBufferSizeEnabled; }
    WEBCORE_EXPORT static void setLowPowerVideoAudioBufferSizeEnabled(bool);

    static bool resourceLoadStatisticsEnabled() { return gResourceLoadStatisticsEnabledEnabled; }
    WEBCORE_EXPORT static void setResourceLoadStatisticsEnabled(bool);

#if PLATFORM(IOS)
    WEBCORE_EXPORT static void setAudioSessionCategoryOverride(unsigned);
    static unsigned audioSessionCategoryOverride();

    WEBCORE_EXPORT static void setNetworkDataUsageTrackingEnabled(bool);
    static bool networkDataUsageTrackingEnabled();

    WEBCORE_EXPORT static void setNetworkInterfaceName(const String&);
    static const String& networkInterfaceName();

#if HAVE(AVKIT)
    static void setAVKitEnabled(bool flag) { gAVKitEnabled = flag; }
#endif
    static bool avKitEnabled() { return gAVKitEnabled; }

    static void setShouldOptOutOfNetworkStateObservation(bool flag) { gShouldOptOutOfNetworkStateObservation = flag; }
    static bool shouldOptOutOfNetworkStateObservation() { return gShouldOptOutOfNetworkStateObservation; }
#endif

#if USE(AUDIO_SESSION)
    static void setShouldManageAudioSessionCategory(bool flag) { gManageAudioSession = flag; }
    static bool shouldManageAudioSessionCategory() { return gManageAudioSession; }
#endif

    static void setCustomPasteboardDataEnabled(bool enabled) { gCustomPasteboardDataEnabled = enabled; }
    static bool customPasteboardDataEnabled() { return gCustomPasteboardDataEnabled; }
    WEBCORE_EXPORT static bool defaultCustomPasteboardDataEnabled();
    
#if ENABLE(MEDIA_STREAM)
    static bool mockCaptureDevicesEnabled();
    WEBCORE_EXPORT static void setMockCaptureDevicesEnabled(bool);

    bool mediaCaptureRequiresSecureConnection() const;
    WEBCORE_EXPORT static void setMediaCaptureRequiresSecureConnection(bool);
#endif

    WEBCORE_EXPORT static void setAllowsAnySSLCertificate(bool);
    static bool allowsAnySSLCertificate();

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
    void mediaTypeOverrideChanged();
    void imagesEnabledChanged();
    void scriptEnabledChanged();
    void pluginsEnabledChanged();
    void userStyleSheetLocationChanged();
    void usesPageCacheChanged();
    void dnsPrefetchingEnabledChanged();
    void resourceUsageOverlayVisibleChanged();
    void storageBlockingPolicyChanged();
    void backgroundShouldExtendBeyondPageChanged();
    void scrollingPerformanceLoggingEnabledChanged();
    void hiddenPageDOMTimerThrottlingStateChanged();
    void hiddenPageCSSAnimationSuspensionEnabledChanged();

    Page* m_page;

    const std::unique_ptr<FontGenericFamilies> m_fontGenericFamilies;
    Seconds m_layoutInterval;
    Seconds m_minimumDOMTimerInterval;

    Timer m_setImageLoadingSettingsTimer;

    Vector<ContentType> m_mediaContentTypesRequiringHardwareSupport;

#if USE(AVFOUNDATION)
    WEBCORE_EXPORT static bool gAVFoundationEnabled;
    WEBCORE_EXPORT static bool gAVFoundationNSURLSessionEnabled;
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT static bool gQTKitEnabled;
#endif

#if USE(GSTREAMER)
    WEBCORE_EXPORT static bool gGStreamerEnabled;
#endif

    static bool gMockScrollbarsEnabled;
    static bool gUsesOverlayScrollbars;
    static bool gMockScrollAnimatorEnabled;

#if PLATFORM(WIN)
    static bool gShouldUseHighResolutionTimers;
#endif
    static bool gShouldRespectPriorityInCSSAttributeSetters;
#if PLATFORM(IOS)
    static bool gNetworkDataUsageTrackingEnabled;
    WEBCORE_EXPORT static bool gAVKitEnabled;
    WEBCORE_EXPORT static bool gShouldOptOutOfNetworkStateObservation;
#endif
    WEBCORE_EXPORT static bool gManageAudioSession;
    WEBCORE_EXPORT static bool gCustomPasteboardDataEnabled;
    
#if ENABLE(MEDIA_STREAM)
    static bool gMockCaptureDevicesEnabled;
    static bool gMediaCaptureRequiresSecureConnection;
#endif

    static bool gLowPowerVideoAudioBufferSizeEnabled;
    static bool gResourceLoadStatisticsEnabledEnabled;
    static bool gAllowsAnySSLCertificate;
};

inline bool SettingsBase::isPostLoadCPUUsageMeasurementEnabled()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

inline bool SettingsBase::isPostBackgroundingCPUUsageMeasurementEnabled()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

inline bool SettingsBase::isPerActivityStateCPUUsageMeasurementEnabled()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

inline bool SettingsBase::isPostLoadMemoryUsageMeasurementEnabled()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

inline bool SettingsBase::isPostBackgroundingMemoryUsageMeasurementEnabled()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

} // namespace WebCore
