/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

// A class that stores static enablers for all experimental features. Note that
// the method names must line up with the JavaScript method they enable for code
// generation to work properly.

class RuntimeEnabledFeatures {
    WTF_MAKE_NONCOPYABLE(RuntimeEnabledFeatures);
public:
    void setBlankAnchorTargetImpliesNoOpenerEnabled(bool isEnabled) { m_blankAnchorTargetImpliesNoOpenerEnabled = isEnabled; }
    bool blankAnchorTargetImpliesNoOpenerEnabled() const { return m_blankAnchorTargetImpliesNoOpenerEnabled; }

    void setDisplayContentsEnabled(bool isEnabled) { m_isDisplayContentsEnabled = isEnabled; }
    bool displayContentsEnabled() const { return m_isDisplayContentsEnabled; }

    void setLinkPreloadEnabled(bool isEnabled) { m_isLinkPreloadEnabled = isEnabled; }
    bool linkPreloadEnabled() const { return m_isLinkPreloadEnabled; }

    void setLinkPrefetchEnabled(bool isEnabled) { m_isLinkPrefetchEnabled = isEnabled; }
    bool linkPrefetchEnabled() const { return m_isLinkPrefetchEnabled; }

    void setMediaPreloadingEnabled(bool isEnabled) { m_isMediaPreloadingEnabled = isEnabled; }
    bool mediaPreloadingEnabled() const { return m_isMediaPreloadingEnabled; }

    void setResourceTimingEnabled(bool isEnabled) { m_isResourceTimingEnabled = isEnabled; }
    bool resourceTimingEnabled() const { return m_isResourceTimingEnabled; }

    void setUserTimingEnabled(bool isEnabled) { m_isUserTimingEnabled = isEnabled; }
    bool userTimingEnabled() const { return m_isUserTimingEnabled; }

    bool performanceTimelineEnabled() const { return resourceTimingEnabled() || userTimingEnabled(); }

    void setShadowDOMEnabled(bool isEnabled) { m_isShadowDOMEnabled = isEnabled; }
    bool shadowDOMEnabled() const { return m_isShadowDOMEnabled; }

    void setInputEventsEnabled(bool isEnabled) { m_inputEventsEnabled = isEnabled; }
    bool inputEventsEnabled() const { return m_inputEventsEnabled; }

    void setInteractiveFormValidationEnabled(bool isEnabled) { m_isInteractiveFormValidationEnabled = isEnabled; }
    bool interactiveFormValidationEnabled() const { return m_isInteractiveFormValidationEnabled; }

    void setCustomElementsEnabled(bool areEnabled) { m_areCustomElementsEnabled = areEnabled; }
    bool customElementsEnabled() const { return m_areCustomElementsEnabled; }

    void setMenuItemElementEnabled(bool isEnabled) { m_isMenuItemElementEnabled = isEnabled; }
    bool menuItemElementEnabled() const { return m_isMenuItemElementEnabled; }
    
    void setDirectoryUploadEnabled(bool isEnabled) { m_isDirectoryUploadEnabled = isEnabled; }
    bool directoryUploadEnabled() const { return m_isDirectoryUploadEnabled; }

#if ENABLE(DARK_MODE_CSS)
    void setDarkModeCSSEnabled(bool isEnabled) { m_isDarkModeCSSEnabled = isEnabled; }
    bool darkModeCSSEnabled() const { return m_isDarkModeCSSEnabled; }
#endif

    void setDataTransferItemsEnabled(bool areEnabled) { m_areDataTransferItemsEnabled = areEnabled; }
    bool dataTransferItemsEnabled() const { return m_areDataTransferItemsEnabled; }

    void setCustomPasteboardDataEnabled(bool isEnabled) { m_isCustomPasteboardDataEnabled = isEnabled; }
    bool customPasteboardDataEnabled() const { return m_isCustomPasteboardDataEnabled; }
    
    void setWebShareEnabled(bool isEnabled) { m_isWebShareEnabled = isEnabled; }
    bool webShareEnabled() const { return m_isWebShareEnabled; }

