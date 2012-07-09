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

#ifndef WebRuntimeFeatures_h
#define WebRuntimeFeatures_h

#define HAS_WEBAUDIO_RUNTIMEFEATURES 1

#include "platform/WebCommon.h"

namespace WebKit {

// This class is used to enable runtime features of WebKit.  It is unspecified
// whether a feature is enabled by default.  In the future, a feature may be
// promoted from disabled by default to enabled by default once it reaches a
// certain level of maturity.
class WebRuntimeFeatures {
public:
    WEBKIT_EXPORT static void enableDatabase(bool);
    WEBKIT_EXPORT static bool isDatabaseEnabled();

    WEBKIT_EXPORT static void enableLocalStorage(bool);
    WEBKIT_EXPORT static bool isLocalStorageEnabled();

    WEBKIT_EXPORT static void enableSessionStorage(bool);
    WEBKIT_EXPORT static bool isSessionStorageEnabled();

    WEBKIT_EXPORT static void enableMediaPlayer(bool);
    WEBKIT_EXPORT static bool isMediaPlayerEnabled();

    WEBKIT_EXPORT static void enableSockets(bool);
    WEBKIT_EXPORT static bool isSocketsEnabled();

    WEBKIT_EXPORT static void enableNotifications(bool);
    WEBKIT_EXPORT static bool isNotificationsEnabled();

    WEBKIT_EXPORT static void enableApplicationCache(bool);
    WEBKIT_EXPORT static bool isApplicationCacheEnabled();
    
    WEBKIT_EXPORT static void enableDataTransferItems(bool);
    WEBKIT_EXPORT static bool isDataTransferItemsEnabled();

    WEBKIT_EXPORT static void enableGeolocation(bool);
    WEBKIT_EXPORT static bool isGeolocationEnabled();

    WEBKIT_EXPORT static void enableIndexedDatabase(bool);
    WEBKIT_EXPORT static bool isIndexedDatabaseEnabled();

    WEBKIT_EXPORT static void enableWebAudio(bool);
    WEBKIT_EXPORT static bool isWebAudioEnabled();

    WEBKIT_EXPORT static void enablePushState(bool);
    WEBKIT_EXPORT static bool isPushStateEnabled(bool);

    WEBKIT_EXPORT static void enableTouch(bool);
    WEBKIT_EXPORT static bool isTouchEnabled();

    WEBKIT_EXPORT static void enableDeviceMotion(bool);
    WEBKIT_EXPORT static bool isDeviceMotionEnabled();

    WEBKIT_EXPORT static void enableDeviceOrientation(bool);
    WEBKIT_EXPORT static bool isDeviceOrientationEnabled();

    WEBKIT_EXPORT static void enableSpeechInput(bool);
    WEBKIT_EXPORT static bool isSpeechInputEnabled();

    WEBKIT_EXPORT static void enableScriptedSpeech(bool);
    WEBKIT_EXPORT static bool isScriptedSpeechEnabled();

    WEBKIT_EXPORT static void enableXHRResponseBlob(bool);
    WEBKIT_EXPORT static bool isXHRResponseBlobEnabled();

    WEBKIT_EXPORT static void enableFileSystem(bool);
    WEBKIT_EXPORT static bool isFileSystemEnabled();
    
    WEBKIT_EXPORT static void enableJavaScriptI18NAPI(bool);
    WEBKIT_EXPORT static bool isJavaScriptI18NAPIEnabled();

    WEBKIT_EXPORT static void enableQuota(bool);
    WEBKIT_EXPORT static bool isQuotaEnabled();

    WEBKIT_EXPORT static void enableMediaStream(bool);
    WEBKIT_EXPORT static bool isMediaStreamEnabled();

    WEBKIT_EXPORT static void enablePeerConnection(bool);
    WEBKIT_EXPORT static bool isPeerConnectionEnabled();

    WEBKIT_EXPORT static void enableFullScreenAPI(bool);
    WEBKIT_EXPORT static bool isFullScreenAPIEnabled();

    WEBKIT_EXPORT static void enablePointerLock(bool);
    WEBKIT_EXPORT static bool isPointerLockEnabled();

    WEBKIT_EXPORT static void enableMediaSource(bool);
    WEBKIT_EXPORT static bool isMediaSourceEnabled();

    WEBKIT_EXPORT static void enableEncryptedMedia(bool);
    WEBKIT_EXPORT static bool isEncryptedMediaEnabled();

    WEBKIT_EXPORT static void enableVideoTrack(bool);
    WEBKIT_EXPORT static bool isVideoTrackEnabled();

    WEBKIT_EXPORT static void enableGamepad(bool);
    WEBKIT_EXPORT static bool isGamepadEnabled();

    WEBKIT_EXPORT static void enableShadowDOM(bool);
    WEBKIT_EXPORT static bool isShadowDOMEnabled();

    WEBKIT_EXPORT static void enableStyleScoped(bool);
    WEBKIT_EXPORT static bool isStyleScopedEnabled();

    WEBKIT_EXPORT static void enableInputTypeDate(bool);
    WEBKIT_EXPORT static bool isInputTypeDateEnabled();

private:
    WebRuntimeFeatures();
};

} // namespace WebKit

#endif
