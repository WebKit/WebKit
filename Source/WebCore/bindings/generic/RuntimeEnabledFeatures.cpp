/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "RuntimeEnabledFeatures.h"

#include "DatabaseManager.h"
#include "MediaPlayer.h"
#include "SharedWorkerRepository.h"
#include "WebSocket.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

RuntimeEnabledFeatures::RuntimeEnabledFeatures()
    : m_isLocalStorageEnabled(true)
    , m_isSessionStorageEnabled(true)
    , m_isWebkitNotificationsEnabled(false)
    , m_isApplicationCacheEnabled(true)
    , m_isDataTransferItemsEnabled(true)
    , m_isGeolocationEnabled(true)
    , m_isIndexedDBEnabled(false)
    , m_isTouchEnabled(true)
    , m_isDeviceMotionEnabled(true)
    , m_isDeviceOrientationEnabled(true)
    , m_isSpeechInputEnabled(true)
    , m_isCSSExclusionsEnabled(true)
    , m_isCSSShapesEnabled(true)
    , m_isCSSRegionsEnabled(false)
    , m_isCSSCompositingEnabled(false)
    , m_isLangAttributeAwareFormControlUIEnabled(false)
#if ENABLE(SCRIPTED_SPEECH)
    , m_isScriptedSpeechEnabled(false)
#endif
#if ENABLE(MEDIA_STREAM)
    , m_isMediaStreamEnabled(true)
    , m_isPeerConnectionEnabled(true)
#endif
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    , m_isLegacyCSSVendorPrefixesEnabled(false)
#endif
#if ENABLE(JAVASCRIPT_I18N_API)
    , m_isJavaScriptI18NAPIEnabled(false)
#endif
#if ENABLE(VIDEO_TRACK)
    , m_isVideoTrackEnabled(true)
#endif
#if ENABLE(INPUT_TYPE_DATE)
    , m_isInputTypeDateEnabled(true)
#endif
#if ENABLE(INPUT_TYPE_DATETIME_INCOMPLETE)
    , m_isInputTypeDateTimeEnabled(false)
#endif
#if ENABLE(INPUT_TYPE_DATETIMELOCAL)
    , m_isInputTypeDateTimeLocalEnabled(true)
#endif
#if ENABLE(INPUT_TYPE_MONTH)
    , m_isInputTypeMonthEnabled(true)
#endif
#if ENABLE(INPUT_TYPE_TIME)
    , m_isInputTypeTimeEnabled(true)
#endif
#if ENABLE(INPUT_TYPE_WEEK)
    , m_isInputTypeWeekEnabled(true)
#endif
#if ENABLE(CSP_NEXT)
    , m_areExperimentalContentSecurityPolicyFeaturesEnabled(false)
#endif
#if ENABLE(FONT_LOAD_EVENTS)
    , m_isFontLoadEventsEnabled(false)
#endif
{
}

RuntimeEnabledFeatures& RuntimeEnabledFeatures::sharedFeatures()
{
    static NeverDestroyed<RuntimeEnabledFeatures> runtimeEnabledFeatures;

    return runtimeEnabledFeatures;
}

#if ENABLE(JAVASCRIPT_I18N_API)
bool RuntimeEnabledFeatures::javaScriptI18NAPIEnabled()
{
    return m_isJavaScriptI18NAPIEnabled;
}
#endif

#if ENABLE(VIDEO)
bool RuntimeEnabledFeatures::audioEnabled() const
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::htmlMediaElementEnabled() const
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::htmlAudioElementEnabled() const
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::htmlVideoElementEnabled() const
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::htmlSourceElementEnabled() const
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::mediaControllerEnabled() const
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::mediaErrorEnabled() const
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::timeRangesEnabled() const
{
    return MediaPlayer::isAvailable();
}
#endif

#if ENABLE(SHARED_WORKERS)
bool RuntimeEnabledFeatures::sharedWorkerEnabled() const
{
    return SharedWorkerRepository::isAvailable();
}
#endif

#if ENABLE(WEB_SOCKETS)
bool RuntimeEnabledFeatures::webSocketEnabled() const
{
    return WebSocket::isAvailable();
}
#endif

} // namespace WebCore
