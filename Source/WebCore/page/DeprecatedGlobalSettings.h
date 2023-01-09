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
#include <wtf/text/WTFString.h>

namespace WebCore {

class DeprecatedGlobalSettings {
public:
#if PLATFORM(WIN)
    WEBCORE_EXPORT static void setShouldUseHighResolutionTimers(bool);
    static bool shouldUseHighResolutionTimers() { return shared().m_shouldUseHighResolutionTimers; }
#endif

#if USE(AVFOUNDATION)
    WEBCORE_EXPORT static void setAVFoundationEnabled(bool);
    static bool isAVFoundationEnabled() { return shared().m_AVFoundationEnabled; }
#endif

#if USE(GSTREAMER)
    WEBCORE_EXPORT static void setGStreamerEnabled(bool);
    static bool isGStreamerEnabled() { return shared().m_GStreamerEnabled; }
#endif

    WEBCORE_EXPORT static void setMockScrollbarsEnabled(bool);
    static bool mockScrollbarsEnabled() { return shared().m_mockScrollbarsEnabled; }

    WEBCORE_EXPORT static void setUsesOverlayScrollbars(bool);
    static bool usesOverlayScrollbars() { return shared().m_usesOverlayScrollbars; }

    static bool lowPowerVideoAudioBufferSizeEnabled() { return shared().m_lowPowerVideoAudioBufferSizeEnabled; }
    static void setLowPowerVideoAudioBufferSizeEnabled(bool flag) { shared().m_lowPowerVideoAudioBufferSizeEnabled = flag; }

    static bool trackingPreventionEnabled() { return shared().m_trackingPreventionEnabled; }
    WEBCORE_EXPORT static void setTrackingPreventionEnabled(bool);

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT static void setAudioSessionCategoryOverride(unsigned);
    static unsigned audioSessionCategoryOverride();

    WEBCORE_EXPORT static void setNetworkDataUsageTrackingEnabled(bool);
    static bool networkDataUsageTrackingEnabled() { return shared().m_networkDataUsageTrackingEnabled; }

    WEBCORE_EXPORT static void setNetworkInterfaceName(const String&);
    static const String& networkInterfaceName() { return shared().m_networkInterfaceName; }

    static void setDisableScreenSizeOverride(bool flag) { shared().m_disableScreenSizeOverride = flag; }
    static bool disableScreenSizeOverride() { return shared().m_disableScreenSizeOverride; }

    static void setShouldOptOutOfNetworkStateObservation(bool flag) { shared().m_shouldOptOutOfNetworkStateObservation = flag; }
    static bool shouldOptOutOfNetworkStateObservation() { return shared().m_shouldOptOutOfNetworkStateObservation; }
#endif

#if USE(AUDIO_SESSION)
    WEBCORE_EXPORT static void setShouldManageAudioSessionCategory(bool);
    WEBCORE_EXPORT static bool shouldManageAudioSessionCategory();
#endif

    WEBCORE_EXPORT static void setAllowsAnySSLCertificate(bool);
    WEBCORE_EXPORT static bool allowsAnySSLCertificate();

    static void setPaintTimingEnabled(bool isEnabled) { shared().m_isPaintTimingEnabled = isEnabled; }
    static bool paintTimingEnabled() { return shared().m_isPaintTimingEnabled; }

    static void setCustomPasteboardDataEnabled(bool isEnabled) { shared().m_isCustomPasteboardDataEnabled = isEnabled; }
    static bool customPasteboardDataEnabled() { return shared().m_isCustomPasteboardDataEnabled; }

    static bool fetchAPIKeepAliveEnabled() { return shared().m_fetchAPIKeepAliveEnabled; }
    static void setFetchAPIKeepAliveEnabled(bool isEnabled) { shared().m_fetchAPIKeepAliveEnabled = isEnabled; }

    static void setRestrictedHTTPResponseAccess(bool isEnabled) { shared().m_isRestrictedHTTPResponseAccess = isEnabled; }
    static bool restrictedHTTPResponseAccess() { return shared().m_isRestrictedHTTPResponseAccess; }

    static void setServerTimingEnabled(bool isEnabled) { shared().m_isServerTimingEnabled = isEnabled; }
    static bool serverTimingEnabled() { return shared().m_isServerTimingEnabled; }

    static void setAttrStyleEnabled(bool isEnabled) { shared().m_attrStyleEnabled = isEnabled; }
    static bool attrStyleEnabled() { return shared().m_attrStyleEnabled; }