    void setModernMediaControlsEnabled(bool areEnabled) { m_areModernMediaControlsEnabled = areEnabled; }
    bool modernMediaControlsEnabled() const { return m_areModernMediaControlsEnabled; }

    void setWebAuthenticationEnabled(bool isEnabled) { m_isWebAuthenticationEnabled = isEnabled; }
    bool webAuthenticationEnabled() const { return m_isWebAuthenticationEnabled; }

    void setWebAuthenticationLocalAuthenticatorEnabled(bool isEnabled) { m_isWebAuthenticationLocalAuthenticatorEnabled = isEnabled; }
    bool webAuthenticationLocalAuthenticatorEnabled() const { return m_isWebAuthenticationLocalAuthenticatorEnabled; }

    void setIsSecureContextAttributeEnabled(bool isEnabled) { m_isSecureContextAttributeEnabled = isEnabled; }
    bool isSecureContextAttributeEnabled() const { return m_isSecureContextAttributeEnabled; }

    void setWebAnimationsEnabled(bool areEnabled) { m_areWebAnimationsEnabled = areEnabled; }
    bool webAnimationsEnabled() const { return m_areWebAnimationsEnabled; }

    void setWebAnimationsCSSIntegrationEnabled(bool isEnabled) { m_isWebAnimationsCSSIntegrationEnabled = isEnabled; }
    bool webAnimationsCSSIntegrationEnabled() const { return m_areWebAnimationsEnabled && m_isWebAnimationsCSSIntegrationEnabled; }

    void setImageBitmapOffscreenCanvasEnabled(bool isEnabled) { m_isImageBitmapOffscreenCanvasEnabled = isEnabled; }
    bool imageBitmapOffscreenCanvasEnabled() const { return m_isImageBitmapOffscreenCanvasEnabled; }

    void setCacheAPIEnabled(bool isEnabled) { m_isCacheAPIEnabled = isEnabled; }
    bool cacheAPIEnabled() const { return m_isCacheAPIEnabled; }

    void setFetchAPIEnabled(bool isEnabled) { m_isFetchAPIEnabled = isEnabled; }
    bool fetchAPIEnabled() const { return m_isFetchAPIEnabled; }

    void setWebSocketEnabled(bool isEnabled) { m_isWebSocketEnabled = isEnabled; }
    bool webSocketEnabled() const { return m_isWebSocketEnabled; }

    bool fetchAPIKeepAliveEnabled() const { return m_fetchAPIKeepAliveEnabled; }
    void setFetchAPIKeepAliveEnabled(bool isEnabled) { m_fetchAPIKeepAliveEnabled = isEnabled; }

    bool spectreGadgetsEnabled() const;

    void setInspectorAdditionsEnabled(bool isEnabled) { m_inspectorAdditionsEnabled = isEnabled; }
    bool inspectorAdditionsEnabled() const { return m_inspectorAdditionsEnabled; }

    void setWebVREnabled(bool isEnabled) { m_webVREnabled = isEnabled; }
    bool webVREnabled() const { return m_webVREnabled; }

    void setAccessibilityObjectModelEnabled(bool isEnabled) { m_accessibilityObjectModelEnabled = isEnabled; }
    bool accessibilityObjectModelEnabled() const { return m_accessibilityObjectModelEnabled; }

    void setAriaReflectionEnabled(bool isEnabled) { m_ariaReflectionEnabled = isEnabled; }
    bool ariaReflectionEnabled() const { return m_ariaReflectionEnabled; }

    void setItpDebugModeEnabled(bool isEnabled) { m_itpDebugMode = isEnabled; }
    bool itpDebugModeEnabled() const { return m_itpDebugMode; }

    void setRestrictedHTTPResponseAccess(bool isEnabled) { m_isRestrictedHTTPResponseAccess = isEnabled; }
    bool restrictedHTTPResponseAccess() const { return m_isRestrictedHTTPResponseAccess; }

