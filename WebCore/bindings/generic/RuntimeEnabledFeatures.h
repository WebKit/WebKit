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

    static void setGeolocationEnabled(bool isEnabled) { isGeolocationEnabled = isEnabled; }
    static bool geolocationEnabled() { return isGeolocationEnabled; }

    static void setIndexedDBEnabled(bool isEnabled) { isIndexedDBEnabled = isEnabled; }
    static bool indexedDBEnabled() { return isIndexedDBEnabled; }
    static bool iDBCursorEnabled() { return isIndexedDBEnabled; }
    static bool iDBDatabaseEnabled() { return isIndexedDBEnabled; }
    static bool iDBDatabaseErrorEnabled() { return isIndexedDBEnabled; }
    static bool iDBDatabaseExceptionEnabled() { return isIndexedDBEnabled; }
    static bool iDBErrorEventEnabled() { return isIndexedDBEnabled; }
    static bool iDBEventEnabled() { return isIndexedDBEnabled; }
    static bool iDBFactoryEnabled() { return isIndexedDBEnabled; }
    static bool iDBIndexEnabled() { return isIndexedDBEnabled; }
    static bool iDBKeyRangeEnabled() { return isIndexedDBEnabled; }
    static bool iDBObjectStoreEnabled() { return isIndexedDBEnabled; }
    static bool iDBRequestEnabled() { return isIndexedDBEnabled; }
    static bool iDBSuccessEventEnabled() { return isIndexedDBEnabled; }
    static bool iDBTransactionEnabled() { return isIndexedDBEnabled; }

#if ENABLE(VIDEO)
    static bool audioEnabled();
    static bool htmlMediaElementEnabled();
    static bool htmlAudioElementEnabled();
    static bool htmlVideoElementEnabled();
    static bool mediaErrorEnabled();
    static bool timeRangesEnabled();
#endif

#if ENABLE(SHARED_WORKERS)
    static bool sharedWorkerEnabled();
#endif

#if ENABLE(WEB_SOCKETS)
    static bool webSocketEnabled();
#endif

#if ENABLE(DATABASE)
    static bool openDatabaseEnabled();
    static bool openDatabaseSyncEnabled();
#endif

#if ENABLE(3D_CANVAS)
    static void setWebGLEnabled(bool isEnabled) { isWebGLEnabled = isEnabled; }
    static bool arrayBufferEnabled() { return isWebGLEnabled; }
    static bool int8ArrayEnabled() { return isWebGLEnabled; }
    static bool uint8ArrayEnabled() { return isWebGLEnabled; }
    static bool int16ArrayEnabled() { return isWebGLEnabled; }
    static bool uint16ArrayEnabled() { return isWebGLEnabled; }
    static bool int32ArrayEnabled() { return isWebGLEnabled; }
    static bool uint32ArrayEnabled() { return isWebGLEnabled; }
    static bool float32ArrayEnabled() { return isWebGLEnabled; }
    static bool webGLRenderingContextEnabled() { return isWebGLEnabled; }
    static bool webGLArrayBufferEnabled() { return isWebGLEnabled; }
    static bool webGLByteArrayEnabled() { return isWebGLEnabled; }
    static bool webGLUnsignedByteArrayEnabled() { return isWebGLEnabled; }
    static bool webGLShortArrayEnabled() { return isWebGLEnabled; }
    static bool webGLUnsignedShortArrayEnabled() { return isWebGLEnabled; }
    static bool webGLIntArrayEnabled() { return isWebGLEnabled; }
    static bool webGLUnsignedIntArrayEnabled() { return isWebGLEnabled; }
    static bool webGLFloatArrayEnabled() { return isWebGLEnabled; }
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
    static bool speechEnabled() { return isSpeechInputEnabled; }

#if ENABLE(XHR_RESPONSE_BLOB)
    static bool xhrResponseBlobEnabled() { return isXHRResponseBlobEnabled; }
    static void setXHRResponseBlobEnabled(bool isEnabled) { isXHRResponseBlobEnabled = isEnabled; }
    static bool responseBlobEnabled() { return isXHRResponseBlobEnabled; }
    static bool asBlobEnabled()  { return isXHRResponseBlobEnabled; }
#endif

#if ENABLE(FILE_SYSTEM)
    static bool fileSystemEnabled() { return isFileSystemEnabled; }
    static void setFileSystemEnabled(bool isEnabled) { isFileSystemEnabled = isEnabled; }
    static bool requestFileSystemEnabled() { return isFileSystemEnabled; }
#endif

private:
    // Never instantiate.
    RuntimeEnabledFeatures() { }

    static bool isLocalStorageEnabled;
    static bool isSessionStorageEnabled;
    static bool isWebkitNotificationsEnabled;
    static bool isApplicationCacheEnabled;
    static bool isGeolocationEnabled;
    static bool isIndexedDBEnabled;
    static bool isWebGLEnabled;
    static bool isPushStateEnabled;
    static bool isTouchEnabled;
    static bool isDeviceMotionEnabled;
    static bool isDeviceOrientationEnabled;
    static bool isSpeechInputEnabled;
#if ENABLE(XHR_RESPONSE_BLOB)
    static bool isXHRResponseBlobEnabled;
#endif

#if ENABLE(FILE_SYSTEM)
    static bool isFileSystemEnabled;
#endif
};

} // namespace WebCore

#endif // RuntimeEnabledFeatures_h
