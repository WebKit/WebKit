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
    static void setShouldUseHighResolutionTimers(bool);
    static bool shouldUseHighResolutionTimers() { return shared().m_shouldUseHighResolutionTimers; }
#endif

#if USE(AVFOUNDATION)
    WEBCORE_EXPORT static void setAVFoundationEnabled(bool flag);
    static bool isAVFoundationEnabled() { return shared().m_AVFoundationEnabled; }
#endif

#if USE(GSTREAMER)
    WEBCORE_EXPORT static void setGStreamerEnabled(bool flag);
    static bool isGStreamerEnabled() { return shared().m_GStreamerEnabled; }
#endif

    WEBCORE_EXPORT static void setMockScrollbarsEnabled(bool flag);
    WEBCORE_EXPORT static bool mockScrollbarsEnabled();

    WEBCORE_EXPORT static void setUsesOverlayScrollbars(bool flag);
    static bool usesOverlayScrollbars();

    static bool lowPowerVideoAudioBufferSizeEnabled() { return shared().m_lowPowerVideoAudioBufferSizeEnabled; }
    WEBCORE_EXPORT static void setLowPowerVideoAudioBufferSizeEnabled(bool);

    static bool trackingPreventionEnabled() { return shared().m_trackingPreventionEnabled; }
    WEBCORE_EXPORT static void setTrackingPreventionEnabled(bool);

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT static void setAudioSessionCategoryOverride(unsigned);
    static unsigned audioSessionCategoryOverride();

    WEBCORE_EXPORT static void setNetworkDataUsageTrackingEnabled(bool);
    static bool networkDataUsageTrackingEnabled();

    WEBCORE_EXPORT static void setNetworkInterfaceName(const String&);
    static const String& networkInterfaceName();

    static void setDisableScreenSizeOverride(bool flag) { shared().m_disableScreenSizeOverride = flag; }
    static bool disableScreenSizeOverride() { return shared().m_disableScreenSizeOverride; }

    static void setShouldOptOutOfNetworkStateObservation(bool flag) { shared().m_shouldOptOutOfNetworkStateObservation = flag; }
    static bool shouldOptOutOfNetworkStateObservation() { return shared().m_shouldOptOutOfNetworkStateObservation; }
#endif

#if USE(AUDIO_SESSION)
    WEBCORE_EXPORT static void setShouldManageAudioSessionCategory(bool flag);
    WEBCORE_EXPORT static bool shouldManageAudioSessionCategory();
#endif

    WEBCORE_EXPORT static void setAllowsAnySSLCertificate(bool);
    WEBCORE_EXPORT static bool allowsAnySSLCertificate();

    static void setPaintTimingEnabled(bool isEnabled) { shared().m_isPaintTimingEnabled = isEnabled; }
    static bool paintTimingEnabled() { return shared().m_isPaintTimingEnabled; }

    static void setMenuItemElementEnabled(bool isEnabled) { shared().m_isMenuItemElementEnabled = isEnabled; }
    static bool menuItemElementEnabled() { return shared().m_isMenuItemElementEnabled; }

    static void setDirectoryUploadEnabled(bool isEnabled) { shared().m_isDirectoryUploadEnabled = isEnabled; }
    static bool directoryUploadEnabled() { return shared().m_isDirectoryUploadEnabled; }

    static void setCustomPasteboardDataEnabled(bool isEnabled) { shared().m_isCustomPasteboardDataEnabled = isEnabled; }
    static bool customPasteboardDataEnabled() { return shared().m_isCustomPasteboardDataEnabled; }

#if ENABLE(OFFSCREEN_CANVAS)
    static void setOffscreenCanvasEnabled(bool isEnabled) { shared().m_isOffscreenCanvasEnabled = isEnabled; }
    static bool offscreenCanvasEnabled() { return shared().m_isOffscreenCanvasEnabled; }
#endif

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    static void setOffscreenCanvasInWorkersEnabled(bool isEnabled) { shared().m_isOffscreenCanvasInWorkersEnabled = isEnabled; }
    static bool offscreenCanvasInWorkersEnabled() { return shared().m_isOffscreenCanvasInWorkersEnabled; }