    void setCrossOriginResourcePolicyEnabled(bool isEnabled) { m_crossOriginResourcePolicyEnabled = isEnabled; }
    bool crossOriginResourcePolicyEnabled() const { return m_crossOriginResourcePolicyEnabled; }

    void setWebGLCompressedTextureASTCSupportEnabled(bool isEnabled) { m_isWebGLCompressedTextureASTCSupportEnabled = isEnabled; }
    bool webGLCompressedTextureASTCSupportEnabled() const { return m_isWebGLCompressedTextureASTCSupportEnabled; }

    void setStorageAccessPromptsEnabled(bool isEnabled)  { m_promptForStorageAccessAPIEnabled = isEnabled; }
    bool storageAccessPromptsEnabled() const { return m_promptForStorageAccessAPIEnabled; }

    void setServerTimingEnabled(bool isEnabled) { m_isServerTimingEnabled = isEnabled; }
    bool serverTimingEnabled() const { return m_isServerTimingEnabled; }

    void setExperimentalPlugInSandboxProfilesEnabled(bool isEnabled) { m_experimentalPlugInSandboxProfilesEnabled = isEnabled; }
    bool experimentalPlugInSandboxProfilesEnabled() const { return m_experimentalPlugInSandboxProfilesEnabled; }

    void setDisabledAdaptationsMetaTagEnabled(bool isEnabled) { m_disabledAdaptationsMetaTagEnabled = isEnabled; }
    bool disabledAdaptationsMetaTagEnabled() const { return m_disabledAdaptationsMetaTagEnabled; }

    void setAttrStyleEnabled(bool isEnabled) { m_attrStyleEnabled = isEnabled; }
    bool attrStyleEnabled() const { return m_attrStyleEnabled; }

    void setWebAPIStatisticsEnabled(bool isEnabled) { m_webAPIStatisticsEnabled = isEnabled; }
    bool webAPIStatisticsEnabled() const { return m_webAPIStatisticsEnabled; }

    void setCSSCustomPropertiesAndValuesEnabled(bool isEnabled) { m_CSSCustomPropertiesAndValuesEnabled = isEnabled; }
    bool cssCustomPropertiesAndValuesEnabled() const { return m_CSSCustomPropertiesAndValuesEnabled; }

    void setPointerEventsEnabled(bool isEnabled) { m_pointerEventsEnabled = isEnabled; }
    bool pointerEventsEnabled() const { return m_pointerEventsEnabled; }

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    void setLayoutFormattingContextEnabled(bool isEnabled) { m_layoutFormattingContextEnabled = isEnabled; }
    bool layoutFormattingContextEnabled() const { return m_layoutFormattingContextEnabled; }
#endif

#if ENABLE(CSS_PAINTING_API)
    void setCSSPaintingAPIEnabled(bool isEnabled) { m_CSSPaintingAPIEnabled = isEnabled; }
    bool cssPaintingAPIEnabled() const { return m_CSSPaintingAPIEnabled; }
#endif

#if ENABLE(CSS_TYPED_OM)
    void setCSSTypedOMEnabled(bool isEnabled) { m_CSSTypedOMEnabled = isEnabled; }
    bool cssTypedOMEnabled() const { return m_CSSTypedOMEnabled; }
#endif

