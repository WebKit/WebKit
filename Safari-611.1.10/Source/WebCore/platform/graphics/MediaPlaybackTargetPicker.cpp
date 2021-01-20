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
#include "MediaPlaybackTargetPicker.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#include "Logging.h"
#include "MediaPlaybackTarget.h"

namespace WebCore {

static const Seconds pendingActionInterval { 100_ms };

MediaPlaybackTargetPicker::MediaPlaybackTargetPicker(Client& client)
    : m_client(&client)
    , m_pendingActionTimer(RunLoop::main(), this, &MediaPlaybackTargetPicker::pendingActionTimerFired)
{
}

MediaPlaybackTargetPicker::~MediaPlaybackTargetPicker()
{
    m_pendingActionTimer.stop();
    m_client = nullptr;
}

void MediaPlaybackTargetPicker::pendingActionTimerFired()
{
    LOG(Media, "MediaPlaybackTargetPicker::pendingActionTimerFired - flags = 0x%x", m_pendingActionFlags);

    PendingActionFlags pendingActions = m_pendingActionFlags;
    m_pendingActionFlags = 0;

    if (pendingActions & CurrentDeviceDidChange)
        m_client->setPlaybackTarget(playbackTarget());

    if (pendingActions & OutputDeviceAvailabilityChanged)
        m_client->externalOutputDeviceAvailableDidChange(externalOutputDeviceAvailable());

    if (pendingActions & PlaybackTargetPickerWasDismissed)
        m_client->playbackTargetPickerWasDismissed();
}

void MediaPlaybackTargetPicker::addPendingAction(PendingActionFlags action)
{
    if (!m_client)
        return;

    m_pendingActionFlags |= action;
    m_pendingActionTimer.startOneShot(pendingActionInterval);
}

void MediaPlaybackTargetPicker::showPlaybackTargetPicker(PlatformView*, const FloatRect&, bool, bool)
{
    ASSERT_NOT_REACHED();
}

void MediaPlaybackTargetPicker::startingMonitoringPlaybackTargets()
{
    ASSERT_NOT_REACHED();
}

void MediaPlaybackTargetPicker::stopMonitoringPlaybackTargets()
{
    ASSERT_NOT_REACHED();
}

void MediaPlaybackTargetPicker::invalidatePlaybackTargets()
{
    ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