#endif

    static void setCacheAPIEnabled(bool isEnabled) { shared().m_isCacheAPIEnabled = isEnabled; }
    static bool cacheAPIEnabled() { return shared().m_isCacheAPIEnabled; }

    static void setWebSocketEnabled(bool isEnabled) { shared().m_isWebSocketEnabled = isEnabled; }
    static bool webSocketEnabled() { return shared().m_isWebSocketEnabled; }

    static bool fetchAPIKeepAliveEnabled() { return shared().m_fetchAPIKeepAliveEnabled; }
    static void setFetchAPIKeepAliveEnabled(bool isEnabled) { shared().m_fetchAPIKeepAliveEnabled = isEnabled; }

    static void setAccessibilityObjectModelEnabled(bool isEnabled) { shared().m_accessibilityObjectModelEnabled = isEnabled; }
    static bool accessibilityObjectModelEnabled() { return shared().m_accessibilityObjectModelEnabled; }

    static void setItpDebugModeEnabled(bool isEnabled) { shared().m_itpDebugMode = isEnabled; }
    static bool itpDebugModeEnabled() { return shared().m_itpDebugMode; }

    static void setRestrictedHTTPResponseAccess(bool isEnabled) { shared().m_isRestrictedHTTPResponseAccess = isEnabled; }
    static bool restrictedHTTPResponseAccess() { return shared().m_isRestrictedHTTPResponseAccess; }

    static void setServerTimingEnabled(bool isEnabled) { shared().m_isServerTimingEnabled = isEnabled; }
    static bool serverTimingEnabled() { return shared().m_isServerTimingEnabled; }

    static void setAttrStyleEnabled(bool isEnabled) { shared().m_attrStyleEnabled = isEnabled; }
    static bool attrStyleEnabled() { return shared().m_attrStyleEnabled; }

    static void setWebAPIStatisticsEnabled(bool isEnabled) { shared().m_webAPIStatisticsEnabled = isEnabled; }
    static bool webAPIStatisticsEnabled() { return shared().m_webAPIStatisticsEnabled; }

    static void setLayoutFormattingContextEnabled(bool isEnabled) { shared().m_layoutFormattingContextEnabled = isEnabled; }
    static bool layoutFormattingContextEnabled() { return shared().m_layoutFormattingContextEnabled; }

    static void setInlineFormattingContextIntegrationEnabled(bool isEnabled) { shared().m_inlineFormattingContextIntegrationEnabled = isEnabled; }
    static bool inlineFormattingContextIntegrationEnabled() { return shared().m_inlineFormattingContextIntegrationEnabled; }

#if ENABLE(CSS_PAINTING_API)
    static void setCSSPaintingAPIEnabled(bool isEnabled) { shared().m_CSSPaintingAPIEnabled = isEnabled; }
    static bool cssPaintingAPIEnabled() { return shared().m_CSSPaintingAPIEnabled; }
#endif

    static void setCSSTypedOMEnabled(bool isEnabled) { shared().m_CSSTypedOMEnabled = isEnabled; }
    static bool cssTypedOMEnabled() { return shared().m_CSSTypedOMEnabled; }

    static void setWebSQLEnabled(bool isEnabled) { shared().m_webSQLEnabled = isEnabled; }
    static bool webSQLEnabled() { return shared().m_webSQLEnabled; }

    static void setHighlightAPIEnabled(bool isEnabled) { shared().m_highlightAPIEnabled = isEnabled; }
    static bool highlightAPIEnabled() { return shared().m_highlightAPIEnabled; }

#if ENABLE(ATTACHMENT_ELEMENT)
    static void setAttachmentElementEnabled(bool areEnabled) { shared().m_isAttachmentElementEnabled = areEnabled; }
    static bool attachmentElementEnabled() { return shared().m_isAttachmentElementEnabled; }
#endif

    static bool userGesturePromisePropagationEnabled() { return shared().m_userGesturePromisePropagationEnabled; }
    static void setUserGesturePromisePropagationEnabled(bool isEnabled) { shared().m_userGesturePromisePropagationEnabled = isEnabled; }