    void setWebSQLDisabled(bool isDisabled) { m_webSQLEnabled = !isDisabled; }
    bool webSQLEnabled() const { return m_webSQLEnabled; }

#if ENABLE(ATTACHMENT_ELEMENT)
    void setAttachmentElementEnabled(bool areEnabled) { m_isAttachmentElementEnabled = areEnabled; }
    bool attachmentElementEnabled() const { return m_isAttachmentElementEnabled; }
#endif

#if ENABLE(INDEXED_DATABASE_IN_WORKERS)
    void setIndexedDBWorkersEnabled(bool isEnabled) { m_isIndexedDBWorkersEnabled = isEnabled; }
    bool indexedDBWorkersEnabled() const { return m_isIndexedDBWorkersEnabled; }
#endif

#if ENABLE(MEDIA_STREAM)
    bool mediaRecorderEnabled() const { return m_isMediaRecorderEnabled; }
    void setMediaRecorderEnabled(bool isEnabled) { m_isMediaRecorderEnabled = isEnabled; }
    bool mediaDevicesEnabled() const { return m_isMediaDevicesEnabled; }
    void setMediaDevicesEnabled(bool isEnabled) { m_isMediaDevicesEnabled = isEnabled; }
    bool mediaStreamEnabled() const { return m_isMediaStreamEnabled; }
    void setMediaStreamEnabled(bool isEnabled) { m_isMediaStreamEnabled = isEnabled; }
    bool screenCaptureEnabled() const { return m_isScreenCaptureEnabled; }
    void setScreenCaptureEnabled(bool isEnabled) { m_isScreenCaptureEnabled = isEnabled; }
#endif

#if ENABLE(WEB_RTC)
    bool webRTCVP8CodecEnabled() const { return m_isWebRTCVP8CodecEnabled; }
    void setWebRTCVP8CodecEnabled(bool isEnabled) { m_isWebRTCVP8CodecEnabled = isEnabled; }
    bool webRTCUnifiedPlanEnabled() const { return m_isWebRTCUnifiedPlanEnabled; }
    void setWebRTCUnifiedPlanEnabled(bool isEnabled) { m_isWebRTCUnifiedPlanEnabled = isEnabled; }
    bool peerConnectionEnabled() const { return m_isPeerConnectionEnabled; }
    void setPeerConnectionEnabled(bool isEnabled) { m_isPeerConnectionEnabled = isEnabled; }
    bool webRTCMDNSICECandidatesEnabled() const { return m_isWebRTCMDNSICECandidatesEnabled; }
    void setWebRTCMDNSICECandidatesEnabled(bool isEnabled) { m_isWebRTCMDNSICECandidatesEnabled = isEnabled; }
    bool webRTCH264SimulcastEnabled() const { return m_isWebRTCH264SimulcastEnabled; }
    void setWebRTCH264SimulcastEnabled(bool isEnabled) { m_isWebRTCH264SimulcastEnabled = isEnabled; }
#endif

#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    void setLegacyCSSVendorPrefixesEnabled(bool isEnabled) { m_isLegacyCSSVendorPrefixesEnabled = isEnabled; }
    bool legacyCSSVendorPrefixesEnabled() const { return m_isLegacyCSSVendorPrefixesEnabled; }
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    bool inputTypeColorEnabled() const { return m_isInputTypeColorEnabled; }
    void setInputTypeColorEnabled(bool isEnabled) { m_isInputTypeColorEnabled = isEnabled; }
#endif

#if ENABLE(DATALIST_ELEMENT)
    bool dataListElementEnabled() const { return m_isDataListElementEnabled; }
    void setDataListElementEnabled(bool isEnabled) { m_isDataListElementEnabled = isEnabled; }
#endif

#if ENABLE(INPUT_TYPE_DATE)
    bool inputTypeDateEnabled() const { return m_isInputTypeDateEnabled; }
    void setInputTypeDateEnabled(bool isEnabled) { m_isInputTypeDateEnabled = isEnabled; }
#endif

#if ENABLE(INPUT_TYPE_DATETIME_INCOMPLETE)
    bool inputTypeDateTimeEnabled() const { return m_isInputTypeDateTimeEnabled; }
    void setInputTypeDateTimeEnabled(bool isEnabled) { m_isInputTypeDateTimeEnabled = isEnabled; }
#endif

#if ENABLE(INPUT_TYPE_DATETIMELOCAL)
    bool inputTypeDateTimeLocalEnabled() const { return m_isInputTypeDateTimeLocalEnabled; }
    void setInputTypeDateTimeLocalEnabled(bool isEnabled) { m_isInputTypeDateTimeLocalEnabled = isEnabled; }
#endif

#if ENABLE(INPUT_TYPE_MONTH)
    bool inputTypeMonthEnabled() const { return m_isInputTypeMonthEnabled; }
    void setInputTypeMonthEnabled(bool isEnabled) { m_isInputTypeMonthEnabled = isEnabled; }
#endif

#if ENABLE(INPUT_TYPE_TIME)
    bool inputTypeTimeEnabled() const { return m_isInputTypeTimeEnabled; }
    void setInputTypeTimeEnabled(bool isEnabled) { m_isInputTypeTimeEnabled = isEnabled; }
#endif

#if ENABLE(INPUT_TYPE_WEEK)
    bool inputTypeWeekEnabled() const { return m_isInputTypeWeekEnabled; }
    void setInputTypeWeekEnabled(bool isEnabled) { m_isInputTypeWeekEnabled = isEnabled; }
#endif

#if ENABLE(GAMEPAD)
    void setGamepadsEnabled(bool areEnabled) { m_areGamepadsEnabled = areEnabled; }
    bool gamepadsEnabled() const { return m_areGamepadsEnabled; }
#endif

#if ENABLE(WEBGL2)
    void setWebGL2Enabled(bool isEnabled) { m_isWebGL2Enabled = isEnabled; }
    bool webGL2Enabled() const { return m_isWebGL2Enabled; }
#endif
    
#if ENABLE(WEBGPU)
    void setWebGPUEnabled(bool isEnabled) { m_isWebGPUEnabled = isEnabled; }
    bool webGPUEnabled() const { return m_isWebGPUEnabled; }
#endif

#if ENABLE(WEBMETAL)
    void setWebMetalEnabled(bool isEnabled) { m_isWebMetalEnabled = isEnabled; }
    bool webMetalEnabled() const { return m_isWebMetalEnabled; }
#endif

#if ENABLE(STREAMS_API)
    void setReadableByteStreamAPIEnabled(bool isEnabled) { m_isReadableByteStreamAPIEnabled = isEnabled; }
    bool readableByteStreamAPIEnabled() const { return m_isReadableByteStreamAPIEnabled; }
    void setWritableStreamAPIEnabled(bool isEnabled) { m_isWritableStreamAPIEnabled = isEnabled; }
    bool writableStreamAPIEnabled() const { return m_isWritableStreamAPIEnabled; }
#endif
    
#if ENABLE(DOWNLOAD_ATTRIBUTE)
    void setDownloadAttributeEnabled(bool isEnabled) { m_isDownloadAttributeEnabled = isEnabled; }
    bool downloadAttributeEnabled() const { return m_isDownloadAttributeEnabled; }
#endif

#if ENABLE(INTERSECTION_OBSERVER)
    void setIntersectionObserverEnabled(bool isEnabled) { m_intersectionObserverEnabled = isEnabled; }
    bool intersectionObserverEnabled() const { return m_intersectionObserverEnabled; }
#endif

