/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

    static void setShouldOptOutOfNetworkStateObservation(bool flag) { gShouldOptOutOfNetworkStateObservation = flag; }
    static bool shouldOptOutOfNetworkStateObservation() { return gShouldOptOutOfNetworkStateObservation; }
#endif

#if USE(AUDIO_SESSION)
    WEBCORE_EXPORT static void setShouldManageAudioSessionCategory(bool flag);
    WEBCORE_EXPORT static bool shouldManageAudioSessionCategory();
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
#if PLATFORM(IOS_FAMILY)
    static bool gNetworkDataUsageTrackingEnabled;
    WEBCORE_EXPORT static bool gShouldOptOutOfNetworkStateObservation;
    WEBCORE_EXPORT static bool gDisableScreenSizeOverride;
#endif
    WEBCORE_EXPORT static bool gManageAudioSession;

    static bool gLowPowerVideoAudioBufferSizeEnabled;
    WEBCORE_EXPORT static bool gResourceLoadStatisticsEnabledEnabled;
    static bool gAllowsAnySSLCertificate;
};

} // namespace WebCore
