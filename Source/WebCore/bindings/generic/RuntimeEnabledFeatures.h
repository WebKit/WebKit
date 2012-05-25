/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef RuntimeEnabledFeatures_h
#define RuntimeEnabledFeatures_h

namespace WebCore {

// A class that stores static enablers for all experimental features. Note that
// the method names must line up with the JavaScript method they enable for code
// generation to work properly.

class RuntimeEnabledFeatures {
public:
    static void setLocalStorageEnabled(bool isEnabled) { isLocalStorageEnabled = isEnabled; }
    static bool localStorageEnabled() { return isLocalStorageEnabled; }

    static void setSessionStorageEnabled(bool isEnabled) { isSessionStorageEnabled = isEnabled; }
    static bool sessionStorageEnabled() { return isSessionStorageEnabled; }

    static void setWebkitNotificationsEnabled(bool isEnabled) { isWebkitNotificationsEnabled = isEnabled; }
    static bool webkitNotificationsEnabled() { return isWebkitNotificationsEnabled; }

    static void setApplicationCacheEnabled(bool isEnabled) { isApplicationCacheEnabled = isEnabled; }
    static bool applicationCacheEnabled() { return isApplicationCacheEnabled; }

    static void setDataTransferItemsEnabled(bool isEnabled) { isDataTransferItemsEnabled = isEnabled; }
    static bool dataTransferItemsEnabled() { return isDataTransferItemsEnabled; }

    static void setGeolocationEnabled(bool isEnabled) { isGeolocationEnabled = isEnabled; }
    static bool geolocationEnabled() { return isGeolocationEnabled; }

    static void setWebkitIndexedDBEnabled(bool isEnabled) { isIndexedDBEnabled = isEnabled; }
    static bool webkitIndexedDBEnabled() { return isIndexedDBEnabled; }
    static bool webkitIDBCursorEnabled() { return isIndexedDBEnabled; }
    static bool webkitIDBDatabaseEnabled() { return isIndexedDBEnabled; }
    static bool webkitIDBDatabaseErrorEnabled() { return isIndexedDBEnabled; }
    static bool webkitIDBDatabaseExceptionEnabled() { return isIndexedDBEnabled; }
    static bool webkitIDBFactoryEnabled() { return isIndexedDBEnabled; }
    static bool webkitIDBIndexEnabled() { return isIndexedDBEnabled; }
    static bool webkitIDBKeyRangeEnabled() { return isIndexedDBEnabled; }
    static bool webkitIDBObjectStoreEnabled() { return isIndexedDBEnabled; }
    static bool webkitIDBRequestEnabled() { return isIndexedDBEnabled; }
    static bool webkitIDBTransactionEnabled() { return isIndexedDBEnabled; }

#if ENABLE(CSS_EXCLUSIONS)
    static void setCSSExclusionsEnabled(bool isEnabled) { isCSSExclusionsEnabled = isEnabled; }
    static bool cssExclusionsEnabled() { return isCSSExclusionsEnabled; }
#else
    static void setCSSExclusionsEnabled(bool) { }
    static bool cssExclusionsEnabled() { return false; }
#endif

#if ENABLE(FULLSCREEN_API)
    // Mozilla version
    static bool webkitFullScreenAPIEnabled() { return isFullScreenAPIEnabled; }
    static void setWebkitFullScreenAPIEnabled(bool isEnabled) { isFullScreenAPIEnabled = isEnabled; }
    static bool webkitRequestFullScreenEnabled() { return isFullScreenAPIEnabled; }
    static bool webkitIsFullScreenEnabled() { return isFullScreenAPIEnabled; }
    static bool webkitFullScreenKeyboardInputAllowedEnabled() { return isFullScreenAPIEnabled; }
    static bool webkitCurrentFullScreenElementEnabled() { return isFullScreenAPIEnabled; }
    static bool webkitCancelFullScreenEnabled() { return isFullScreenAPIEnabled; }

    // W3C version
    static bool webkitFullscreenEnabledEnabled() { return isFullScreenAPIEnabled; }
    static bool webkitFullscreenElementEnabled() { return isFullScreenAPIEnabled; }
    static bool webkitExitFullscreenEnabled() { return isFullScreenAPIEnabled; }
    static bool webkitRequestFullscreenEnabled() { return isFullScreenAPIEnabled; }
#endif

#if ENABLE(POINTER_LOCK)
    static bool webkitPointerLockEnabled() { return isPointerLockEnabled; }
    static void setWebkitPointerLockEnabled(bool isEnabled) { isPointerLockEnabled = isEnabled; }
    static bool webkitPointerEnabled() { return isPointerLockEnabled; }
    static bool webkitMovementXEnabled() { return isPointerLockEnabled; }
    static bool webkitMovementYEnabled() { return isPointerLockEnabled; }
#endif

#if ENABLE(VIDEO)
    static bool audioEnabled();
    static bool htmlMediaElementEnabled();
    static bool htmlAudioElementEnabled();
    static bool htmlVideoElementEnabled();
    static bool htmlSourceElementEnabled();
    static bool mediaControllerEnabled();
    static bool mediaErrorEnabled();
    static bool timeRangesEnabled();
#endif

#if ENABLE(SHARED_WORKERS)
    static bool sharedWorkerEnabled();
#endif

#if ENABLE(WEB_SOCKETS)
    static bool webSocketEnabled();
#endif

#if ENABLE(SQL_DATABASE)
    static bool openDatabaseEnabled();
    static bool openDatabaseSyncEnabled();
#endif

#if ENABLE(WEB_AUDIO)
    static void setWebkitAudioContextEnabled(bool isEnabled) { isWebAudioEnabled = isEnabled; }
    static bool webkitAudioContextEnabled() { return isWebAudioEnabled; }
#endif