    void setUndoManagerAPIEnabled(bool isEnabled) { m_undoManagerAPIEnabled = isEnabled; }
    bool undoManagerAPIEnabled() const { return m_undoManagerAPIEnabled; }

#if ENABLE(ENCRYPTED_MEDIA)
    void setEncryptedMediaAPIEnabled(bool isEnabled) { m_encryptedMediaAPIEnabled = isEnabled; }
    bool encryptedMediaAPIEnabled() const { return m_encryptedMediaAPIEnabled; }
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void setLegacyEncryptedMediaAPIEnabled(bool isEnabled) { m_legacyEncryptedMediaAPIEnabled = isEnabled; }
    bool legacyEncryptedMediaAPIEnabled() const { return m_legacyEncryptedMediaAPIEnabled; }
#endif

#if ENABLE(SERVICE_WORKER)
    bool serviceWorkerEnabled() const { return m_serviceWorkerEnabled; }
    void setServiceWorkerEnabled(bool isEnabled) { m_serviceWorkerEnabled = isEnabled; }
#endif

#if USE(SYSTEM_PREVIEW)
    void setSystemPreviewEnabled(bool isEnabled) { m_systemPreviewEnabled = isEnabled; }
    bool systemPreviewEnabled() const { return m_systemPreviewEnabled; }
#endif