    static void setInlineFormattingContextIntegrationEnabled(bool isEnabled) { shared().m_inlineFormattingContextIntegrationEnabled = isEnabled; }
    static bool inlineFormattingContextIntegrationEnabled() { return shared().m_inlineFormattingContextIntegrationEnabled; }

    static void setWebSQLEnabled(bool isEnabled) { shared().m_webSQLEnabled = isEnabled; }
    static bool webSQLEnabled() { return shared().m_webSQLEnabled; }

    static void setHighlightAPIEnabled(bool isEnabled) { shared().m_highlightAPIEnabled = isEnabled; }
    static bool highlightAPIEnabled() { return shared().m_highlightAPIEnabled; }

#if ENABLE(ATTACHMENT_ELEMENT)
    static void setAttachmentElementEnabled(bool areEnabled) { shared().m_isAttachmentElementEnabled = areEnabled; }
    static bool attachmentElementEnabled() { return shared().m_isAttachmentElementEnabled; }
#endif

#if ENABLE(WEB_RTC)
    static bool webRTCH264LowLatencyEncoderEnabled() { return shared().m_isWebRTCH264LowLatencyEncoderEnabled; }
    static void setWebRTCH264LowLatencyEncoderEnabled(bool isEnabled) { shared().m_isWebRTCH264LowLatencyEncoderEnabled = isEnabled; }
    static bool webRTCH264SimulcastEnabled() { return shared().m_isWebRTCH264SimulcastEnabled; }
    static void setWebRTCH264SimulcastEnabled(bool isEnabled) { shared().m_isWebRTCH264SimulcastEnabled = isEnabled; }
    static bool webRTCPlatformTCPSocketsEnabled() { return shared().m_isWebRTCPlatformTCPSocketsEnabled; }
    static void setWebRTCPlatformTCPSocketsEnabled(bool isEnabled) { shared().m_isWebRTCPlatformTCPSocketsEnabled = isEnabled; }
    static bool webRTCPlatformUDPSocketsEnabled() { return shared().m_isWebRTCPlatformUDPSocketsEnabled; }
    static void setWebRTCPlatformUDPSocketsEnabled(bool isEnabled) { shared().m_isWebRTCPlatformUDPSocketsEnabled = isEnabled; }
#endif
    static bool webRTCAudioLatencyAdaptationEnabled() { return shared().m_isWebRTCAudioLatencyAdaptationEnabled; }
    static void setWebRTCAudioLatencyAdaptationEnabled(bool isEnabled) { shared().m_isWebRTCAudioLatencyAdaptationEnabled = isEnabled; }

    static void setReadableByteStreamAPIEnabled(bool isEnabled) { shared().m_isReadableByteStreamAPIEnabled = isEnabled; }
    static bool readableByteStreamAPIEnabled() { return shared().m_isReadableByteStreamAPIEnabled; }

    static void setLineHeightUnitsEnabled(bool isEnabled) { shared().m_lineHeightUnitsEnabled = isEnabled; }
    static bool lineHeightUnitsEnabled() { return shared().m_lineHeightUnitsEnabled; }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    static void setIsAccessibilityIsolatedTreeEnabled(bool isEnabled) { shared().m_accessibilityIsolatedTree = isEnabled; }
    static bool isAccessibilityIsolatedTreeEnabled() { return shared().m_accessibilityIsolatedTree; }
#endif