#if ENABLE(WEB_RTC)
    static bool webRTCDTMFEnabled() { return shared().m_isWebRTCDTMFEnabled; }
    static void setWebRTCDTMFEnabled(bool isEnabled) { shared().m_isWebRTCDTMFEnabled = isEnabled; }
    static bool webRTCH264LowLatencyEncoderEnabled() { return shared().m_isWebRTCH264LowLatencyEncoderEnabled; }
    static void setWebRTCH264LowLatencyEncoderEnabled(bool isEnabled) { shared().m_isWebRTCH264LowLatencyEncoderEnabled = isEnabled; }
    static bool webRTCMDNSICECandidatesEnabled() { return shared().m_isWebRTCMDNSICECandidatesEnabled; }
    static void setWebRTCMDNSICECandidatesEnabled(bool isEnabled) { shared().m_isWebRTCMDNSICECandidatesEnabled = isEnabled; }
    static bool webRTCH264SimulcastEnabled() { return shared().m_isWebRTCH264SimulcastEnabled; }
    static void setWebRTCH264SimulcastEnabled(bool isEnabled) { shared().m_isWebRTCH264SimulcastEnabled = isEnabled; }
    static bool webRTCPlatformTCPSocketsEnabled() { return shared().m_isWebRTCPlatformTCPSocketsEnabled; }
    static void setWebRTCPlatformTCPSocketsEnabled(bool isEnabled) { shared().m_isWebRTCPlatformTCPSocketsEnabled = isEnabled; }
    static bool webRTCPlatformUDPSocketsEnabled() { return shared().m_isWebRTCPlatformUDPSocketsEnabled; }
    static void setWebRTCPlatformUDPSocketsEnabled(bool isEnabled) { shared().m_isWebRTCPlatformUDPSocketsEnabled = isEnabled; }
#endif
    static bool webRTCAudioLatencyAdaptationEnabled() { return shared().m_isWebRTCAudioLatencyAdaptationEnabled; }
    static void setWebRTCAudioLatencyAdaptationEnabled(bool isEnabled) { shared().m_isWebRTCAudioLatencyAdaptationEnabled = isEnabled; }

#if ENABLE(DATALIST_ELEMENT)
    static bool dataListElementEnabled() { return shared().m_isDataListElementEnabled; }
    static void setDataListElementEnabled(bool isEnabled) { shared().m_isDataListElementEnabled = isEnabled; }
#endif

    static void setReadableByteStreamAPIEnabled(bool isEnabled) { shared().m_isReadableByteStreamAPIEnabled = isEnabled; }
    static bool readableByteStreamAPIEnabled() { return shared().m_isReadableByteStreamAPIEnabled; }

    static void setCSSLogicalEnabled(bool isEnabled) { shared().m_CSSLogicalEnabled = isEnabled; }
    static bool cssLogicalEnabled() { return shared().m_CSSLogicalEnabled; }

    static void setLineHeightUnitsEnabled(bool isEnabled) { shared().m_lineHeightUnitsEnabled = isEnabled; }
    static bool lineHeightUnitsEnabled() { return shared().m_lineHeightUnitsEnabled; }

    static bool privateClickMeasurementDebugModeEnabled() { return shared().m_privateClickMeasurementDebugModeEnabled; }
    static void setPrivateClickMeasurementDebugModeEnabled(bool isEnabled) { shared().m_privateClickMeasurementDebugModeEnabled = isEnabled; }
    static bool privateClickMeasurementFraudPreventionEnabled() { return shared().m_privateClickMeasurementFraudPreventionEnabled; }
    static void setPrivateClickMeasurementFraudPreventionEnabled(bool isEnabled) { shared().m_privateClickMeasurementFraudPreventionEnabled = isEnabled; }

#if ENABLE(TOUCH_EVENTS)
    static bool mouseEventsSimulationEnabled() { return shared().m_mouseEventsSimulationEnabled; }
    static void setMouseEventsSimulationEnabled(bool isEnabled) { shared().m_mouseEventsSimulationEnabled = isEnabled; }
    static bool touchEventsEnabled();
    static void setTouchEventsEnabled(bool isEnabled) { shared().m_touchEventsEnabled = isEnabled; }
#endif

    static bool pageAtRuleSupportEnabled() { return shared().m_pageAtRuleSupportEnabled; }
    static void setPageAtRuleSupportEnabled(bool isEnabled) { shared().m_pageAtRuleSupportEnabled = isEnabled; }

#if HAVE(NSURLSESSION_WEBSOCKET)
    static bool isNSURLSessionWebSocketEnabled() { return shared().m_isNSURLSessionWebSocketEnabled; }
    static void setIsNSURLSessionWebSocketEnabled(bool isEnabled) { shared().m_isNSURLSessionWebSocketEnabled = isEnabled; }
#endif

    static bool secureContextChecksEnabled() { return shared().m_secureContextChecksEnabled; }
    static void setSecureContextChecksEnabled(bool isEnabled) { shared().m_secureContextChecksEnabled = isEnabled; }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    static void setIsAccessibilityIsolatedTreeEnabled(bool isEnabled) { shared().m_accessibilityIsolatedTree = isEnabled; }
    static bool isAccessibilityIsolatedTreeEnabled() { return shared().m_accessibilityIsolatedTree; }