    void setCSSLogicalEnabled(bool isEnabled) { m_CSSLogicalEnabled = isEnabled; }
    bool cssLogicalEnabled() const { return m_CSSLogicalEnabled; }

    bool adClickAttributionEnabled() const { return m_adClickAttributionEnabled; }
    void setAdClickAttributionEnabled(bool isEnabled) { m_adClickAttributionEnabled = isEnabled; }

#if ENABLE(TOUCH_EVENTS)
    bool mouseEventsSimulationEnabled() const { return m_mouseEventsSimulationEnabled; }
    void setMouseEventsSimulationEnabled(bool isEnabled) { m_mouseEventsSimulationEnabled = isEnabled; }

    bool mousemoveEventHandlingPreventsDefaultEnabled() const { return m_mousemoveEventHandlingPreventsDefaultEnabled; }
    void setMousemoveEventHandlingPreventsDefaultEnabled(bool isEnabled) { m_mousemoveEventHandlingPreventsDefaultEnabled = isEnabled; }
#endif
    
    WEBCORE_EXPORT static RuntimeEnabledFeatures& sharedFeatures();

private:
    // Never instantiate.
    RuntimeEnabledFeatures();

    bool m_blankAnchorTargetImpliesNoOpenerEnabled { true };
    bool m_areModernMediaControlsEnabled { false };
    bool m_isLinkPreloadEnabled { true };
    bool m_isLinkPrefetchEnabled { false };
    bool m_isMediaPreloadingEnabled { false };
    bool m_isResourceTimingEnabled { false };
    bool m_isUserTimingEnabled { false };
    bool m_isInteractiveFormValidationEnabled { false };
    bool m_isWebAuthenticationEnabled { false };
    bool m_isWebAuthenticationLocalAuthenticatorEnabled { false };
    bool m_isSecureContextAttributeEnabled { false };
    bool m_isDisplayContentsEnabled { true };
    bool m_isShadowDOMEnabled { true };
    bool m_areCustomElementsEnabled { true };
    bool m_isMenuItemElementEnabled { false };
    bool m_isDirectoryUploadEnabled { false };
    bool m_areDataTransferItemsEnabled { false };
    bool m_isCustomPasteboardDataEnabled { false };
    bool m_isWebShareEnabled { false };
    bool m_inputEventsEnabled { true };
    bool m_areWebAnimationsEnabled { false };
    bool m_isWebAnimationsCSSIntegrationEnabled { false };
    bool m_isImageBitmapOffscreenCanvasEnabled { true };
    bool m_isCacheAPIEnabled { false };
    bool m_isFetchAPIEnabled { true };
    bool m_isWebSocketEnabled { true };
    bool m_fetchAPIKeepAliveEnabled { false };
    bool m_inspectorAdditionsEnabled { false };
    bool m_webVREnabled { false };
    bool m_accessibilityObjectModelEnabled { false };
    bool m_ariaReflectionEnabled { true };
    bool m_itpDebugMode { false };
    bool m_isRestrictedHTTPResponseAccess { true };
    bool m_crossOriginResourcePolicyEnabled { true };
    bool m_isWebGLCompressedTextureASTCSupportEnabled { false };
    bool m_promptForStorageAccessAPIEnabled { false };
    bool m_isServerTimingEnabled { false };
    bool m_experimentalPlugInSandboxProfilesEnabled { false };
    bool m_disabledAdaptationsMetaTagEnabled { false };
    bool m_attrStyleEnabled { false };
    bool m_webAPIStatisticsEnabled { false };
    bool m_CSSCustomPropertiesAndValuesEnabled { false };
    bool m_pointerEventsEnabled { true };
    bool m_webSQLEnabled { true };

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    bool m_layoutFormattingContextEnabled { false };
#endif

#if ENABLE(CSS_PAINTING_API)
    bool m_CSSPaintingAPIEnabled { false };
#endif

#if ENABLE(CSS_TYPED_OM)
    bool m_CSSTypedOMEnabled { false };
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    bool m_isAttachmentElementEnabled { false };
#endif

#if ENABLE(DARK_MODE_CSS)
    bool m_isDarkModeCSSEnabled { true };
#endif

#if ENABLE(INDEXED_DATABASE_IN_WORKERS)
    bool m_isIndexedDBWorkersEnabled { true };
#endif

#if ENABLE(MEDIA_STREAM)
    bool m_isMediaRecorderEnabled { false };
    bool m_isMediaDevicesEnabled { false };
    bool m_isMediaStreamEnabled { true };
    bool m_isScreenCaptureEnabled { false };
#endif

#if ENABLE(WEB_RTC)
    bool m_isWebRTCVP8CodecEnabled { true };
    bool m_isWebRTCUnifiedPlanEnabled { true };
    bool m_isPeerConnectionEnabled { true };
    bool m_isWebRTCMDNSICECandidatesEnabled { false };
    bool m_isWebRTCH264SimulcastEnabled { true };
#endif

#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    bool m_isLegacyCSSVendorPrefixesEnabled { false };
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    bool m_isInputTypeColorEnabled { false };
#endif

#if ENABLE(DATALIST_ELEMENT)
    bool m_isDataListElementEnabled { false };
#endif

#if ENABLE(INPUT_TYPE_DATE)
    bool m_isInputTypeDateEnabled { true };
#endif

#if ENABLE(INPUT_TYPE_DATETIME_INCOMPLETE)
    bool m_isInputTypeDateTimeEnabled { false };
#endif

#if ENABLE(INPUT_TYPE_DATETIMELOCAL)
    bool m_isInputTypeDateTimeLocalEnabled { true };
#endif

#if ENABLE(INPUT_TYPE_MONTH)
    bool m_isInputTypeMonthEnabled { true };
#endif

#if ENABLE(INPUT_TYPE_TIME)
    bool m_isInputTypeTimeEnabled { true };
#endif

#if ENABLE(INPUT_TYPE_WEEK)
    bool m_isInputTypeWeekEnabled { true };
#endif

#if ENABLE(GAMEPAD)
    bool m_areGamepadsEnabled { false };
#endif

#if ENABLE(STREAMS_API)
    bool m_isReadableByteStreamAPIEnabled { false };
    bool m_isWritableStreamAPIEnabled { false };
#endif

#if ENABLE(WEBGL2)
    bool m_isWebGL2Enabled { false };
#endif
    
#if ENABLE(WEBGPU)
    bool m_isWebGPUEnabled { false };
#endif

#if ENABLE(WEBMETAL)
    bool m_isWebMetalEnabled { false };
#endif

#if ENABLE(DOWNLOAD_ATTRIBUTE)
    bool m_isDownloadAttributeEnabled { false };
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    bool m_encryptedMediaAPIEnabled { false };
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    bool m_legacyEncryptedMediaAPIEnabled { false };
#endif

#if ENABLE(INTERSECTION_OBSERVER)
    bool m_intersectionObserverEnabled { false };
#endif

#if ENABLE(SERVICE_WORKER)
    bool m_serviceWorkerEnabled { false };
#endif

#if USE(SYSTEM_PREVIEW)
    bool m_systemPreviewEnabled { false };
#endif

    bool m_undoManagerAPIEnabled { false };

    bool m_CSSLogicalEnabled { false };

    bool m_adClickAttributionEnabled { false };

#if ENABLE(TOUCH_EVENTS)
    bool m_mouseEventsSimulationEnabled { true };
    bool m_mousemoveEventHandlingPreventsDefaultEnabled { false };
#endif

    friend class WTF::NeverDestroyed<RuntimeEnabledFeatures>;
};

} // namespace WebCore