    static void setArePDFImagesEnabled(bool isEnabled) { shared().m_arePDFImagesEnabled = isEnabled; }
    static bool arePDFImagesEnabled() { return shared().m_arePDFImagesEnabled; }

#if ENABLE(WEBM_FORMAT_READER)
    static void setWebMFormatReaderEnabled(bool isEnabled) { shared().m_webMFormatReaderEnabled = isEnabled; }
    static bool webMFormatReaderEnabled() { return shared().m_webMFormatReaderEnabled; }
#endif

#if ENABLE(MEDIA_SOURCE)
    static void setWebMParserEnabled(bool isEnabled) { shared().m_webMParserEnabled = isEnabled; }
    static bool webMParserEnabled() { return shared().m_webMParserEnabled; }
#endif

#if ENABLE(VORBIS)
    WEBCORE_EXPORT static void setVorbisDecoderEnabled(bool);
    static bool vorbisDecoderEnabled() { return shared().m_vorbisDecoderEnabled; }
#endif

#if ENABLE(OPUS)
    WEBCORE_EXPORT static void setOpusDecoderEnabled(bool);
    static bool opusDecoderEnabled() { return shared().m_opusDecoderEnabled; }
#endif

#if ENABLE(MEDIA_SOURCE) && (HAVE(AVSAMPLEBUFFERVIDEOOUTPUT) || USE(GSTREAMER))
    WEBCORE_EXPORT static void setMediaSourceInlinePaintingEnabled(bool);
    static bool mediaSourceInlinePaintingEnabled() { return shared().m_mediaSourceInlinePaintingEnabled; }
#endif

#if HAVE(AVCONTENTKEYSPECIFIER)
    static void setSampleBufferContentKeySessionSupportEnabled(bool);
    static bool sampleBufferContentKeySessionSupportEnabled() { return shared().m_sampleBufferContentKeySessionSupportEnabled; }
#endif

#if ENABLE(BUILT_IN_NOTIFICATIONS)
    static void setBuiltInNotificationsEnabled(bool isEnabled) { shared().m_builtInNotificationsEnabled = isEnabled; }
    static bool builtInNotificationsEnabled() { return shared().m_builtInNotificationsEnabled; }
#endif

#if ENABLE(MODEL_ELEMENT)
    static void setModelDocumentEnabled(bool isEnabled) { shared().m_modelDocumentEnabled = isEnabled; }
    static bool modelDocumentEnabled() { return shared().m_modelDocumentEnabled; }
#endif


private:
    WEBCORE_EXPORT static DeprecatedGlobalSettings& shared();
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

#if PLATFORM(WIN)
    bool m_shouldUseHighResolutionTimers { true };
#endif
#if PLATFORM(IOS_FAMILY)
    bool m_networkDataUsageTrackingEnabled { false };
    String m_networkInterfaceName;
    bool m_shouldOptOutOfNetworkStateObservation { false };
    bool m_disableScreenSizeOverride { false };
#endif

    bool m_lowPowerVideoAudioBufferSizeEnabled { false };
    bool m_trackingPreventionEnabled { false };
    bool m_allowsAnySSLCertificate { false };

    bool m_isPaintTimingEnabled { false };

    bool m_isCustomPasteboardDataEnabled { false };
    bool m_fetchAPIKeepAliveEnabled { false };
    bool m_isRestrictedHTTPResponseAccess { true };
    bool m_isServerTimingEnabled { false };
    bool m_attrStyleEnabled { false };
    bool m_webSQLEnabled { false };
    bool m_highlightAPIEnabled { false };

    bool m_inlineFormattingContextIntegrationEnabled { true };

#if ENABLE(ATTACHMENT_ELEMENT)
    bool m_isAttachmentElementEnabled { false };
#endif

#if ENABLE(WEB_RTC)
    bool m_isWebRTCH264SimulcastEnabled { true };
    bool m_isWebRTCH264LowLatencyEncoderEnabled { false };
    bool m_isWebRTCPlatformTCPSocketsEnabled { false };
    bool m_isWebRTCPlatformUDPSocketsEnabled { false };
#endif
    bool m_isWebRTCAudioLatencyAdaptationEnabled { true };

    bool m_isReadableByteStreamAPIEnabled { false };

    bool m_lineHeightUnitsEnabled { true };

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    bool m_accessibilityIsolatedTree { false };
#endif

    bool m_arePDFImagesEnabled { true };

#if ENABLE(WEBM_FORMAT_READER)
    bool m_webMFormatReaderEnabled { false };
#endif

#if ENABLE(MEDIA_SOURCE)
    bool m_webMParserEnabled { false };
#endif

#if ENABLE(VORBIS)
    bool m_vorbisDecoderEnabled { false };
#endif

#if ENABLE(OPUS)
    bool m_opusDecoderEnabled { false };
#endif

#if ENABLE(MEDIA_SOURCE) && (HAVE(AVSAMPLEBUFFERVIDEOOUTPUT) || USE(GSTREAMER))
    bool m_mediaSourceInlinePaintingEnabled { false };
#endif

#if HAVE(AVCONTENTKEYSPECIFIER)
    bool m_sampleBufferContentKeySessionSupportEnabled { false };
#endif

#if ENABLE(BUILT_IN_NOTIFICATIONS)
    bool m_builtInNotificationsEnabled { false };
#endif

#if ENABLE(MODEL_ELEMENT)
    bool m_modelDocumentEnabled { false };
#endif

    friend class NeverDestroyed<DeprecatedGlobalSettings>;
};

} // namespace WebCore
