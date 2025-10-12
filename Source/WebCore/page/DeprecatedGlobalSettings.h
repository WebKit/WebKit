/*
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
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
#include <wtf/Platform.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DeprecatedGlobalSettings {
public:
#if USE(AVFOUNDATION)
    WEBCORE_EXPORT static void setAVFoundationEnabled(bool);
    static bool isAVFoundationEnabled() { return singleton().m_AVFoundationEnabled; }
#endif

#if USE(GSTREAMER)
    WEBCORE_EXPORT static void setGStreamerEnabled(bool);
    static bool isGStreamerEnabled() { return singleton().m_GStreamerEnabled; }
#endif

    WEBCORE_EXPORT static void setMockScrollbarsEnabled(bool);
    static bool mockScrollbarsEnabled() { return singleton().m_mockScrollbarsEnabled; }

    WEBCORE_EXPORT static void setUsesOverlayScrollbars(bool);
    static bool usesOverlayScrollbars() { return singleton().m_usesOverlayScrollbars; }

    static bool lowPowerVideoAudioBufferSizeEnabled() { return singleton().m_lowPowerVideoAudioBufferSizeEnabled; }
    static void setLowPowerVideoAudioBufferSizeEnabled(bool flag) { singleton().m_lowPowerVideoAudioBufferSizeEnabled = flag; }

    static bool trackingPreventionEnabled() { return singleton().m_trackingPreventionEnabled; }
    WEBCORE_EXPORT static void setTrackingPreventionEnabled(bool);

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT static void setAudioSessionCategoryOverride(unsigned);
    static unsigned audioSessionCategoryOverride();

    WEBCORE_EXPORT static void setNetworkInterfaceName(const String&);
    static const String& networkInterfaceName() { return singleton().m_networkInterfaceName; }

    static void setDisableScreenSizeOverride(bool flag) { singleton().m_disableScreenSizeOverride = flag; }
    static bool disableScreenSizeOverride() { return singleton().m_disableScreenSizeOverride; }

    static void setShouldOptOutOfNetworkStateObservation(bool flag) { singleton().m_shouldOptOutOfNetworkStateObservation = flag; }
    static bool shouldOptOutOfNetworkStateObservation() { return singleton().m_shouldOptOutOfNetworkStateObservation; }
#endif

#if USE(AUDIO_SESSION)
    WEBCORE_EXPORT static void setShouldManageAudioSessionCategory(bool);
    WEBCORE_EXPORT static bool shouldManageAudioSessionCategory();
#endif

    WEBCORE_EXPORT static void setAllowsAnySSLCertificate(bool);
    WEBCORE_EXPORT static bool allowsAnySSLCertificate();

    static void setCustomPasteboardDataEnabled(bool isEnabled) { singleton().m_isCustomPasteboardDataEnabled = isEnabled; }
    static bool customPasteboardDataEnabled() { return singleton().m_isCustomPasteboardDataEnabled; }

    static void setAttrStyleEnabled(bool isEnabled) { singleton().m_attrStyleEnabled = isEnabled; }
    static bool attrStyleEnabled() { return singleton().m_attrStyleEnabled; }

    static void setWebSQLEnabled(bool isEnabled) { singleton().m_webSQLEnabled = isEnabled; }
    static bool webSQLEnabled() { return singleton().m_webSQLEnabled; }

#if ENABLE(ATTACHMENT_ELEMENT)
    static void setAttachmentElementEnabled(bool areEnabled) { singleton().m_isAttachmentElementEnabled = areEnabled; }
    static bool attachmentElementEnabled() { return singleton().m_isAttachmentElementEnabled; }
#endif

    static bool webRTCAudioLatencyAdaptationEnabled() { return singleton().m_isWebRTCAudioLatencyAdaptationEnabled; }
    static void setWebRTCAudioLatencyAdaptationEnabled(bool isEnabled) { singleton().m_isWebRTCAudioLatencyAdaptationEnabled = isEnabled; }

    static void setReadableByteStreamAPIEnabled(bool isEnabled) { singleton().m_isReadableByteStreamAPIEnabled = isEnabled; }
    static bool readableByteStreamAPIEnabled() { return singleton().m_isReadableByteStreamAPIEnabled; }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    static void setIsAccessibilityIsolatedTreeEnabled(bool isEnabled) { singleton().m_accessibilityIsolatedTree = isEnabled; }
    static bool isAccessibilityIsolatedTreeEnabled() { return singleton().m_accessibilityIsolatedTree; }
#endif

#if ENABLE(AX_THREAD_TEXT_APIS)
    static void setAccessibilityThreadTextApisEnabled(bool isEnabled) { singleton().m_accessibilityThreadTextApis = isEnabled; }
    static bool accessibilityThreadTextApisEnabled() { return singleton().m_accessibilityThreadTextApis; }
#endif

    static void setArePDFImagesEnabled(bool isEnabled) { singleton().m_arePDFImagesEnabled = isEnabled; }
    static bool arePDFImagesEnabled() { return singleton().m_arePDFImagesEnabled; }

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    static void setBuiltInNotificationsEnabled(bool isEnabled) { singleton().m_builtInNotificationsEnabled = isEnabled; }
    WEBCORE_EXPORT static bool builtInNotificationsEnabled();
#endif

#if ENABLE(MODEL_ELEMENT)
    static void setModelDocumentEnabled(bool isEnabled) { singleton().m_modelDocumentEnabled = isEnabled; }
    static bool modelDocumentEnabled() { return singleton().m_modelDocumentEnabled; }
#endif

#if HAVE(WEBCONTENTRESTRICTIONS)
    static void setUsesWebContentRestrictionsForFilter(bool uses) { singleton().m_usesWebContentRestrictionsForFilter = uses; }
    static bool usesWebContentRestrictionsForFilter() { return singleton().m_usesWebContentRestrictionsForFilter; };
#endif

private:
    WEBCORE_EXPORT static DeprecatedGlobalSettings& singleton();
    DeprecatedGlobalSettings() = default;
    ~DeprecatedGlobalSettings() = default;

#if USE(AVFOUNDATION)
    bool m_AVFoundationEnabled { true };
#endif

#if USE(GSTREAMER)
    bool m_GStreamerEnabled { true };
#endif

    bool m_mockScrollbarsEnabled { false };
    bool m_usesOverlayScrollbars { false };

#if PLATFORM(IOS_FAMILY)
    String m_networkInterfaceName;
    bool m_shouldOptOutOfNetworkStateObservation { false };
    bool m_disableScreenSizeOverride { false };
#endif

    bool m_lowPowerVideoAudioBufferSizeEnabled { false };
    bool m_trackingPreventionEnabled { false };
    bool m_allowsAnySSLCertificate { false };

    bool m_isCustomPasteboardDataEnabled { false };
    bool m_attrStyleEnabled { false };
    bool m_webSQLEnabled { false };

#if ENABLE(ATTACHMENT_ELEMENT)
    bool m_isAttachmentElementEnabled { false };
#endif

    bool m_isWebRTCAudioLatencyAdaptationEnabled { true };

    bool m_isReadableByteStreamAPIEnabled { false };

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    bool m_accessibilityIsolatedTree { false };
#endif

#if ENABLE(AX_THREAD_TEXT_APIS)
    bool m_accessibilityThreadTextApis { false };
#endif

    bool m_arePDFImagesEnabled { true };

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    bool m_builtInNotificationsEnabled { false };
#endif

#if ENABLE(MODEL_ELEMENT)
    bool m_modelDocumentEnabled { false };
#endif

#if HAVE(WEBCONTENTRESTRICTIONS)
    bool m_usesWebContentRestrictionsForFilter { false };
#endif
    friend class NeverDestroyed<DeprecatedGlobalSettings>;
};

} // namespace WebCore