#endif

    static void setArePDFImagesEnabled(bool isEnabled) { shared().m_arePDFImagesEnabled = isEnabled; }
    static bool arePDFImagesEnabled() { return shared().m_arePDFImagesEnabled; }

#if HAVE(INCREMENTAL_PDF_APIS)
    static void setIncrementalPDFLoadingEnabled(bool isEnabled) { shared().m_incrementalPDFLoadingEnabled = isEnabled; }
    static bool incrementalPDFLoadingEnabled() { return shared().m_incrementalPDFLoadingEnabled; }
#endif
    
#if ENABLE(WEBM_FORMAT_READER)
    static void setWebMFormatReaderEnabled(bool isEnabled) { shared().m_webMFormatReaderEnabled = isEnabled; }
    static bool webMFormatReaderEnabled() { return shared().m_webMFormatReaderEnabled; }
#endif

#if ENABLE(MEDIA_SOURCE)
    static void setWebMParserEnabled(bool isEnabled) { shared().m_webMParserEnabled = isEnabled; }
    static bool webMParserEnabled() { return shared().m_webMParserEnabled; }
#endif

#if HAVE(CELESTIAL)
    static void setDisableMediaExperiencePIDInheritance(bool isDisabled) { shared().m_disableMediaExperiencePIDInheritance = isDisabled; }
    static bool disableMediaExperiencePIDInheritance() { return shared().m_disableMediaExperiencePIDInheritance; }
#endif

#if ENABLE(VORBIS)
    WEBCORE_EXPORT static void setVorbisDecoderEnabled(bool isEnabled);
    static bool vorbisDecoderEnabled() { return shared().m_vorbisDecoderEnabled; }
#endif

#if ENABLE(OPUS)
    WEBCORE_EXPORT static void setOpusDecoderEnabled(bool isEnabled);
    static bool opusDecoderEnabled() { return shared().m_opusDecoderEnabled; }
#endif

#if ENABLE(MEDIA_SOURCE) && (HAVE(AVSAMPLEBUFFERVIDEOOUTPUT) || USE(GSTREAMER))
    WEBCORE_EXPORT static void setMediaSourceInlinePaintingEnabled(bool);
    static bool mediaSourceInlinePaintingEnabled() { return shared().m_mediaSourceInlinePaintingEnabled; }
#endif

#if HAVE(AVCONTENTKEYSPECIFIER)
    WEBCORE_EXPORT static void setSampleBufferContentKeySessionSupportEnabled(bool);
    static bool sampleBufferContentKeySessionSupportEnabled() { return shared().m_sampleBufferContentKeySessionSupportEnabled; }
#endif

#if ENABLE(BUILT_IN_NOTIFICATIONS)
    static void setBuiltInNotificationsEnabled(bool isEnabled) { shared().m_builtInNotificationsEnabled = isEnabled; }
    static bool builtInNotificationsEnabled() { return shared().m_builtInNotificationsEnabled; }
#endif

#if ENABLE(NOTIFICATION_EVENT)
    static void setNotificationEventEnabled(bool isEnabled) { shared().m_notificationEventEnabled = isEnabled; }
    static bool notificationEventEnabled() { return shared().m_notificationEventEnabled; }
#endif

#if ENABLE(MODEL_ELEMENT)
    static void setModelDocumentEnabled(bool isEnabled) { shared().m_modelDocumentEnabled = isEnabled; }
    static bool modelDocumentEnabled() { return shared().m_modelDocumentEnabled; }
#endif


private:
    WEBCORE_EXPORT static DeprecatedGlobalSettings& shared();
    DeprecatedGlobalSettings();
    ~DeprecatedGlobalSettings();

#if USE(AVFOUNDATION)
    bool m_AVFoundationEnabled;
#endif

#if USE(GSTREAMER)
    bool m_GStreamerEnabled;
#endif

    bool m_mockScrollbarsEnabled;
    bool m_usesOverlayScrollbars;

#if PLATFORM(WIN)
    bool m_shouldUseHighResolutionTimers;
#endif
#if PLATFORM(IOS_FAMILY)
    bool m_networkDataUsageTrackingEnabled;
    bool m_shouldOptOutOfNetworkStateObservation;
    bool m_disableScreenSizeOverride;
