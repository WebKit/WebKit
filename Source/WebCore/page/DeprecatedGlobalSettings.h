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

#include <wtf/Forward.h>

namespace WebCore {

class DeprecatedGlobalSettings {
public:
    DeprecatedGlobalSettings() = delete;

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

    WEBCORE_EXPORT static void setShouldRespectPriorityInCSSAttributeSetters(bool);
    static bool shouldRespectPriorityInCSSAttributeSetters();

    static bool lowPowerVideoAudioBufferSizeEnabled() { return gLowPowerVideoAudioBufferSizeEnabled; }
    WEBCORE_EXPORT static void setLowPowerVideoAudioBufferSizeEnabled(bool);

    static bool resourceLoadStatisticsEnabled() { return gResourceLoadStatisticsEnabledEnabled; }
    WEBCORE_EXPORT static void setResourceLoadStatisticsEnabled(bool);

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT static void setAudioSessionCategoryOverride(unsigned);
    static unsigned audioSessionCategoryOverride();

    WEBCORE_EXPORT static void setNetworkDataUsageTrackingEnabled(bool);
    static bool networkDataUsageTrackingEnabled();

    WEBCORE_EXPORT static void setNetworkInterfaceName(const String&);
    static const String& networkInterfaceName();

    static void setDisableScreenSizeOverride(bool flag) { gDisableScreenSizeOverride = flag; }
    static bool disableScreenSizeOverride() { return gDisableScreenSizeOverride; }
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
    
#if ENABLE(MEDIA_STREAM)
    static bool mockCaptureDevicesEnabled();
    WEBCORE_EXPORT static void setMockCaptureDevicesEnabled(bool);

    static bool mediaCaptureRequiresSecureConnection();
    WEBCORE_EXPORT static void setMediaCaptureRequiresSecureConnection(bool);
#endif

    WEBCORE_EXPORT static void setAllowsAnySSLCertificate(bool);
    static bool allowsAnySSLCertificate();

private:
#if USE(AVFOUNDATION)
    WEBCORE_EXPORT static bool gAVFoundationEnabled;
    WEBCORE_EXPORT static bool gAVFoundationNSURLSessionEnabled;
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
#if PLATFORM(IOS_FAMILY)
    static bool gNetworkDataUsageTrackingEnabled;
    WEBCORE_EXPORT static bool gAVKitEnabled;
    WEBCORE_EXPORT static bool gShouldOptOutOfNetworkStateObservation;
    WEBCORE_EXPORT static bool gDisableScreenSizeOverride;
#endif
    WEBCORE_EXPORT static bool gManageAudioSession;
    
#if ENABLE(MEDIA_STREAM)
    static bool gMockCaptureDevicesEnabled;
    static bool gMediaCaptureRequiresSecureConnection;
#endif

    static bool gLowPowerVideoAudioBufferSizeEnabled;
    static bool gResourceLoadStatisticsEnabledEnabled;
    static bool gAllowsAnySSLCertificate;
};

inline bool DeprecatedGlobalSettings::isPostLoadCPUUsageMeasurementEnabled()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

inline bool DeprecatedGlobalSettings::isPostBackgroundingCPUUsageMeasurementEnabled()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

inline bool DeprecatedGlobalSettings::isPerActivityStateCPUUsageMeasurementEnabled()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

inline bool DeprecatedGlobalSettings::isPostLoadMemoryUsageMeasurementEnabled()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

inline bool DeprecatedGlobalSettings::isPostBackgroundingMemoryUsageMeasurementEnabled()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

} // namespace WebCore
