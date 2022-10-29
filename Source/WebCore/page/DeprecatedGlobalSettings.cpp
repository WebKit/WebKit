/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DeprecatedGlobalSettings.h"

#include "AudioSession.h"
#include "HTMLMediaElement.h"
#include "MediaPlayer.h"
#include "PlatformMediaSessionManager.h"
#include "PlatformScreen.h"
#include <JavaScriptCore/Options.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(COCOA)
#include "MediaSessionManagerCocoa.h"
#endif

namespace WebCore {

DeprecatedGlobalSettings::DeprecatedGlobalSettings()
{
#if PLATFORM(WATCHOS)
    m_isWebSocketEnabled = false;
#endif
#if USE(AVFOUNDATION)
    m_AVFoundationEnabled = true;
#endif

#if USE(GSTREAMER)
    m_GStreamerEnabled = true;
#endif
#if PLATFORM(WIN)
    m_shouldUseHighResolutionTimers = true;
#endif
#if PLATFORM(IOS_FAMILY)
    m_networkDataUsageTrackingEnabled = false;
    m_shouldOptOutOfNetworkStateObservation = false;
    m_disableScreenSizeOverride = false;
#endif

    m_mockScrollbarsEnabled = false;
    m_usesOverlayScrollbars = false;
    m_lowPowerVideoAudioBufferSizeEnabled = false;
    m_trackingPreventionEnabled = false;
    m_allowsAnySSLCertificate = false;
}

DeprecatedGlobalSettings::~DeprecatedGlobalSettings() = default;

DeprecatedGlobalSettings& DeprecatedGlobalSettings::shared()
{
    static NeverDestroyed<DeprecatedGlobalSettings> deprecatedGlobalSettings;
    return deprecatedGlobalSettings;
}

#if ENABLE(TOUCH_EVENTS)
bool DeprecatedGlobalSettings::touchEventsEnabled()
{
    return shared().m_touchEventsEnabled.value_or(screenHasTouchDevice());
}
#endif

#if ENABLE(VORBIS)
void DeprecatedGlobalSettings::setVorbisDecoderEnabled(bool isEnabled)
{
    shared().m_vorbisDecoderEnabled = isEnabled;
    PlatformMediaSessionManager::setVorbisDecoderEnabled(isEnabled);
}
#endif

#if ENABLE(OPUS)
void DeprecatedGlobalSettings::setOpusDecoderEnabled(bool isEnabled)
{
    shared().m_opusDecoderEnabled = isEnabled;
    PlatformMediaSessionManager::setOpusDecoderEnabled(isEnabled);
}
#endif

#if ENABLE(MEDIA_SOURCE) && (HAVE(AVSAMPLEBUFFERVIDEOOUTPUT) || USE(GSTREAMER))
void DeprecatedGlobalSettings::setMediaSourceInlinePaintingEnabled(bool isEnabled)
{
    shared().m_mediaSourceInlinePaintingEnabled = isEnabled;
#if HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
    MediaSessionManagerCocoa::setMediaSourceInlinePaintingEnabled(isEnabled);
#endif
}
#endif

#if HAVE(AVCONTENTKEYSPECIFIER)
void DeprecatedGlobalSettings::setSampleBufferContentKeySessionSupportEnabled(bool enabled)
{
    shared().m_sampleBufferContentKeySessionSupportEnabled = enabled;
    MediaSessionManagerCocoa::setSampleBufferContentKeySessionSupportEnabled(enabled);
}
#endif


#if PLATFORM(WIN)
void DeprecatedGlobalSettings::setShouldUseHighResolutionTimers(bool shouldUseHighResolutionTimers)
{
    shared().m_shouldUseHighResolutionTimers = shouldUseHighResolutionTimers;
}
#endif

#if USE(AVFOUNDATION)
void DeprecatedGlobalSettings::setAVFoundationEnabled(bool enabled)
{
    if (shared().m_AVFoundationEnabled == enabled)
        return;

    shared().m_AVFoundationEnabled = enabled;
    HTMLMediaElement::resetMediaEngines();
}
#endif

#if USE(GSTREAMER)
void DeprecatedGlobalSettings::setGStreamerEnabled(bool enabled)
{
    if (shared().m_GStreamerEnabled == enabled)
        return;

    shared().m_GStreamerEnabled = enabled;

#if ENABLE(VIDEO)
    HTMLMediaElement::resetMediaEngines();
#endif
}
#endif

// It's very important that this setting doesn't change in the middle of a document's lifetime.
// The Mac port uses this flag when registering and deregistering platform-dependent scrollbar
// objects. Therefore, if this changes at an unexpected time, deregistration may not happen
// correctly, which may cause the platform to follow dangling pointers.
void DeprecatedGlobalSettings::setMockScrollbarsEnabled(bool flag)
{
    shared().m_mockScrollbarsEnabled = flag;
    // FIXME: This should update scroll bars in existing pages.
}

bool DeprecatedGlobalSettings::mockScrollbarsEnabled()
{
    return shared().m_mockScrollbarsEnabled;
}

void DeprecatedGlobalSettings::setUsesOverlayScrollbars(bool flag)
{
    shared().m_usesOverlayScrollbars = flag;
    // FIXME: This should update scroll bars in existing pages.
}

bool DeprecatedGlobalSettings::usesOverlayScrollbars()
{
    return shared().m_usesOverlayScrollbars;
}

void DeprecatedGlobalSettings::setLowPowerVideoAudioBufferSizeEnabled(bool flag)
{
    shared().m_lowPowerVideoAudioBufferSizeEnabled = flag;
}

void DeprecatedGlobalSettings::setTrackingPreventionEnabled(bool flag)
{
    shared().m_trackingPreventionEnabled = flag;
}

#if PLATFORM(IOS_FAMILY)
void DeprecatedGlobalSettings::setAudioSessionCategoryOverride(unsigned sessionCategory)
{
    AudioSession::sharedSession().setCategoryOverride(static_cast<AudioSession::CategoryType>(sessionCategory));
}

unsigned DeprecatedGlobalSettings::audioSessionCategoryOverride()
{
    return static_cast<unsigned>(AudioSession::sharedSession().categoryOverride());
}

void DeprecatedGlobalSettings::setNetworkDataUsageTrackingEnabled(bool trackingEnabled)
{
    shared().m_networkDataUsageTrackingEnabled = trackingEnabled;
}

bool DeprecatedGlobalSettings::networkDataUsageTrackingEnabled()
{
    return shared().m_networkDataUsageTrackingEnabled;
}

void DeprecatedGlobalSettings::setNetworkInterfaceName(const String& networkInterfaceName)
{
    shared().m_networkInterfaceName = networkInterfaceName;
}

const String& DeprecatedGlobalSettings::networkInterfaceName()
{
    return shared().m_networkInterfaceName;
}
#endif

#if USE(AUDIO_SESSION)
void DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(bool flag)
{
    AudioSession::setShouldManageAudioSessionCategory(flag);
}

bool DeprecatedGlobalSettings::shouldManageAudioSessionCategory()
{
    return AudioSession::shouldManageAudioSessionCategory();
}
#endif

void DeprecatedGlobalSettings::setAllowsAnySSLCertificate(bool allowAnySSLCertificate)
{
    shared().m_allowsAnySSLCertificate = allowAnySSLCertificate;
}

bool DeprecatedGlobalSettings::allowsAnySSLCertificate()
{
    return shared().m_allowsAnySSLCertificate;
}

} // namespace WebCore