#endif
    bool m_manageAudioSession;

    bool m_lowPowerVideoAudioBufferSizeEnabled;
    bool m_trackingPreventionEnabled;
    bool m_allowsAnySSLCertificate;

    String m_networkInterfaceName;

    bool m_isPaintTimingEnabled { false };
    bool m_isMenuItemElementEnabled { false };
    bool m_isDirectoryUploadEnabled { false };
    bool m_isCustomPasteboardDataEnabled { false };
#if ENABLE(OFFSCREEN_CANVAS)
    bool m_isOffscreenCanvasEnabled { false };
#endif
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    bool m_isOffscreenCanvasInWorkersEnabled { false };
#endif
    bool m_isCacheAPIEnabled { false };
    bool m_isWebSocketEnabled { true };
    bool m_fetchAPIKeepAliveEnabled { false };
    bool m_accessibilityObjectModelEnabled { false };
    bool m_itpDebugMode { false };
    bool m_isRestrictedHTTPResponseAccess { true };
    bool m_isServerTimingEnabled { false };
    bool m_attrStyleEnabled { false };
    bool m_webAPIStatisticsEnabled { false };
    bool m_syntheticEditingCommandsEnabled { true };
    bool m_webSQLEnabled { false };
    bool m_keygenElementEnabled { false };
    bool m_pageAtRuleSupportEnabled { false };
    bool m_highlightAPIEnabled { false };

    bool m_layoutFormattingContextEnabled { false };
    bool m_inlineFormattingContextIntegrationEnabled { true };

#if ENABLE(CSS_PAINTING_API)
    bool m_CSSPaintingAPIEnabled { false };
#endif

    bool m_CSSTypedOMEnabled { false };

#if ENABLE(ATTACHMENT_ELEMENT)
    bool m_isAttachmentElementEnabled { false };
#endif

    bool m_userGesturePromisePropagationEnabled { true };

#if ENABLE(WEB_RTC)
    bool m_isWebRTCDTMFEnabled { true };
    bool m_isWebRTCH264SimulcastEnabled { true };
    bool m_isWebRTCMDNSICECandidatesEnabled { false };
    bool m_isWebRTCH264LowLatencyEncoderEnabled { false };
    bool m_isWebRTCPlatformTCPSocketsEnabled { false };
    bool m_isWebRTCPlatformUDPSocketsEnabled { false };
#endif
    bool m_isWebRTCAudioLatencyAdaptationEnabled { true };

#if ENABLE(DATALIST_ELEMENT)
    bool m_isDataListElementEnabled { false };
#endif

    bool m_isReadableByteStreamAPIEnabled { false };

    bool m_CSSLogicalEnabled { false };

    // False by default until https://bugs.webkit.org/show_bug.cgi?id=211351 /
    // https://github.com/w3c/csswg-drafts/issues/3257 have been sorted out.
    bool m_lineHeightUnitsEnabled { false };

    bool m_privateClickMeasurementDebugModeEnabled { false };
#if HAVE(RSA_BSSA)
    bool m_privateClickMeasurementFraudPreventionEnabled { true };
#else
    bool m_privateClickMeasurementFraudPreventionEnabled { false };
#endif

#if ENABLE(TOUCH_EVENTS)
    bool m_mouseEventsSimulationEnabled { false };
    std::optional<bool> m_touchEventsEnabled;
#endif

#if HAVE(NSURLSESSION_WEBSOCKET)
    bool m_isNSURLSessionWebSocketEnabled { false };
#endif

    bool m_secureContextChecksEnabled { true };

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    bool m_accessibilityIsolatedTree { false };
#endif

    bool m_arePDFImagesEnabled { true };

#if HAVE(INCREMENTAL_PDF_APIS)
    bool m_incrementalPDFLoadingEnabled { false };
#endif

#if ENABLE(WEBM_FORMAT_READER)
    bool m_webMFormatReaderEnabled { false };
#endif

#if ENABLE(MEDIA_SOURCE)
    bool m_webMParserEnabled { false };
#endif

#if HAVE(CELESTIAL)
    bool m_disableMediaExperiencePIDInheritance { false };
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

#if ENABLE(NOTIFICATION_EVENT)
    bool m_notificationEventEnabled { true };
#endif

#if ENABLE(MODEL_ELEMENT)
    bool m_modelDocumentEnabled { false };
#endif

    friend class NeverDestroyed<DeprecatedGlobalSettings>;
};

} // namespace WebCore
