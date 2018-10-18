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

#pragma once

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#include "MediaPlaybackTargetContext.h"
#include "MediaPlaybackTargetPicker.h"
#include "MediaPlaybackTargetPickerMock.h"
#include "MediaProducer.h"
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>

namespace WebCore {

struct ClientState;
class IntRect;
class WebMediaSessionManagerClient;

class WebMediaSessionManager : public MediaPlaybackTargetPicker::Client {
    WTF_MAKE_NONCOPYABLE(WebMediaSessionManager);
public:

    WEBCORE_EXPORT static WebMediaSessionManager& shared();

    WEBCORE_EXPORT void setMockMediaPlaybackTargetPickerEnabled(bool);
    WEBCORE_EXPORT void setMockMediaPlaybackTargetPickerState(const String&, MediaPlaybackTargetContext::State);

    WEBCORE_EXPORT uint64_t addPlaybackTargetPickerClient(WebMediaSessionManagerClient&, uint64_t);
    WEBCORE_EXPORT void removePlaybackTargetPickerClient(WebMediaSessionManagerClient&, uint64_t);
    WEBCORE_EXPORT void removeAllPlaybackTargetPickerClients(WebMediaSessionManagerClient&);
    WEBCORE_EXPORT void showPlaybackTargetPicker(WebMediaSessionManagerClient&, uint64_t, const IntRect&, bool, bool);
    WEBCORE_EXPORT void clientStateDidChange(WebMediaSessionManagerClient&, uint64_t, WebCore::MediaProducer::MediaStateFlags);

protected:
    WebMediaSessionManager();
    virtual ~WebMediaSessionManager();

    virtual WebCore::MediaPlaybackTargetPicker& platformPicker() = 0;
    static WebMediaSessionManager& platformManager();

private:

    WebCore::MediaPlaybackTargetPicker& targetPicker();
    WebCore::MediaPlaybackTargetPickerMock& mockPicker();

    // MediaPlaybackTargetPicker::Client
    void setPlaybackTarget(Ref<WebCore::MediaPlaybackTarget>&&) override;
    void externalOutputDeviceAvailableDidChange(bool) override;

    size_t find(WebMediaSessionManagerClient*, uint64_t);

    void configurePlaybackTargetClients();
    void configureNewClients();
    void configurePlaybackTargetMonitoring();
    void configureWatchdogTimer();

    enum ConfigurationTaskFlags {
        NoTask = 0,
        InitialConfigurationTask = 1 << 0,
        TargetClientsConfigurationTask = 1 << 1,
        TargetMonitoringConfigurationTask = 1 << 2,
        WatchdogTimerConfigurationTask = 1 << 3,
    };
    typedef unsigned ConfigurationTasks;
    String toString(ConfigurationTasks);

    void scheduleDelayedTask(ConfigurationTasks);
    void taskTimerFired();

    void watchdogTimerFired();

    RunLoop::Timer<WebMediaSessionManager> m_taskTimer;
    RunLoop::Timer<WebMediaSessionManager> m_watchdogTimer;

    Vector<std::unique_ptr<ClientState>> m_clientState;
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
    std::unique_ptr<WebCore::MediaPlaybackTargetPickerMock> m_pickerOverride;
    ConfigurationTasks m_taskFlags { NoTask };
    Seconds m_currentWatchdogInterval;
    bool m_externalOutputDeviceAvailable { false };
    bool m_targetChanged { false };
    bool m_mockPickerEnabled { false };
};

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
