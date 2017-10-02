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

#include "ContentType.h"
#include "SecurityOrigin.h"
#include "SettingsDefaultValues.h"
#include "SettingsGenerated.h"
#include "Timer.h"
#include "URL.h"
#include <unicode/uscript.h>
#include <wtf/HashMap.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

class FontGenericFamilies;

class Settings : public SettingsGenerated {
public:
    static Ref<Settings> create(Page*);
    ~Settings();

    void pageDestroyed() { m_page = nullptr; }

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

    WEBCORE_EXPORT static bool defaultTextAutosizingEnabled();
    WEBCORE_EXPORT static float defaultMinimumZoomFontSize();

    // Only set by Layout Tests.
    WEBCORE_EXPORT void setMediaTypeOverride(const String&);
    const String& mediaTypeOverride() const { return m_mediaTypeOverride; }

    // Unlike areImagesEnabled, this only suppresses the network load of
    // the image URL.  A cached image will still be rendered if requested.
    WEBCORE_EXPORT void setLoadsImagesAutomatically(bool);
    bool loadsImagesAutomatically() const { return m_loadsImagesAutomatically; }

    // Clients that execute script should call ScriptController::canExecuteScripts()
    // instead of this function. ScriptController::canExecuteScripts() checks the
    // HTML sandbox, plug-in sandboxing, and other important details.
    bool isScriptEnabled() const { return m_isScriptEnabled; }
    WEBCORE_EXPORT void setScriptEnabled(bool);

    WEBCORE_EXPORT void setImagesEnabled(bool);
    bool areImagesEnabled() const { return m_areImagesEnabled; }

    WEBCORE_EXPORT void setPluginsEnabled(bool);
    bool arePluginsEnabled() const { return m_arePluginsEnabled; }

    WEBCORE_EXPORT void setDNSPrefetchingEnabled(bool);
    bool dnsPrefetchingEnabled() const { return m_dnsPrefetchingEnabled; }

    WEBCORE_EXPORT void setUserStyleSheetLocation(const URL&);
    const URL& userStyleSheetLocation() const { return m_userStyleSheetLocation; }

    WEBCORE_EXPORT void setMinimumDOMTimerInterval(Seconds); // Initialized to DOMTimer::defaultMinimumInterval().
    Seconds minimumDOMTimerInterval() const { return m_minimumDOMTimerInterval; }

    WEBCORE_EXPORT void setLayoutInterval(Seconds);
    Seconds layoutInterval() const { return m_layoutInterval; }

    bool hiddenPageDOMTimerThrottlingEnabled() const { return m_hiddenPageDOMTimerThrottlingEnabled; }
    WEBCORE_EXPORT void setHiddenPageDOMTimerThrottlingEnabled(bool);
    bool hiddenPageDOMTimerThrottlingAutoIncreases() const { return m_hiddenPageDOMTimerThrottlingAutoIncreases; }
    WEBCORE_EXPORT void setHiddenPageDOMTimerThrottlingAutoIncreases(bool);

    WEBCORE_EXPORT void setUsesPageCache(bool);
    bool usesPageCache() const { return m_usesPageCache; }

#if ENABLE(RESOURCE_USAGE)
    bool resourceUsageOverlayVisible() const { return m_resourceUsageOverlayVisible; }
    WEBCORE_EXPORT void setResourceUsageOverlayVisible(bool);
#endif

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

    WEBCORE_EXPORT void setBackgroundShouldExtendBeyondPage(bool);
    bool backgroundShouldExtendBeyondPage() const { return m_backgroundShouldExtendBeyondPage; }

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

    WEBCORE_EXPORT static void setMockScrollbarsEnabled(bool flag);
    WEBCORE_EXPORT static bool mockScrollbarsEnabled();

    WEBCORE_EXPORT static void setUsesOverlayScrollbars(bool flag);
    static bool usesOverlayScrollbars();

    WEBCORE_EXPORT static void setUsesMockScrollAnimator(bool);
    static bool usesMockScrollAnimator();

    WEBCORE_EXPORT void setStorageBlockingPolicy(SecurityOrigin::StorageBlockingPolicy);
    SecurityOrigin::StorageBlockingPolicy storageBlockingPolicy() const { return m_storageBlockingPolicy; }

    WEBCORE_EXPORT void setScrollingPerformanceLoggingEnabled(bool);
    bool scrollingPerformanceLoggingEnabled() { return m_scrollingPerformanceLoggingEnabled; }

    WEBCORE_EXPORT static void setShouldRespectPriorityInCSSAttributeSetters(bool);
    static bool shouldRespectPriorityInCSSAttributeSetters();

    bool hiddenPageCSSAnimationSuspensionEnabled() const { return m_hiddenPageCSSAnimationSuspensionEnabled; }
    WEBCORE_EXPORT void setHiddenPageCSSAnimationSuspensionEnabled(bool);

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
    WEBCORE_EXPORT static bool customPasteboardDataEnabled();
    
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

private:
    explicit Settings(Page*);

    void initializeDefaultFontFamilies();

    String m_mediaTypeOverride;
    URL m_userStyleSheetLocation;
    const std::unique_ptr<FontGenericFamilies> m_fontGenericFamilies;
    SecurityOrigin::StorageBlockingPolicy m_storageBlockingPolicy;
    Seconds m_layoutInterval;
    Seconds m_minimumDOMTimerInterval;

    bool m_loadsImagesAutomatically : 1;
    bool m_areImagesEnabled : 1;
    bool m_arePluginsEnabled : 1;
    bool m_isScriptEnabled : 1;
    bool m_usesPageCache : 1;
    bool m_backgroundShouldExtendBeyondPage : 1;
    bool m_dnsPrefetchingEnabled : 1;

    bool m_scrollingPerformanceLoggingEnabled : 1;

    Timer m_setImageLoadingSettingsTimer;
    void imageLoadingSettingsTimerFired();

    bool m_hiddenPageDOMTimerThrottlingEnabled : 1;
    bool m_hiddenPageCSSAnimationSuspensionEnabled : 1;

#if ENABLE(RESOURCE_USAGE)
    bool m_resourceUsageOverlayVisible { false };
#endif

    bool m_hiddenPageDOMTimerThrottlingAutoIncreases { false };

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

    Vector<ContentType> m_mediaContentTypesRequiringHardwareSupport;
};

inline bool Settings::isPostLoadCPUUsageMeasurementEnabled()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

inline bool Settings::isPostBackgroundingCPUUsageMeasurementEnabled()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

inline bool Settings::isPerActivityStateCPUUsageMeasurementEnabled()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

inline bool Settings::isPostLoadMemoryUsageMeasurementEnabled()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

inline bool Settings::isPostBackgroundingMemoryUsageMeasurementEnabled()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

} // namespace WebCore
