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

#include "config.h"
#include "WebRuntimeFeatures.h"

#include "AbstractDatabase.h"
#include "RuntimeEnabledFeatures.h"
#include "WebMediaPlayerClientImpl.h"
#include "Modules/websockets/WebSocket.h"

#include <wtf/UnusedParam.h>

using namespace WebCore;

namespace WebKit {

void WebRuntimeFeatures::enableDatabase(bool enable)
{
#if ENABLE(SQL_DATABASE)
    AbstractDatabase::setIsAvailable(enable);
#endif
}

bool WebRuntimeFeatures::isDatabaseEnabled()
{
#if ENABLE(SQL_DATABASE)
    return AbstractDatabase::isAvailable();
#else
    return false;
#endif
}

// FIXME: Remove the ability to enable this feature at runtime.
void WebRuntimeFeatures::enableLocalStorage(bool enable)
{
    RuntimeEnabledFeatures::setLocalStorageEnabled(enable);
}

// FIXME: Remove the ability to enable this feature at runtime.
bool WebRuntimeFeatures::isLocalStorageEnabled()
{
    return RuntimeEnabledFeatures::localStorageEnabled();
}

// FIXME: Remove the ability to enable this feature at runtime.
void WebRuntimeFeatures::enableSessionStorage(bool enable)
{
    RuntimeEnabledFeatures::setSessionStorageEnabled(enable);
}

// FIXME: Remove the ability to enable this feature at runtime.
bool WebRuntimeFeatures::isSessionStorageEnabled()
{
    return RuntimeEnabledFeatures::sessionStorageEnabled();
}

void WebRuntimeFeatures::enableMediaPlayer(bool enable)
{
#if ENABLE(VIDEO)
    WebMediaPlayerClientImpl::setIsEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isMediaPlayerEnabled()
{
#if ENABLE(VIDEO)
    return WebMediaPlayerClientImpl::isEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableSockets(bool enable)
{
#if ENABLE(WEB_SOCKETS)
    WebCore::WebSocket::setIsAvailable(enable);
#endif
}

bool WebRuntimeFeatures::isSocketsEnabled()
{
#if ENABLE(WEB_SOCKETS)
    return WebSocket::isAvailable();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableNotifications(bool enable)
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    RuntimeEnabledFeatures::setWebkitNotificationsEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isNotificationsEnabled()
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    return RuntimeEnabledFeatures::webkitNotificationsEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableApplicationCache(bool enable)
{
    RuntimeEnabledFeatures::setApplicationCacheEnabled(enable);
}

bool WebRuntimeFeatures::isApplicationCacheEnabled()
{
    return RuntimeEnabledFeatures::applicationCacheEnabled();
}

void WebRuntimeFeatures::enableDataTransferItems(bool enable)
{
#if ENABLE(DATA_TRANSFER_ITEMS)
    RuntimeEnabledFeatures::setDataTransferItemsEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isDataTransferItemsEnabled()
{
#if ENABLE(DATA_TRANSFER_ITEMS)
    return RuntimeEnabledFeatures::dataTransferItemsEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableGeolocation(bool enable)
{
#if ENABLE(GEOLOCATION)
    RuntimeEnabledFeatures::setGeolocationEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isGeolocationEnabled()
{
#if ENABLE(GEOLOCATION)
    return RuntimeEnabledFeatures::geolocationEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableIndexedDatabase(bool enable)
{
#if ENABLE(INDEXED_DATABASE)
    RuntimeEnabledFeatures::setWebkitIndexedDBEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isIndexedDatabaseEnabled()
{
#if ENABLE(INDEXED_DATABASE)
    return RuntimeEnabledFeatures::webkitIndexedDBEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableWebAudio(bool enable)
{
#if ENABLE(WEB_AUDIO)
    RuntimeEnabledFeatures::setWebkitAudioContextEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isWebAudioEnabled()
{
#if ENABLE(WEB_AUDIO)
    return RuntimeEnabledFeatures::webkitAudioContextEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enablePushState(bool enable)
{
    RuntimeEnabledFeatures::setPushStateEnabled(enable);
}

bool WebRuntimeFeatures::isPushStateEnabled(bool enable)
{
    return RuntimeEnabledFeatures::pushStateEnabled();
}

void WebRuntimeFeatures::enableTouch(bool enable)
{
#if ENABLE(TOUCH_EVENTS)
    RuntimeEnabledFeatures::setTouchEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isTouchEnabled()
{
#if ENABLE(TOUCH_EVENTS)
    return RuntimeEnabledFeatures::touchEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableDeviceMotion(bool enable)
{
    RuntimeEnabledFeatures::setDeviceMotionEnabled(enable);
}

bool WebRuntimeFeatures::isDeviceMotionEnabled()
{
    return RuntimeEnabledFeatures::deviceMotionEnabled();
}

void WebRuntimeFeatures::enableDeviceOrientation(bool enable)
{
    RuntimeEnabledFeatures::setDeviceOrientationEnabled(enable);
}

bool WebRuntimeFeatures::isDeviceOrientationEnabled()
{
    return RuntimeEnabledFeatures::deviceOrientationEnabled();
}

void WebRuntimeFeatures::enableSpeechInput(bool enable)
{
    RuntimeEnabledFeatures::setSpeechInputEnabled(enable);
}

bool WebRuntimeFeatures::isSpeechInputEnabled()
{
    return RuntimeEnabledFeatures::speechInputEnabled();
}

void WebRuntimeFeatures::enableScriptedSpeech(bool enable)
{
#if ENABLE(SCRIPTED_SPEECH)
    RuntimeEnabledFeatures::setScriptedSpeechEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isScriptedSpeechEnabled()
{
#if ENABLE(SCRIPTED_SPEECH)
    return RuntimeEnabledFeatures::scriptedSpeechEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableXHRResponseBlob(bool enable)
{
#if ENABLE(XHR_RESPONSE_BLOB)
    RuntimeEnabledFeatures::setXHRResponseBlobEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isXHRResponseBlobEnabled()
{
#if ENABLE(XHR_RESPONSE_BLOB)
    return RuntimeEnabledFeatures::xhrResponseBlobEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableFileSystem(bool enable)
{
#if ENABLE(FILE_SYSTEM)
    RuntimeEnabledFeatures::setFileSystemEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isFileSystemEnabled()
{
#if ENABLE(FILE_SYSTEM)
    return RuntimeEnabledFeatures::fileSystemEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableJavaScriptI18NAPI(bool enable)
{
#if ENABLE(JAVASCRIPT_I18N_API)
    RuntimeEnabledFeatures::setJavaScriptI18NAPIEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isJavaScriptI18NAPIEnabled()
{
#if ENABLE(JAVASCRIPT_I18N_API)
    return RuntimeEnabledFeatures::javaScriptI18NAPIEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableQuota(bool enable)
{
#if ENABLE(QUOTA)
    RuntimeEnabledFeatures::setQuotaEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isQuotaEnabled()
{
#if ENABLE(QUOTA)
    return RuntimeEnabledFeatures::quotaEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableMediaStream(bool enable)
{
#if ENABLE(MEDIA_STREAM)
    RuntimeEnabledFeatures::setMediaStreamEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isMediaStreamEnabled()
{
#if ENABLE(MEDIA_STREAM)
    return RuntimeEnabledFeatures::mediaStreamEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enablePeerConnection(bool enable)
{
#if ENABLE(MEDIA_STREAM)
    RuntimeEnabledFeatures::setPeerConnectionEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isPeerConnectionEnabled()
{
#if ENABLE(MEDIA_STREAM)
    return RuntimeEnabledFeatures::peerConnectionEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableFullScreenAPI(bool enable)
{
#if ENABLE(FULLSCREEN_API)
    RuntimeEnabledFeatures::setWebkitFullScreenAPIEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isFullScreenAPIEnabled()
{
#if ENABLE(FULLSCREEN_API)
    return RuntimeEnabledFeatures::webkitFullScreenAPIEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enablePointerLock(bool enable)
{
#if ENABLE(POINTER_LOCK)
    RuntimeEnabledFeatures::setWebkitPointerLockEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isPointerLockEnabled()
{
#if ENABLE(POINTER_LOCK)
    return RuntimeEnabledFeatures::webkitPointerLockEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableMediaSource(bool enable)
{
#if ENABLE(MEDIA_SOURCE)
    RuntimeEnabledFeatures::setMediaSourceEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isMediaSourceEnabled()
{
#if ENABLE(MEDIA_SOURCE)
    return RuntimeEnabledFeatures::mediaSourceEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableEncryptedMedia(bool enable)
{
#if ENABLE(ENCRYPTED_MEDIA)
    RuntimeEnabledFeatures::setEncryptedMediaEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isEncryptedMediaEnabled()
{
#if ENABLE(ENCRYPTED_MEDIA)
    return RuntimeEnabledFeatures::encryptedMediaEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableVideoTrack(bool enable)
{
#if ENABLE(VIDEO_TRACK)
    RuntimeEnabledFeatures::setWebkitVideoTrackEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isVideoTrackEnabled()
{
#if ENABLE(VIDEO_TRACK)
    return RuntimeEnabledFeatures::webkitVideoTrackEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableGamepad(bool enable)
{
#if ENABLE(GAMEPAD)
    RuntimeEnabledFeatures::setWebkitGamepadsEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isGamepadEnabled()
{
#if ENABLE(GAMEPAD)
    return RuntimeEnabledFeatures::webkitGamepadsEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableShadowDOM(bool enable)
{
#if ENABLE(SHADOW_DOM)
    RuntimeEnabledFeatures::setShadowDOMEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isShadowDOMEnabled()
{
#if ENABLE(SHADOW_DOM)
    return RuntimeEnabledFeatures::shadowDOMEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableStyleScoped(bool enable)
{
#if ENABLE(STYLE_SCOPED)
    RuntimeEnabledFeatures::setStyleScopedEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isStyleScopedEnabled()
{
#if ENABLE(STYLE_SCOPED)
    return RuntimeEnabledFeatures::styleScopedEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableInputTypeDate(bool enable)
{
#if ENABLE(INPUT_TYPE_DATE)
    RuntimeEnabledFeatures::setInputTypeDateEnabled(enable);
#else
    UNUSED_PARAM(enable);
#endif
}

bool WebRuntimeFeatures::isInputTypeDateEnabled()
{
#if ENABLE(INPUT_TYPE_DATE)
    return RuntimeEnabledFeatures::inputTypeDateEnabled();
#else
    return false;
#endif
}

} // namespace WebKit