    static void setPushStateEnabled(bool isEnabled) { isPushStateEnabled = isEnabled; }
    static bool pushStateEnabled() { return isPushStateEnabled; }
    static bool replaceStateEnabled() { return isPushStateEnabled; }

#if ENABLE(TOUCH_EVENTS)
    static bool touchEnabled() { return isTouchEnabled; }
    static void setTouchEnabled(bool isEnabled) { isTouchEnabled = isEnabled; }
    static bool ontouchstartEnabled() { return isTouchEnabled; }
    static bool ontouchmoveEnabled() { return isTouchEnabled; }
    static bool ontouchendEnabled() { return isTouchEnabled; }
    static bool ontouchcancelEnabled() { return isTouchEnabled; }
    static bool createTouchEnabled() { return isTouchEnabled; }
    static bool createTouchListEnabled() { return isTouchEnabled; }
#endif

    static void setDeviceMotionEnabled(bool isEnabled) { isDeviceMotionEnabled = isEnabled; }
    static bool deviceMotionEnabled() { return isDeviceMotionEnabled; }
    static bool deviceMotionEventEnabled() { return isDeviceMotionEnabled; }
    static bool ondevicemotionEnabled() { return isDeviceMotionEnabled; }
    
    static void setDeviceOrientationEnabled(bool isEnabled) { isDeviceOrientationEnabled = isEnabled; }
    static bool deviceOrientationEnabled() { return isDeviceOrientationEnabled; }
    static bool deviceOrientationEventEnabled() { return isDeviceOrientationEnabled; }
    static bool ondeviceorientationEnabled() { return isDeviceOrientationEnabled; }

    static void setSpeechInputEnabled(bool isEnabled) { isSpeechInputEnabled = isEnabled; }
    static bool speechInputEnabled() { return isSpeechInputEnabled; }
    static bool webkitSpeechEnabled() { return isSpeechInputEnabled; }
    static bool webkitGrammarEnabled() { return isSpeechInputEnabled; }

#if ENABLE(SCRIPTED_SPEECH)
    static void setScriptedSpeechEnabled(bool isEnabled) { isScriptedSpeechEnabled = isEnabled; }
    static bool scriptedSpeechEnabled() { return isScriptedSpeechEnabled; }
    static bool webkitSpeechRecognitionEnabled() { return isScriptedSpeechEnabled; }
    static bool webkitSpeechRecognitionErrorEnabled() { return isScriptedSpeechEnabled; }
    static bool webkitSpeechRecognitionEventEnabled() { return isScriptedSpeechEnabled; }
    static bool webkitSpeechGrammarEnabled() { return isScriptedSpeechEnabled; }
    static bool webkitSpeechGrammarListEnabled() { return isScriptedSpeechEnabled; }
#endif

#if ENABLE(XHR_RESPONSE_BLOB)
    static bool xhrResponseBlobEnabled() { return isXHRResponseBlobEnabled; }
    static void setXHRResponseBlobEnabled(bool isEnabled) { isXHRResponseBlobEnabled = isEnabled; }
    static bool responseBlobEnabled() { return isXHRResponseBlobEnabled; }
    static bool asBlobEnabled()  { return isXHRResponseBlobEnabled; }
#endif

#if ENABLE(FILE_SYSTEM)
    static bool fileSystemEnabled();
    static void setFileSystemEnabled(bool isEnabled) { isFileSystemEnabled = isEnabled; }
#endif

#if ENABLE(JAVASCRIPT_I18N_API)
    static bool javaScriptI18NAPIEnabled();
    static void setJavaScriptI18NAPIEnabled(bool isEnabled) { isJavaScriptI18NAPIEnabled = isEnabled; }
#endif

#if ENABLE(MEDIA_STREAM)
    static bool mediaStreamEnabled() { return isMediaStreamEnabled; }
    static void setMediaStreamEnabled(bool isEnabled) { isMediaStreamEnabled = isEnabled; }
    static bool webkitGetUserMediaEnabled() { return isMediaStreamEnabled; }
    static bool webkitMediaStreamEnabled() { return isMediaStreamEnabled; }

