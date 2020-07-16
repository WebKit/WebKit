/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaPlaybackTargetPickerMock.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#include "FloatRect.h"
#include "Logging.h"
#include "MediaPlaybackTargetMock.h"
#include "WebMediaSessionManager.h"

namespace WebCore {

static const Seconds timerInterval { 100_ms };

MediaPlaybackTargetPickerMock::MediaPlaybackTargetPickerMock(MediaPlaybackTargetPicker::Client& client)
    : MediaPlaybackTargetPicker(client)
{
    LOG(Media, "MediaPlaybackTargetPickerMock::MediaPlaybackTargetPickerMock");
}

MediaPlaybackTargetPickerMock::~MediaPlaybackTargetPickerMock()
{
    LOG(Media, "MediaPlaybackTargetPickerMock::~MediaPlaybackTargetPickerMock");
    setClient(nullptr);
}

bool MediaPlaybackTargetPickerMock::externalOutputDeviceAvailable()
{
    LOG(Media, "MediaPlaybackTargetPickerMock::externalOutputDeviceAvailable");
    return m_state == MediaPlaybackTargetContext::OutputDeviceAvailable;
}

Ref<MediaPlaybackTarget> MediaPlaybackTargetPickerMock::playbackTarget()
{
    LOG(Media, "MediaPlaybackTargetPickerMock::playbackTarget");
    return WebCore::MediaPlaybackTargetMock::create(m_deviceName, m_state);
}

void MediaPlaybackTargetPickerMock::showPlaybackTargetPicker(PlatformView*, const FloatRect&, bool checkActiveRoute, bool useDarkAppearance)
{
    if (!client() || m_showingMenu)
        return;

#if LOG_DISABLED
    UNUSED_PARAM(checkActiveRoute);
    UNUSED_PARAM(useDarkAppearance);
#endif

    LOG(Media, "MediaPlaybackTargetPickerMock::showPlaybackTargetPicker - checkActiveRoute = %i, useDarkAppearance = %i", (int)checkActiveRoute, (int)useDarkAppearance);

    m_showingMenu = true;
    m_taskQueue.enqueueTask([this] {
        m_showingMenu = false;
        currentDeviceDidChange();
    });
}

void MediaPlaybackTargetPickerMock::startingMonitoringPlaybackTargets()
{
    LOG(Media, "MediaPlaybackTargetPickerMock::startingMonitoringPlaybackTargets");

    m_taskQueue.enqueueTask([this] {
        if (m_state == MediaPlaybackTargetContext::OutputDeviceAvailable)
            availableDevicesDidChange();

        if (!m_deviceName.isEmpty() && m_state != MediaPlaybackTargetContext::Unknown)
            currentDeviceDidChange();
    });
}

void MediaPlaybackTargetPickerMock::stopMonitoringPlaybackTargets()
{
    LOG(Media, "MediaPlaybackTargetPickerMock::stopMonitoringPlaybackTargets");
}

void MediaPlaybackTargetPickerMock::invalidatePlaybackTargets()
{
    LOG(Media, "MediaPlaybackTargetPickerMock::invalidatePlaybackTargets");
    setState(emptyString(), MediaPlaybackTargetContext::Unknown);
}

void MediaPlaybackTargetPickerMock::setState(const String& deviceName, MediaPlaybackTargetContext::State state)
{
    LOG(Media, "MediaPlaybackTargetPickerMock::setState - name = %s, state = 0x%x", deviceName.utf8().data(), (unsigned)state);

    m_taskQueue.enqueueTask([this, state, deviceName] {
        if (deviceName != m_deviceName && state != MediaPlaybackTargetContext::Unknown) {
            m_deviceName = deviceName;
            currentDeviceDidChange();
        }

        if (m_state != state) {
            m_state = state;
            availableDevicesDidChange();
        }
    });
}

void MediaPlaybackTargetPickerMock::dismissPopup()
{
    if (!m_showingMenu)
        return;

    m_showingMenu = false;
    currentDeviceDidChange();
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