    static bool peerConnectionEnabled() { return isMediaStreamEnabled && isPeerConnectionEnabled; }
    static void setPeerConnectionEnabled(bool isEnabled) { isPeerConnectionEnabled = isEnabled; }
    static bool webkitDeprecatedPeerConnectionEnabled() { return peerConnectionEnabled(); }
    static bool webkitPeerConnection00Enabled() { return peerConnectionEnabled(); }
#endif

#if ENABLE(GAMEPAD)
    static void setWebkitGamepadsEnabled(bool isEnabled) { isGamepadEnabled = isEnabled; }
    static bool webkitGamepadsEnabled() { return isGamepadEnabled; }
#endif

#if ENABLE(QUOTA)
    static bool quotaEnabled() { return isQuotaEnabled; }
    static void setQuotaEnabled(bool isEnabled) { isQuotaEnabled = isEnabled; }
#endif

#if ENABLE(MEDIA_SOURCE)
    static bool mediaSourceEnabled() { return isMediaSourceEnabled; }
    static void setMediaSourceEnabled(bool isEnabled) { isMediaSourceEnabled = isEnabled; }
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    static bool encryptedMediaEnabled() { return isEncryptedMediaEnabled; }
    static void setEncryptedMediaEnabled(bool isEnabled) { isEncryptedMediaEnabled = isEnabled; }
#endif

#if ENABLE(VIDEO_TRACK)
    static bool webkitVideoTrackEnabled() { return isVideoTrackEnabled; }
    static void setWebkitVideoTrackEnabled(bool isEnabled) { isVideoTrackEnabled = isEnabled; }
#endif

#if ENABLE(SHADOW_DOM)
    static bool shadowDOMEnabled() { return isShadowDOMEnabled; }
    static void setShadowDOMEnabled(bool isEnabled) { isShadowDOMEnabled = isEnabled; }
#endif

#if ENABLE(STYLE_SCOPED)
    static bool styleScopedEnabled() { return isStyleScopedEnabled; }
    static void setStyleScopedEnabled(bool isEnabled) { isStyleScopedEnabled = isEnabled; }
#endif

#if ENABLE(INPUT_TYPE_DATE)
    static bool inputTypeDateEnabled() { return isInputTypeDateEnabled; }
    static void setInputTypeDateEnabled(bool isEnabled) { isInputTypeDateEnabled = isEnabled; }
#endif

private:
    // Never instantiate.
    RuntimeEnabledFeatures() { }

    static bool isLocalStorageEnabled;
    static bool isSessionStorageEnabled;
    static bool isWebkitNotificationsEnabled;
    static bool isApplicationCacheEnabled;
    static bool isDataTransferItemsEnabled;
    static bool isGeolocationEnabled;
    static bool isIndexedDBEnabled;
    static bool isWebAudioEnabled;
    static bool isPushStateEnabled;
    static bool isTouchEnabled;
    static bool isDeviceMotionEnabled;
    static bool isDeviceOrientationEnabled;
    static bool isSpeechInputEnabled;
    static bool isCSSExclusionsEnabled;
#if ENABLE(SCRIPTED_SPEECH)
    static bool isScriptedSpeechEnabled;
#endif
#if ENABLE(XHR_RESPONSE_BLOB)
    static bool isXHRResponseBlobEnabled;
#endif

#if ENABLE(FILE_SYSTEM)
    static bool isFileSystemEnabled;
#endif

#if ENABLE(JAVASCRIPT_I18N_API)
    static bool isJavaScriptI18NAPIEnabled;
#endif

#if ENABLE(MEDIA_STREAM)
    static bool isMediaStreamEnabled;
    static bool isPeerConnectionEnabled;
#endif

#if ENABLE(GAMEPAD)
    static bool isGamepadEnabled;
#endif

#if ENABLE(QUOTA)
    static bool isQuotaEnabled;
#endif

#if ENABLE(FULLSCREEN_API)
    static bool isFullScreenAPIEnabled;
#endif

#if ENABLE(POINTER_LOCK)
    static bool isPointerLockEnabled;
#endif

#if ENABLE(MEDIA_SOURCE)
    static bool isMediaSourceEnabled;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    static bool isEncryptedMediaEnabled;
#endif

#if ENABLE(VIDEO_TRACK)
    static bool isVideoTrackEnabled;
#endif

#if ENABLE(SHADOW_DOM)
    static bool isShadowDOMEnabled;
#endif

#if ENABLE(STYLE_SCOPED)
    static bool isStyleScopedEnabled;
#endif

#if ENABLE(INPUT_TYPE_DATE)
    static bool isInputTypeDateEnabled;
#endif
};

} // namespace WebCore

#endif // RuntimeEnabledFeatures_h
